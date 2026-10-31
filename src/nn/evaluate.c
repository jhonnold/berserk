// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "evaluate.h"
#include "threats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bits.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../thread.h"
#include "../util.h"
#include "accumulator.h"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "../incbin.h"

INCBIN(Embed, EVALFILE);

#define FT_MAX     255
#define FT_SHIFT   9
#define EVAL_SCALE 160

#define N_L1_ACT    (2 * N_L2)
#define N_L3_IN     N_L3
#define N_L2_CHUNKS (N_L1_ACT / 4)

#define HEAD_ONE (127 * 64)

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_OUTPUT_BUCKETS][N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_OUTPUT_BUCKETS][N_L2] ALIGN;

int8_t L2_WEIGHTS[N_OUTPUT_BUCKETS][N_L2_CHUNKS * N_L3 * 4] ALIGN;
int32_t L2_BIASES[N_OUTPUT_BUCKETS][N_L3] ALIGN;

int8_t L3_WEIGHTS[N_OUTPUT_BUCKETS][N_L3_IN] ALIGN;
int32_t L3_BIASES[N_OUTPUT_BUCKETS];

uint16_t LOOKUP_INDICES[256][8] ALIGN;

#if defined(SIMD) && !(defined(__AVX512F__) && defined(__AVX512BW__))
INLINE size_t StoreNNZ(uint16_t* dest, size_t count, nnz_t* base, const nnz_t increment, const uint32_t lookup) {
  const nnz_t offsets = nnz_load(LOOKUP_INDICES[lookup]);
  nnz_store(dest + count, nnz_add(*base, offsets));
  *base = nnz_add(*base, increment);
  return count + BitCount(lookup);
}
#endif

#if defined(SIMD)
INLINE vu8_t Pairwise(vi16_t a0, vi16_t a1, vi16_t b0, vi16_t b1) {
  const vi16_t zero = vi16_zero();
  const vi16_t cap  = vi16_set1(FT_MAX);

  a0 = vi16_slli(vi16_min(vi16_max(a0, zero), cap), MULHI_SHIFT - FT_SHIFT);
  a1 = vi16_slli(vi16_min(vi16_max(a1, zero), cap), MULHI_SHIFT - FT_SHIFT);
  b0 = vi16_min(b0, cap);
  b1 = vi16_min(b1, cap);

  return vi16_packus(vi16_mulhi(a0, b0), vi16_mulhi(a1, b1));
}
#endif

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
#if defined(SIMD)
  const size_t WIDTH = sizeof(vi16_t) / sizeof(acc_t);
  const size_t HALF  = N_HIDDEN / 2 / WIDTH;
  const int views[2] = {stm, !stm};

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VBMI2__)
  const __m512i increment = _mm512_set1_epi16(32);
  __m512i base            = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, //
                                             15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
#elif defined(__AVX512F__) && defined(__AVX512BW__)
  const __m512i increment = _mm512_set1_epi32(16);
  __m512i base            = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
#else
  const nnz_t increment = nnz_set1(8);
  nnz_t base            = nnz_zero();
#endif
  size_t count = 0;

  for (int v = 0; v < 2; v++) {
    const vi16_t* in = (vi16_t*) acc->values[views[v]];
    vu8_t* out      = (vu8_t*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const vu8_t o0 = Pairwise(in[i + 0], in[i + 1], in[i + 0 + HALF], in[i + 1 + HALF]);
      const vu8_t o1 = Pairwise(in[i + 2], in[i + 3], in[i + 2 + HALF], in[i + 3 + HALF]);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

#if defined(__AVX512F__) && defined(__AVX512BW__)
      const uint32_t m0 = vi32_nnz_mask(o0);
      const uint32_t m1 = vi32_nnz_mask(o1);

#if defined(__AVX512VBMI2__)
      const uint32_t m = m0 | (m1 << 16);
      _mm512_storeu_si512((__m512i*) (nnz + count), _mm512_maskz_compress_epi16(m, base));
      count += BitCount(m);
      base = _mm512_add_epi16(base, increment);
#else
      _mm256_storeu_si256((__m256i*) (nnz + count), _mm512_cvtepi32_epi16(_mm512_maskz_compress_epi32(m0, base)));
      count += BitCount(m0);
      base = _mm512_add_epi32(base, increment);

      _mm256_storeu_si256((__m256i*) (nnz + count), _mm512_cvtepi32_epi16(_mm512_maskz_compress_epi32(m1, base)));
      count += BitCount(m1);
      base = _mm512_add_epi32(base, increment);
#endif
#elif defined(__AVX2__)
      count = StoreNNZ(nnz, count, &base, increment, vi32_nnz_mask(o0));
      count = StoreNNZ(nnz, count, &base, increment, vi32_nnz_mask(o1));
#else
      count = StoreNNZ(nnz, count, &base, increment, vi32_nnz_mask2(o0, o1));
#endif
    }
  }

  return count;
