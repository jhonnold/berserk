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
#define N_L3_IN     (N_L3 + N_L1_ACT)
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

#ifdef __SSE4_1__
#include <immintrin.h>
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#if defined(__SSE4_1__) && !(defined(__AVX512F__) && defined(__AVX512BW__))
INLINE size_t StoreNNZ(uint16_t* dest, size_t count, __m128i* base, const __m128i increment, const uint32_t lookup) {
  const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
  _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(*base, offsets));
  *base = _mm_add_epi16(*base, increment);
  return count + BitCount(lookup);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE __m512i m512_pairwise_epi16(__m512i a0, __m512i a1, __m512i b0, __m512i b1) {
  const __m512i zero = _mm512_setzero_si512();
  const __m512i cap  = _mm512_set1_epi16(FT_MAX);

  a0 = _mm512_slli_epi16(_mm512_min_epi16(_mm512_max_epi16(a0, zero), cap), 16 - FT_SHIFT);
  a1 = _mm512_slli_epi16(_mm512_min_epi16(_mm512_max_epi16(a1, zero), cap), 16 - FT_SHIFT);
  b0 = _mm512_min_epi16(b0, cap);
  b1 = _mm512_min_epi16(b1, cap);

  return _mm512_packus_epi16(_mm512_mulhi_epi16(a0, b0), _mm512_mulhi_epi16(a1, b1));
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH = sizeof(__m512i) / sizeof(acc_t);
  const size_t HALF  = N_HIDDEN / 2 / WIDTH;
  const int views[2] = {stm, !stm};

  const __m512i zero = _mm512_setzero_si512();
#if defined(__AVX512VBMI2__)
  const __m512i increment = _mm512_set1_epi16(32);
  __m512i base            = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, //
                                             15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
#else
  const __m512i increment = _mm512_set1_epi32(16);
  __m512i base            = _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
#endif
  size_t count = 0;

  for (int v = 0; v < 2; v++) {
    const __m512i* in = (__m512i*) acc->values[views[v]];
    __m512i* out      = (__m512i*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const __m512i o0 = m512_pairwise_epi16(in[i + 0], in[i + 1], in[i + 0 + HALF], in[i + 1 + HALF]);
      const __m512i o1 = m512_pairwise_epi16(in[i + 2], in[i + 3], in[i + 2 + HALF], in[i + 3 + HALF]);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      const uint32_t m0 = _mm512_cmpgt_epi32_mask(o0, zero);
      const uint32_t m1 = _mm512_cmpgt_epi32_mask(o1, zero);

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
    }
  }

  return count;
}
#elif defined(__AVX2__)
INLINE __m256i m256_pairwise_epi16(__m256i a0, __m256i a1, __m256i b0, __m256i b1) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i cap  = _mm256_set1_epi16(FT_MAX);

  a0 = _mm256_slli_epi16(_mm256_min_epi16(_mm256_max_epi16(a0, zero), cap), 16 - FT_SHIFT);
  a1 = _mm256_slli_epi16(_mm256_min_epi16(_mm256_max_epi16(a1, zero), cap), 16 - FT_SHIFT);
  b0 = _mm256_min_epi16(b0, cap);
  b1 = _mm256_min_epi16(b1, cap);

  return _mm256_packus_epi16(_mm256_mulhi_epi16(a0, b0), _mm256_mulhi_epi16(a1, b1));
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH = sizeof(__m256i) / sizeof(acc_t);
  const size_t HALF  = N_HIDDEN / 2 / WIDTH;
  const int views[2] = {stm, !stm};

  const __m256i zero      = _mm256_setzero_si256();
  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const __m256i o0 = m256_pairwise_epi16(in[i + 0], in[i + 1], in[i + 0 + HALF], in[i + 1 + HALF]);
      const __m256i o1 = m256_pairwise_epi16(in[i + 2], in[i + 3], in[i + 2 + HALF], in[i + 3 + HALF]);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      count = StoreNNZ(nnz, count, &base, increment, _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(o0, zero))));
      count = StoreNNZ(nnz, count, &base, increment, _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(o1, zero))));
    }
  }

  return count;
}
#elif defined(__SSE4_1__)
INLINE __m128i m128_pairwise_epi16(__m128i a0, __m128i a1, __m128i b0, __m128i b1) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i cap  = _mm_set1_epi16(FT_MAX);

  a0 = _mm_slli_epi16(_mm_min_epi16(_mm_max_epi16(a0, zero), cap), 16 - FT_SHIFT);
  a1 = _mm_slli_epi16(_mm_min_epi16(_mm_max_epi16(a1, zero), cap), 16 - FT_SHIFT);
  b0 = _mm_min_epi16(b0, cap);
  b1 = _mm_min_epi16(b1, cap);

  return _mm_packus_epi16(_mm_mulhi_epi16(a0, b0), _mm_mulhi_epi16(a1, b1));
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH = sizeof(__m128i) / sizeof(acc_t);
  const size_t HALF  = N_HIDDEN / 2 / WIDTH;
  const int views[2] = {stm, !stm};

  const __m128i zero      = _mm_setzero_si128();
  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const __m128i o0 = m128_pairwise_epi16(in[i + 0], in[i + 1], in[i + 0 + HALF], in[i + 1 + HALF]);
      const __m128i o1 = m128_pairwise_epi16(in[i + 2], in[i + 3], in[i + 2 + HALF], in[i + 3 + HALF]);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      const uint32_t m0 = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(o0, zero)));
      const uint32_t m1 = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(o1, zero)));

      count = StoreNNZ(nnz, count, &base, increment, m0 | (m1 << 4));
    }
  }

  return count;
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE uint8x16_t u8x16_pairwise_s16(int16x8_t a0, int16x8_t a1, int16x8_t b0, int16x8_t b1) {
  const int16x8_t zero = vdupq_n_s16(0);
  const int16x8_t cap  = vdupq_n_s16(FT_MAX);

  // vqdmulh doubles the product, so shift one less than the x86 mulhi path
  a0 = vshlq_n_s16(vminq_s16(vmaxq_s16(a0, zero), cap), 15 - FT_SHIFT);
  a1 = vshlq_n_s16(vminq_s16(vmaxq_s16(a1, zero), cap), 15 - FT_SHIFT);
  b0 = vminq_s16(b0, cap);
  b1 = vminq_s16(b1, cap);

  return vcombine_u8(vqmovun_s16(vqdmulhq_s16(a0, b0)), vqmovun_s16(vqdmulhq_s16(a1, b1)));
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH = sizeof(int16x8_t) / sizeof(acc_t);
  const size_t HALF  = N_HIDDEN / 2 / WIDTH;
  const int views[2] = {stm, !stm};

  const uint16_t lanes[8]    = {1, 2, 4, 8, 16, 32, 64, 128};
  const uint16x8_t bits      = vld1q_u16(lanes);
  const uint16x8_t increment = vdupq_n_u16(8);
  uint16x8_t base            = vdupq_n_u16(0);
  size_t count               = 0;

  for (int v = 0; v < 2; v++) {
    const int16x8_t* in = (int16x8_t*) acc->values[views[v]];
    uint8x16_t* out     = (uint8x16_t*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const uint8x16_t o0 = u8x16_pairwise_s16(in[i + 0], in[i + 1], in[i + 0 + HALF], in[i + 1 + HALF]);
      const uint8x16_t o1 = u8x16_pairwise_s16(in[i + 2], in[i + 3], in[i + 2 + HALF], in[i + 3 + HALF]);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      const uint32x4_t c0 = vreinterpretq_u32_u8(o0);
      const uint32x4_t c1 = vreinterpretq_u32_u8(o1);
      const uint16x8_t nz = vcombine_u16(vmovn_u32(vtstq_u32(c0, c0)), vmovn_u32(vtstq_u32(c1, c1)));

      const uint32_t lookup    = vaddvq_u16(vandq_u16(nz, bits));
      const uint16x8_t offsets = vld1q_u16(LOOKUP_INDICES[lookup]);
      vst1q_u16(nnz + count, vaddq_u16(base, offsets));
      count += BitCount(lookup);
      base = vaddq_u16(base, increment);
    }
  }

  return count;
}
#else
INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
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
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE void m512_add_dpbusd_epi32(__m512i* acc, __m512i a, __m512i b) {
#if defined(__AVX512VNNI__)
  *acc = _mm512_dpbusd_epi32(*acc, a, b);
#else
  __m512i p0 = _mm512_maddubs_epi16(a, b);
  p0         = _mm512_madd_epi16(p0, _mm512_set1_epi16(1));
  *acc       = _mm512_add_epi32(*acc, p0);
#endif
}