#else
  const int views[2] = {stm, !stm};

  (void) nnz;

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    uint8_t* out    = &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < N_HIDDEN / 2; i++) {
      const int lo = Min(FT_MAX, Max(0, in[i]));
      const int hi = Min(FT_MAX, (int) in[i + N_HIDDEN / 2]);

      out[i] = Min(FT_MAX, Max(0, (lo * hi) >> FT_SHIFT));
    }
  }

  return 0;
#endif
}

#if defined(SIMD)
INLINE void L1Chunk(vi32_t* regs, const int32_t* in32, const uint16_t i0, const int8_t* weights) {
  const size_t OUT_CC = N_L2 / (sizeof(vi32_t) / sizeof(int32_t));

  const vi8_t f0  = vi8_bcast32(&in32[i0]);
  const vi8_t* c0 = (vi8_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

  for (size_t j = 0; j < OUT_CC; j++)
    vi32_dpbusd(regs + j, f0, c0[j]);
}
#endif

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
#if defined(SIMD)
  const size_t OUT_WIDTH = sizeof(vi32_t) / sizeof(int32_t);
  const size_t OUT_CC    = N_L2 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  vi32_t* out        = (vi32_t*) dest;

  size_t i = 0;
#if L1_UNROLL == 4
  // Two sets of sums so consecutive chunks don't wait on each other
  vi32_t regs[OUT_CC], alts[OUT_CC];
  for (i = 0; i < OUT_CC; i++) {
    regs[i] = vi32_load(biases + i * OUT_WIDTH);
    alts[i] = vi32_zero();
  }

  for (i = 0; i + 3 < count; i += 4) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];
    const uint16_t i2 = nnz[i + 2];
    const uint16_t i3 = nnz[i + 3];

    const vi8_t f0 = vi8_bcast32(&in32[i0]);
    const vi8_t f1 = vi8_bcast32(&in32[i1]);
    const vi8_t f2 = vi8_bcast32(&in32[i2]);
    const vi8_t f3 = vi8_bcast32(&in32[i3]);

    const vi8_t* c0 = (vi8_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const vi8_t* c1 = (vi8_t*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];
    const vi8_t* c2 = (vi8_t*) &weights[i2 * N_L2 * SPARSE_CHUNK_SIZE];
    const vi8_t* c3 = (vi8_t*) &weights[i3 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++) {
      vi32_dpbusd_x2(regs + j, f0, c0[j], f1, c1[j]);
      vi32_dpbusd_x2(alts + j, f2, c2[j], f3, c3[j]);
    }
  }

  for (; i < count; i++)
    L1Chunk(regs, in32, nnz[i], weights);

  for (i = 0; i < OUT_CC; i++)
    out[i] = vi32_add(regs[i], alts[i]);