INLINE void m512_add_dpbusd_epi32x2(__m512i* acc, __m512i a0, __m512i b0, __m512i a1, __m512i b1) {
#if defined(__AVX512VNNI__)
  *acc = _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(*acc, a0, b0), a1, b1);
#else
  __m512i p0 = _mm512_maddubs_epi16(a0, b0);
  __m512i p1 = _mm512_maddubs_epi16(a1, b1);

  p0   = _mm512_madd_epi16(_mm512_add_epi16(p0, p1), _mm512_set1_epi16(1));
  *acc = _mm512_add_epi32(*acc, p0);
#endif
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m512i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L2 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m512i* bias = (__m512i*) biases;
  __m512i* out        = (__m512i*) dest;

  __m512i regs[OUT_CC], alts[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++) {
    regs[i] = bias[i];
    alts[i] = _mm512_setzero_si512();
  }

  size_t i = 0;
  for (; i + 3 < count; i += 4) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];
    const uint16_t i2 = nnz[i + 2];
    const uint16_t i3 = nnz[i + 3];

    const __m512i f0 = _mm512_set1_epi32(in32[i0]);
    const __m512i f1 = _mm512_set1_epi32(in32[i1]);
    const __m512i f2 = _mm512_set1_epi32(in32[i2]);
    const __m512i f3 = _mm512_set1_epi32(in32[i3]);

    const __m512i* c0 = (__m512i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m512i* c1 = (__m512i*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m512i* c2 = (__m512i*) &weights[i2 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m512i* c3 = (__m512i*) &weights[i3 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++) {
      m512_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
      m512_add_dpbusd_epi32x2(alts + j, f2, c2[j], f3, c3[j]);
    }
  }

  for (; i < count; i++) {
    const uint16_t i0 = nnz[i];
    const __m512i f0  = _mm512_set1_epi32(in32[i0]);
    const __m512i* c0 = (__m512i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = _mm512_add_epi32(regs[i], alts[i]);
}
#elif defined(__AVX2__)
INLINE void m256_add_dpbusd_epi32(__m256i* acc, __m256i a, __m256i b) {
#if defined(__AVXVNNI__)
  *acc = _mm256_dpbusd_avx_epi32(*acc, a, b);
#else
  __m256i p0 = _mm256_maddubs_epi16(a, b);
  p0         = _mm256_madd_epi16(p0, _mm256_set1_epi16(1));
  *acc       = _mm256_add_epi32(*acc, p0);
#endif
}

INLINE void m256_add_dpbusd_epi32x2(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
#if defined(__AVXVNNI__)
  *acc = _mm256_dpbusd_avx_epi32(_mm256_dpbusd_avx_epi32(*acc, a0, b0), a1, b1);
#else
  __m256i p0 = _mm256_maddubs_epi16(a0, b0);
  __m256i p1 = _mm256_maddubs_epi16(a1, b1);

  p0   = _mm256_madd_epi16(_mm256_add_epi16(p0, p1), _mm256_set1_epi16(1));
  *acc = _mm256_add_epi32(*acc, p0);
#endif
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m256i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L2 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m256i* bias = (__m256i*) biases;
  __m256i* out        = (__m256i*) dest;

  __m256i regs[OUT_CC], alts[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++) {
    regs[i] = bias[i];
    alts[i] = _mm256_setzero_si256();
  }

  size_t i = 0;
  for (; i + 3 < count; i += 4) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];
    const uint16_t i2 = nnz[i + 2];
    const uint16_t i3 = nnz[i + 3];

    const __m256i f0 = _mm256_set1_epi32(in32[i0]);
    const __m256i f1 = _mm256_set1_epi32(in32[i1]);
    const __m256i f2 = _mm256_set1_epi32(in32[i2]);
    const __m256i f3 = _mm256_set1_epi32(in32[i3]);

    const __m256i* c0 = (__m256i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m256i* c1 = (__m256i*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m256i* c2 = (__m256i*) &weights[i2 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m256i* c3 = (__m256i*) &weights[i3 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++) {
      m256_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
      m256_add_dpbusd_epi32x2(alts + j, f2, c2[j], f3, c3[j]);
    }
  }

  for (; i < count; i++) {
    const uint16_t i0 = nnz[i];
    const __m256i f0  = _mm256_set1_epi32(in32[i0]);
    const __m256i* c0 = (__m256i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = _mm256_add_epi32(regs[i], alts[i]);
}
#elif defined(__SSE4_1__)
INLINE void m128_add_dpbusd_epi32(__m128i* acc, __m128i a, __m128i b) {
  __m128i p0 = _mm_maddubs_epi16(a, b);
  p0         = _mm_madd_epi16(p0, _mm_set1_epi16(1));
  *acc       = _mm_add_epi32(*acc, p0);
}

INLINE void m128_add_dpbusd_epi32x2(__m128i* acc, __m128i a0, __m128i b0, __m128i a1, __m128i b1) {
  __m128i p0 = _mm_maddubs_epi16(a0, b0);
  __m128i p1 = _mm_maddubs_epi16(a1, b1);

  p0   = _mm_madd_epi16(_mm_add_epi16(p0, p1), _mm_set1_epi16(1));
  *acc = _mm_add_epi32(*acc, p0);
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m128i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L2 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m128i* bias = (__m128i*) biases;
  __m128i* out        = (__m128i*) dest;

  __m128i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = bias[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m128i f0 = _mm_set1_epi32(in32[i0]);
    const __m128i f1 = _mm_set1_epi32(in32[i1]);

    const __m128i* c0 = (__m128i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m128i* c1 = (__m128i*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m128_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m128i f0  = _mm_set1_epi32(in32[i0]);
    const __m128i* c0 = (__m128i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m128_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
// Every uint8 activation is capped at 127, so the signed dot product is exact.
INLINE void int8x16_add_dpbusd(int32x4_t* acc, int8x16_t a, int8x16_t b) {
#if defined(__ARM_FEATURE_DOTPROD)
  *acc = vdotq_s32(*acc, a, b);
#else
  const int16x8_t p0 = vmull_s8(vget_low_s8(a), vget_low_s8(b));
  const int16x8_t p1 = vmull_high_s8(a, b);

  *acc = vpadalq_s16(*acc, vpaddq_s16(p0, p1));
#endif
}

INLINE void int8x16_add_dpbusd_x2(int32x4_t* acc, int8x16_t a0, int8x16_t b0, int8x16_t a1, int8x16_t b1) {
#if defined(__ARM_FEATURE_DOTPROD)
  *acc = vdotq_s32(vdotq_s32(*acc, a0, b0), a1, b1);
#else
  const int16x8_t p0 = vmull_s8(vget_low_s8(a0), vget_low_s8(b0));
  const int16x8_t p1 = vmull_high_s8(a0, b0);
  const int16x8_t p2 = vmull_s8(vget_low_s8(a1), vget_low_s8(b1));
  const int16x8_t p3 = vmull_high_s8(a1, b1);

  *acc = vpadalq_s16(vpadalq_s16(*acc, vpaddq_s16(p0, p1)), vpaddq_s16(p2, p3));
#endif
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(int32x4_t) / sizeof(int32_t);
  const size_t OUT_CC    = N_L2 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  int32x4_t* out      = (int32x4_t*) dest;

  int32x4_t regs[OUT_CC], alts[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++) {
    regs[i] = vld1q_s32(biases + i * OUT_WIDTH);
    alts[i] = vdupq_n_s32(0);
  }

  size_t i = 0;
  for (; i + 3 < count; i += 4) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];
    const uint16_t i2 = nnz[i + 2];
    const uint16_t i3 = nnz[i + 3];

    const int8x16_t f0 = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[i0]));
    const int8x16_t f1 = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[i1]));
    const int8x16_t f2 = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[i2]));
    const int8x16_t f3 = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[i3]));

    const int8x16_t* c0 = (int8x16_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const int8x16_t* c1 = (int8x16_t*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];
    const int8x16_t* c2 = (int8x16_t*) &weights[i2 * N_L2 * SPARSE_CHUNK_SIZE];
    const int8x16_t* c3 = (int8x16_t*) &weights[i3 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++) {
      int8x16_add_dpbusd_x2(regs + j, f0, c0[j], f1, c1[j]);
      int8x16_add_dpbusd_x2(alts + j, f2, c2[j], f3, c3[j]);
    }
  }

  for (; i < count; i++) {
    const uint16_t i0   = nnz[i];
    const int8x16_t f0  = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[i0]));
    const int8x16_t* c0 = (int8x16_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      int8x16_add_dpbusd(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = vaddq_s32(regs[i], alts[i]);
}
#else
INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights,
                     const int32_t* biases) {
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
}
#endif

#if defined(__SSE4_1__)
INLINE void L1Activate(uint8_t* clamped, uint8_t* squared, int32_t* src) {
  const __m128i* in = (__m128i*) src;
  __m128i* outC     = (__m128i*) clamped;
  __m128i* outS     = (__m128i*) squared;

  for (size_t i = 0; i < N_L2 / 16; i++) {
    const __m128i a0 = _mm_packs_epi32(in[4 * i + 0], in[4 * i + 1]);
    const __m128i a1 = _mm_packs_epi32(in[4 * i + 2], in[4 * i + 3]);

    const __m128i c = _mm_packs_epi16(_mm_srai_epi16(a0, 6), _mm_srai_epi16(a1, 6));
    outC[i]         = _mm_max_epi8(c, _mm_setzero_si128());

    const __m128i s0 = _mm_srli_epi16(_mm_mulhi_epi16(a0, a0), 3);
    const __m128i s1 = _mm_srli_epi16(_mm_mulhi_epi16(a1, a1), 3);
    outS[i]          = _mm_packs_epi16(s0, s1);
  }
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE void L1Activate(uint8_t* clamped, uint8_t* squared, int32_t* src) {
  const int32x4_t* in = (int32x4_t*) src;
  int8x16_t* outC     = (int8x16_t*) clamped;
  int8x16_t* outS     = (int8x16_t*) squared;

  for (size_t i = 0; i < N_L2 / 16; i++) {
    const int16x8_t a0 = vcombine_s16(vqmovn_s32(in[4 * i + 0]), vqmovn_s32(in[4 * i + 1]));
    const int16x8_t a1 = vcombine_s16(vqmovn_s32(in[4 * i + 2]), vqmovn_s32(in[4 * i + 3]));

    const int8x16_t c = vcombine_s8(vqshrn_n_s16(a0, 6), vqshrn_n_s16(a1, 6));
    outC[i]           = vmaxq_s8(c, vdupq_n_s8(0));

    // vqdmulh is (2 * a * a) >> 16, one more shift recovers the x86 mulhi >> 3
    outS[i] = vcombine_s8(vqshrn_n_s16(vqdmulhq_s16(a0, a0), 4), vqshrn_n_s16(vqdmulhq_s16(a1, a1), 4));
  }
}
#else
INLINE int32_t Sat16(const int32_t x) {
  return x < -32768 ? -32768 : x > 32767 ? 32767 : x;
}

INLINE uint8_t SquareQ(const int32_t x) {
  const int32_t s = Sat16(x);
  const int32_t q = ((s * s) >> 16) >> 3;
  return q > 127 ? 127 : q;
}

INLINE void L1Activate(uint8_t* clamped, uint8_t* squared, int32_t* src) {
  for (size_t i = 0; i < N_L2; i++) {
    const int32_t c = src[i] >> 6;
    clamped[i]      = c < 0 ? 0 : c > 127 ? 127 : c;
    squared[i]      = SquareQ(src[i]);
  }
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m512i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L3 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m512i* w    = (__m512i*) weights;
  const __m512i* bias = (__m512i*) biases;
  __m512i* out        = (__m512i*) dest;

  __m512i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = bias[i];

  for (size_t j = 0; j < N_L2_CHUNKS; j += 2) {
    const __m512i f0  = _mm512_set1_epi32(in32[j + 0]);
    const __m512i f1  = _mm512_set1_epi32(in32[j + 1]);
    const __m512i* c0 = w + (j + 0) * OUT_CC;
    const __m512i* c1 = w + (j + 1) * OUT_CC;

    for (size_t i = 0; i < OUT_CC; i++)
      m512_add_dpbusd_epi32x2(regs + i, f0, c0[i], f1, c1[i]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#elif defined(__AVX2__)
INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m256i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L3 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m256i* w    = (__m256i*) weights;
  const __m256i* bias = (__m256i*) biases;
  __m256i* out        = (__m256i*) dest;

  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = bias[i];

  for (size_t j = 0; j < N_L2_CHUNKS; j += 2) {
    const __m256i f0  = _mm256_set1_epi32(in32[j + 0]);
    const __m256i f1  = _mm256_set1_epi32(in32[j + 1]);
    const __m256i* c0 = w + (j + 0) * OUT_CC;
    const __m256i* c1 = w + (j + 1) * OUT_CC;

    for (size_t i = 0; i < OUT_CC; i++)
      m256_add_dpbusd_epi32x2(regs + i, f0, c0[i], f1, c1[i]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#elif defined(__SSE4_1__)
INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(__m128i) / sizeof(int32_t);
  const size_t OUT_CC    = N_L3 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const __m128i* w    = (__m128i*) weights;
  const __m128i* bias = (__m128i*) biases;
  __m128i* out        = (__m128i*) dest;

  __m128i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = bias[i];

  for (size_t j = 0; j < N_L2_CHUNKS; j += 2) {
    const __m128i f0  = _mm_set1_epi32(in32[j + 0]);
    const __m128i f1  = _mm_set1_epi32(in32[j + 1]);
    const __m128i* c0 = w + (j + 0) * OUT_CC;
    const __m128i* c1 = w + (j + 1) * OUT_CC;

    for (size_t i = 0; i < OUT_CC; i++)
      m128_add_dpbusd_epi32x2(regs + i, f0, c0[i], f1, c1[i]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
  const size_t OUT_WIDTH = sizeof(int32x4_t) / sizeof(int32_t);
  const size_t OUT_CC    = N_L3 / OUT_WIDTH;

  const int32_t* in32 = (int32_t*) src;
  const int8x16_t* w  = (int8x16_t*) weights;
  int32x4_t* out      = (int32x4_t*) dest;

  int32x4_t regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = vld1q_s32(biases + i * OUT_WIDTH);

  for (size_t j = 0; j < N_L2_CHUNKS; j += 2) {
    const int8x16_t f0  = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[j + 0]));
    const int8x16_t f1  = vreinterpretq_s8_s32(vld1q_dup_s32(&in32[j + 1]));
    const int8x16_t* c0 = w + (j + 0) * OUT_CC;
    const int8x16_t* c1 = w + (j + 1) * OUT_CC;

    for (size_t i = 0; i < OUT_CC; i++)
      int8x16_add_dpbusd_x2(regs + i, f0, c0[i], f1, c1[i]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#else
INLINE void L2Affine(int32_t* dest, uint8_t* src, const int8_t* weights, const int32_t* biases) {
  for (size_t o = 0; o < N_L3; o++)
    dest[o] = biases[o];

  for (size_t i = 0; i < N_L1_ACT; i++)
    for (size_t o = 0; o < N_L3; o++)
      dest[o] += src[i] * weights[(i / 4) * N_L3 * 4 + o * 4 + (i % 4)];
}
#endif

#if defined(__SSE4_1__)
INLINE void L2Activate(uint8_t* dest, int32_t* src) {
  const __m128i* in = (__m128i*) src;
  __m128i* out      = (__m128i*) dest;

  for (size_t i = 0; i < N_L3 / 16; i++) {
    __m128i a0 = _mm_packs_epi32(in[4 * i + 0], in[4 * i + 1]);
    __m128i a1 = _mm_packs_epi32(in[4 * i + 2], in[4 * i + 3]);
    a0         = _mm_max_epi16(a0, _mm_setzero_si128());
    a1         = _mm_max_epi16(a1, _mm_setzero_si128());

    const __m128i s0 = _mm_srli_epi16(_mm_mulhi_epi16(a0, a0), 3);
    const __m128i s1 = _mm_srli_epi16(_mm_mulhi_epi16(a1, a1), 3);
    out[i]           = _mm_packs_epi16(s0, s1);
  }
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE void L2Activate(uint8_t* dest, int32_t* src) {
  const int32x4_t* in = (int32x4_t*) src;
  int8x16_t* out      = (int8x16_t*) dest;

  for (size_t i = 0; i < N_L3 / 16; i++) {
    int16x8_t a0 = vcombine_s16(vqmovn_s32(in[4 * i + 0]), vqmovn_s32(in[4 * i + 1]));
    int16x8_t a1 = vcombine_s16(vqmovn_s32(in[4 * i + 2]), vqmovn_s32(in[4 * i + 3]));
    a0           = vmaxq_s16(a0, vdupq_n_s16(0));
    a1           = vmaxq_s16(a1, vdupq_n_s16(0));

    out[i] = vcombine_s8(vqshrn_n_s16(vqdmulhq_s16(a0, a0), 4), vqshrn_n_s16(vqdmulhq_s16(a1, a1), 4));
  }
}
#else
INLINE void L2Activate(uint8_t* dest, int32_t* src) {
  for (size_t i = 0; i < N_L3; i++)
    dest[i] = SquareQ(src[i] < 0 ? 0 : src[i]);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
  __m512i a0 = _mm512_setzero_si512();
  m512_add_dpbusd_epi32(&a0, *(__m512i*) src, *(__m512i*) weights);

  return _mm512_reduce_add_epi32(a0);
}
#elif defined(__AVX2__)
INLINE int32_t m256_reduce_add_epi32(__m256i a) {
  const __m128i a4 = _mm_add_epi32(_mm256_castsi256_si128(a), _mm256_extracti128_si256(a, 1));
  const __m128i a2 = _mm_add_epi32(a4, _mm_shuffle_epi32(a4, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1);
}

INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
  const __m256i* in = (__m256i*) src;
  const __m256i* w  = (__m256i*) weights;

  __m256i a0 = _mm256_setzero_si256();
  m256_add_dpbusd_epi32x2(&a0, in[0], w[0], in[1], w[1]);

  return m256_reduce_add_epi32(a0);
}
#elif defined(__SSE4_1__)
INLINE int32_t m128_reduce_add_epi32(__m128i a) {
  const __m128i a2 = _mm_add_epi32(a, _mm_shuffle_epi32(a, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1);
}

INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
  const __m128i* in = (__m128i*) src;
  const __m128i* w  = (__m128i*) weights;

  __m128i a0 = _mm_setzero_si128();
  m128_add_dpbusd_epi32x2(&a0, in[0], w[0], in[1], w[1]);
  m128_add_dpbusd_epi32x2(&a0, in[2], w[2], in[3], w[3]);

  return m128_reduce_add_epi32(a0);
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
  const int8x16_t* in = (int8x16_t*) src;
  const int8x16_t* w  = (int8x16_t*) weights;

  int32x4_t a0 = vdupq_n_s32(0);
  int8x16_add_dpbusd_x2(&a0, in[0], w[0], in[1], w[1]);
  int8x16_add_dpbusd_x2(&a0, in[2], w[2], in[3], w[3]);

  return vaddvq_s32(a0);
}
#else
INLINE int32_t L3Transform(uint8_t* src, const int8_t* weights) {
  int32_t result = 0;

  for (size_t i = 0; i < N_L3_IN; i++)
    result += src[i] * weights[i];

  return result;
}
#endif

INLINE int PropagateView(Accumulator* accumulator, const int stm, const int bucket) {
  uint8_t x0[N_L1] ALIGN;
  // The index list is written in whole groups of up to 32, so leave room for the tail.
  uint16_t nnz[N_L1 / SPARSE_CHUNK_SIZE + 32] ALIGN;
  int32_t dest[N_L3] ALIGN;
  uint8_t act[N_L3_IN] ALIGN;

  const size_t count = InputPairwise8(x0, nnz, accumulator, stm);
  L1Affine(dest, x0, nnz, count, L1_WEIGHTS[bucket], L1_BIASES[bucket]);
  L1Activate(act + N_L3, act + N_L3 + N_L2, dest);
  L2Affine(dest, act + N_L3, L2_WEIGHTS[bucket], L2_BIASES[bucket]);
  L2Activate(act, dest);

  const int32_t out = L3Transform(act, L3_WEIGHTS[bucket]) + L3_BIASES[bucket];
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

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  // Alloc a chunk of memory for the L1 weights which we
  // cannot copy into the stack directly
  int8_t* l1 = malloc(N_L1 * N_L2 * sizeof(int8_t));

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);

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

#if defined(__AVX512F__) && defined(__AVX512BW__)
  const size_t WIDTH         = sizeof(__m512i) / sizeof(int16_t);
  const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  __m512i* weights = (__m512i*) INPUT_WEIGHTS;
  __m512i* biases  = (__m512i*) INPUT_BIASES;

  for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
    __m128i a1 = _mm512_extracti32x4_epi32(weights[i], 1);
    __m128i a2 = _mm512_extracti32x4_epi32(weights[i], 2);
    __m128i a3 = _mm512_extracti32x4_epi32(weights[i], 3);
    __m128i b0 = _mm512_extracti32x4_epi32(weights[i + 1], 0);
    __m128i b1 = _mm512_extracti32x4_epi32(weights[i + 1], 1);
    __m128i b2 = _mm512_extracti32x4_epi32(weights[i + 1], 2);

    weights[i]     = _mm512_inserti32x4(weights[i], a2, 1);
    weights[i]     = _mm512_inserti32x4(weights[i], b0, 2);
    weights[i]     = _mm512_inserti32x4(weights[i], b2, 3);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], a1, 0);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], a3, 1);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], b1, 2);
  }

  for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
    __m128i a1 = _mm512_extracti32x4_epi32(biases[i], 1);
    __m128i a2 = _mm512_extracti32x4_epi32(biases[i], 2);
    __m128i a3 = _mm512_extracti32x4_epi32(biases[i], 3);
    __m128i b0 = _mm512_extracti32x4_epi32(biases[i + 1], 0);
    __m128i b1 = _mm512_extracti32x4_epi32(biases[i + 1], 1);
    __m128i b2 = _mm512_extracti32x4_epi32(biases[i + 1], 2);

    biases[i]     = _mm512_inserti32x4(biases[i], a2, 1);
    biases[i]     = _mm512_inserti32x4(biases[i], b0, 2);
    biases[i]     = _mm512_inserti32x4(biases[i], b2, 3);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], a1, 0);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], a3, 1);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], b1, 2);
  }
#elif defined(__AVX2__)
  const size_t WIDTH         = sizeof(__m256i) / sizeof(int16_t);
  const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  __m256i* weights = (__m256i*) INPUT_WEIGHTS;
  __m256i* biases  = (__m256i*) INPUT_BIASES;

  for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
    __m128i a1 = _mm256_extracti128_si256(weights[i], 1);
    __m128i b0 = _mm256_extracti128_si256(weights[i + 1], 0);

    weights[i]     = _mm256_inserti128_si256(weights[i], b0, 1);
    weights[i + 1] = _mm256_inserti128_si256(weights[i + 1], a1, 0);
  }

  for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
    __m128i a1 = _mm256_extracti128_si256(biases[i], 1);
    __m128i b0 = _mm256_extracti128_si256(biases[i + 1], 0);

    biases[i]     = _mm256_inserti128_si256(biases[i], b0, 1);
    biases[i + 1] = _mm256_inserti128_si256(biases[i + 1], a1, 0);
  }
#endif
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

  if ((size_t) EmbedSize < NETWORK_SIZE) {
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

  uint8_t* data = malloc(NETWORK_SIZE);
  if (fread(data, sizeof(uint8_t), NETWORK_SIZE, fin) != NETWORK_SIZE) {
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