#else
  vi32_t regs[OUT_CC];
  for (i = 0; i < OUT_CC; i++)
    regs[i] = vi32_load(biases + i * OUT_WIDTH);

  for (i = 0; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const vi8_t f0 = vi8_bcast32(&in32[i0]);
    const vi8_t f1 = vi8_bcast32(&in32[i1]);

    const vi8_t* c0 = (vi8_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const vi8_t* c1 = (vi8_t*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      vi32_dpbusd_x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count)
    L1Chunk(regs, in32, nnz[i], weights);

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
#endif
#else
  (void) nnz;
  (void) count;

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = biases[i];

  for (size_t i = 0; i < N_L1; i++) {
    if (!src[i])
      continue;

    for (size_t j = 0; j < N_L2; j++)
      dest[j] += src[i] * weights[j * N_L1 + i];
  }
#endif
}

#if !defined(SIMD)
INLINE int32_t Sat16(const int32_t x) {
  return x < -32768 ? -32768 : x > 32767 ? 32767 : x;
}

INLINE uint8_t SquareQ(const int32_t x) {
  const int32_t s = Sat16(x);
  const int32_t q = ((s * s) >> 16) >> 3;
  return q > 127 ? 127 : q;
}
#endif

INLINE void L1Activate(uint8_t* clamped, uint8_t* squared, int32_t* src) {
#if defined(SIMD)
  const v128_32_t* in = (v128_32_t*) src;
  v128_8_t* outC      = (v128_8_t*) clamped;
  v128_8_t* outS      = (v128_8_t*) squared;

  for (size_t i = 0; i < N_L2 / 16; i++) {
    const v128_16_t a0 = v128_packs32(in[4 * i + 0], in[4 * i + 1]);
    const v128_16_t a1 = v128_packs32(in[4 * i + 2], in[4 * i + 3]);

    outC[i] = v128_clamp(a0, a1);
    outS[i] = v128_squareq(a0, a1);
  }
#else
  for (size_t i = 0; i < N_L2; i++) {
    const int32_t c = src[i] >> 6;
    clamped[i]      = c < 0 ? 0 : c > 127 ? 127 : c;
    squared[i]      = SquareQ(src[i]);
  }
#endif
}

INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
#if defined(SIMD)
  const size_t OUT_WIDTH = sizeof(vi32_t) / sizeof(int32_t);
  const size_t OUT_CC    = N_L3 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const vi8_t* w     = (vi8_t*) weights;
  vi32_t* out        = (vi32_t*) dest;

  vi32_t regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = vi32_load(biases + i * OUT_WIDTH);

  for (size_t j = 0; j < N_L2_CHUNKS; j += 2) {
    const vi8_t f0  = vi8_bcast32(&in32[j + 0]);
    const vi8_t f1  = vi8_bcast32(&in32[j + 1]);
    const vi8_t* c0 = w + (j + 0) * OUT_CC;
    const vi8_t* c1 = w + (j + 1) * OUT_CC;

    for (size_t i = 0; i < OUT_CC; i++)
      vi32_dpbusd_x2(regs + i, f0, c0[i], f1, c1[i]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
#else
  for (size_t o = 0; o < N_L3; o++)
    dest[o] = biases[o];

  for (size_t i = 0; i < N_L1_ACT; i++)
    for (size_t o = 0; o < N_L3; o++)
      dest[o] += src[i] * weights[(i / 4) * N_L3 * 4 + o * 4 + (i % 4)];
#endif
}

INLINE void L2Activate(uint8_t* dest, int32_t* src) {
#if defined(SIMD)
  const v128_32_t* in = (v128_32_t*) src;
  v128_8_t* out       = (v128_8_t*) dest;

  for (size_t i = 0; i < N_L3 / 16; i++) {
    v128_16_t a0 = v128_packs32(in[4 * i + 0], in[4 * i + 1]);
    v128_16_t a1 = v128_packs32(in[4 * i + 2], in[4 * i + 3]);
    a0           = v128_max16(a0, v128_zero16());
    a1           = v128_max16(a1, v128_zero16());

    out[i] = v128_squareq(a0, a1);
  }
#else
  for (size_t i = 0; i < N_L3; i++)
    dest[i] = SquareQ(src[i] < 0 ? 0 : src[i]);
#endif
}

INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
#if defined(SIMD)
  vi32_t a0 = vi32_zero();

#if defined(__AVX512F__) && defined(__AVX512BW__)
  // the 32 inputs fill half a register
  const __m512i in = _mm512_zextsi256_si512(*(__m256i*) src);
  const __m512i w  = _mm512_zextsi256_si512(*(__m256i*) weights);
  vi32_dpbusd(&a0, in, w);
#elif defined(__AVX2__)
  vi32_dpbusd(&a0, *(vi8_t*) src, *(vi8_t*) weights);
#else
  const vi8_t* in = (vi8_t*) src;
  const vi8_t* w  = (vi8_t*) weights;
  vi32_dpbusd_x2(&a0, in[0], w[0], in[1], w[1]);
#endif

  return vi32_reduce(a0);
#else
  int32_t result = 0;

  for (size_t i = 0; i < N_L3_IN; i++)
    result += src[i] * weights[i];

  return result;
#endif
}

INLINE int PropagateView(Accumulator* accumulator, const int stm, const int bucket) {
  uint8_t x0[N_L1] ALIGN;
  // The index list is written in whole groups of up to 32, so leave room for the tail.
  uint16_t nnz[N_L1 / SPARSE_CHUNK_SIZE + 32] ALIGN;
  int32_t dest[N_L3] ALIGN;
  uint8_t act1[N_L1_ACT] ALIGN;
  uint8_t act2[N_L3] ALIGN;

  const size_t count = InputPairwise8(x0, nnz, accumulator, stm);
  L1Affine(dest, x0, nnz, count, L1_WEIGHTS[bucket], L1_BIASES[bucket]);
  L1Activate(act1, act1 + N_L2, dest);
  L2Affine(dest, act1, L2_WEIGHTS[bucket], L2_BIASES[bucket]);
  L2Activate(act2, dest);

  const int32_t out = L3Transform(act2, L3_WEIGHTS[bucket]) + L3_BIASES[bucket];
  return (int) ((int64_t) out * EVAL_SCALE / HEAD_ONE);
}

int OutputBucket(Board* board) {
  return (BitCount(OccBB(BOTH)) - 2) / 4;
}

// Both call sites pass a literal side, so specialising on it lets the pair of
// perspective indices fold away instead of being read back from a local array.
int Propagate(Accumulator* accumulator, const int stm, const int bucket) {
  return stm == WHITE ? PropagateView(accumulator, WHITE, bucket) : PropagateView(accumulator, BLACK, bucket);
}

int Predict(Board* board) {
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  return Propagate(board->accumulators, board->stm, OutputBucket(board));
}

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN +            // input weights
                            sizeof(int16_t) * N_HIDDEN +                         // input biases
                            sizeof(int8_t) * N_THREATS * N_HIDDEN +              // threat weights
                            sizeof(int8_t) * N_OUTPUT_BUCKETS * N_L1 * N_L2 +    // L1 weights
                            sizeof(int32_t) * N_OUTPUT_BUCKETS * N_L2 +          // L1 biases
                            sizeof(int8_t) * N_OUTPUT_BUCKETS * N_L3 * N_L1_ACT +  // L2 weights
                            sizeof(int32_t) * N_OUTPUT_BUCKETS * N_L3 +          // L2 biases
                            sizeof(int8_t) * N_OUTPUT_BUCKETS * N_L3_IN +          // L3 weights
                            sizeof(int32_t) * N_OUTPUT_BUCKETS;                  // L3 biases

#if defined(__SSE4_1__) || defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE int WeightIdxScrambled(int idx) {
  return ((idx / SPARSE_CHUNK_SIZE) % (N_L1 / SPARSE_CHUNK_SIZE) * N_L2 * SPARSE_CHUNK_SIZE) +
         (idx / N_L1 * SPARSE_CHUNK_SIZE) + (idx % SPARSE_CHUNK_SIZE);
}
#endif

// [output][input] becomes [input chunk][output][4], so one broadcast chunk meets every output.
INLINE int L2WeightIdxScrambled(int idx) {
  const int o = idx / N_L1_ACT;
  const int i = idx % N_L1_ACT;

  return (i / 4) * N_L3 * 4 + o * 4 + (i % 4);
}

INLINE void PermuteForPackus(int16_t* data, size_t len) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
  __m512i* v = (__m512i*) data;

  for (size_t i = 0; i < len / 32; i += 2) {
    __m128i a1 = _mm512_extracti32x4_epi32(v[i], 1);
    __m128i a2 = _mm512_extracti32x4_epi32(v[i], 2);
    __m128i a3 = _mm512_extracti32x4_epi32(v[i], 3);
    __m128i b0 = _mm512_extracti32x4_epi32(v[i + 1], 0);
    __m128i b1 = _mm512_extracti32x4_epi32(v[i + 1], 1);
    __m128i b2 = _mm512_extracti32x4_epi32(v[i + 1], 2);

    v[i]     = _mm512_inserti32x4(v[i], a2, 1);
    v[i]     = _mm512_inserti32x4(v[i], b0, 2);
    v[i]     = _mm512_inserti32x4(v[i], b2, 3);
    v[i + 1] = _mm512_inserti32x4(v[i + 1], a1, 0);
    v[i + 1] = _mm512_inserti32x4(v[i + 1], a3, 1);
    v[i + 1] = _mm512_inserti32x4(v[i + 1], b1, 2);
  }
#elif defined(__AVX2__)
  __m256i* v = (__m256i*) data;

  for (size_t i = 0; i < len / 16; i += 2) {
    __m128i a1 = _mm256_extracti128_si256(v[i], 1);
    __m128i b0 = _mm256_extracti128_si256(v[i + 1], 0);

    v[i]     = _mm256_inserti128_si256(v[i], b0, 1);
    v[i + 1] = _mm256_inserti128_si256(v[i + 1], a1, 0);
  }
#else
  (void) data;
  (void) len;
#endif
}

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  // Alloc a chunk of memory for the L1 weights which we
  // cannot copy into the stack directly
  int8_t* l1 = malloc(N_L1 * N_L2 * sizeof(int8_t));

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);
  const int8_t* threats = (const int8_t*) &in[offset];
  offset += N_THREATS * N_HIDDEN * sizeof(int8_t);

  for (int b = 0; b < N_OUTPUT_BUCKETS; b++) {
    memcpy(l1, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
    offset += N_L1 * N_L2 * sizeof(int8_t);

#if defined(__SSE4_1__) || defined(__ARM_NEON__) || defined(__ARM_NEON)
    // Shuffle the L1 weights for sparse matmul
    for (int i = 0; i < N_L1 * N_L2; i++)
      L1_WEIGHTS[b][WeightIdxScrambled(i)] = l1[i];
#else
    memcpy(L1_WEIGHTS[b], l1, N_L1 * N_L2 * sizeof(int8_t));
#endif
  }

  free(l1);

  memcpy(L1_BIASES, &in[offset], sizeof(L1_BIASES));
  offset += sizeof(L1_BIASES);

  for (int b = 0; b < N_OUTPUT_BUCKETS; b++)
    for (int i = 0; i < N_L3 * N_L1_ACT; i++)
      L2_WEIGHTS[b][L2WeightIdxScrambled(i)] = (int8_t) in[offset++];

  memcpy(L2_BIASES, &in[offset], sizeof(L2_BIASES));
  offset += sizeof(L2_BIASES);
  memcpy(L3_WEIGHTS, &in[offset], sizeof(L3_WEIGHTS));
  offset += sizeof(L3_WEIGHTS);
  memcpy(L3_BIASES, &in[offset], sizeof(L3_BIASES));

  PermuteForPackus(INPUT_WEIGHTS, N_FEATURES * N_HIDDEN);
  PermuteForPackus(INPUT_BIASES, N_HIDDEN);

  // Threat rows take the same lane order as the PSQ rows
  int16_t order[N_HIDDEN] ALIGN;
  for (int i = 0; i < N_HIDDEN; i++)
    order[i] = i;
  PermuteForPackus(order, N_HIDDEN);

  for (size_t f = 0; f < N_THREATS; f++)
    for (int i = 0; i < N_HIDDEN; i++)
      THREAT_WEIGHTS[f * N_HIDDEN + i] = threats[f * N_HIDDEN + order[i]];
}

INLINE void InitLookupIndices() {
  for (size_t i = 0; i < 256; i++) {
    uint64_t j = i;
    uint64_t k = 0;
    while (j)
      LOOKUP_INDICES[i][k++] = PopLSB(&j);
  }
}

void LoadDefaultNN() {
  InitLookupIndices();

  // bullet pads a network to 64 bytes; anything else is another format.
  if ((size_t) EmbedSize < NETWORK_SIZE || (size_t) EmbedSize >= NETWORK_SIZE + 64) {
    fprintf(stderr, "embedded network is %u bytes, this engine reads %zu: build with EVALFILE=<network>\n",
            (unsigned) EmbedSize, NETWORK_SIZE);
    exit(1);
  }

  CopyData(EmbedData);
}

int LoadNetwork(char* path) {
  FILE* fin = fopen(path, "rb");
  if (fin == NULL) {
    printf("info string Unable to read file at %s\n", path);
    return 0;
  }

  uint8_t* data = malloc(NETWORK_SIZE + 64);
  const size_t read = fread(data, sizeof(uint8_t), NETWORK_SIZE + 64, fin);
  if (read < NETWORK_SIZE || read >= NETWORK_SIZE + 64) {
    printf("info string Error reading file at %s\n", path);
    return 0;
  }

  CopyData(data);

  for (int i = 0; i < Threads.count; i++)
    ResetRefreshTable(Threads.threads[i]->refreshTable);

  fclose(fin);
  free(data);

  return 1;
}
