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

#include <math.h>
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

// QA = 255: the feature transformer's activation is a uint8 spanning the byte's
// whole range, and the CReLU clip is the byte itself. Each later stage rescales
// by its own shift -- they were one shared constant when every stage happened to
// want 5.
//
// The transformer's two halves are then multiplied together pairwise: unit i of
// the output is `crelu(acc[i]) * crelu(acc[i + N_HIDDEN / 2]) >> FT_SHIFT`, so
// N_HIDDEN units per perspective become N_HIDDEN / 2 and L1 takes N_HIDDEN
// inputs, not two.
//
// FT_SHIFT is 9 and not 8 because the product has to come back into a byte with
// room to spare: 255 * 255 >> 9 is 127, and L1Affine emulates `dpbusd` with
// `maddubs`, which sums two uint8 x int8 products into a *saturating* int16.
// At 127 that step is 127 * 127 * 2 = 32,258 and cannot saturate; at 255 it
// could.
//
// Everything after L1 is float: L1's int32 sums are dequantised by L1_NORM and
// clipped by clamp(x, 0, 1), a float L2 applies SCReLU, and
// L3 reads L2's output alone. L1, L2 and L3 each hold one block
// of weights per material-count output bucket, chosen by OutputBucket.
//
// The float layers run in one fixed order, one multiply and then one add, and
// the makefile builds with -ffp-contract=off so no compiler fuses the two. That
// makes the eval bit-identical on every ISA and to the trainer's `net.rs`, which
// is what lets the featdump gates demand zero mismatches.
#define FT_MAX     255
#define FT_SHIFT   9
#define EVAL_SCALE 160.0f

// L1's inputs sit at 255 * 255 / 512 and its weights at 64.
#define L1_NORM ((float) (1 << FT_SHIFT) / (float) (FT_MAX * FT_MAX * 64))

#define N_L1_ACT N_L2              // clamp(x, 0, 1)
#define N_L3_IN  N_L3              // L2's output alone
#define L3_LANES 8                 // partial sums L3 keeps before its fixed tree

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_OUTPUT_BUCKETS][N_L1 * N_L2] ALIGN;
float L1_BIASES[N_OUTPUT_BUCKETS][N_L2] ALIGN;

// [bucket][input][output], so L2 runs across its outputs one input at a time.
float L2_WEIGHTS[N_OUTPUT_BUCKETS][N_L1_ACT * N_L3] ALIGN;
float L2_BIASES[N_OUTPUT_BUCKETS][N_L3] ALIGN;

float L3_WEIGHTS[N_OUTPUT_BUCKETS][N_L3_IN] ALIGN;
float L3_BIASES[N_OUTPUT_BUCKETS];

uint16_t LOOKUP_INDICES[256][8] ALIGN;

// The sparse-input index list is built in the same pass that produces the
// activation bytes, instead of streaming all of L1 a second time.
//
// The test has to be "this 4-byte chunk is non-zero", not "> 0": a CReLU output
// byte now reaches 255, so a chunk whose high byte is >= 128 reads as a negative
// int32 and a signed greater-than would drop a live input.
#ifdef __SSE4_1__
#include <immintrin.h>

INLINE size_t StoreNNZ(uint16_t* dest, size_t count, __m128i* base, const __m128i increment, const uint32_t lookup) {
  const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
  _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(*base, offsets));
  *base = _mm_add_epi16(*base, increment);
  return count + BitCount(lookup);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(acc_t);
  const size_t HALF   = N_HIDDEN / 2 / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m512i zero = _mm512_setzero_si512();
  const __m512i cap  = _mm512_set1_epi16(FT_MAX);

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m512i* in = (__m512i*) acc->values[views[v]];
    __m512i* out      = (__m512i*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 2) {
      const __m512i a0 = _mm512_min_epi16(_mm512_max_epi16(in[i + 0], zero), cap);
      const __m512i b0 = _mm512_min_epi16(in[i + 0 + HALF], cap);
      const __m512i a1 = _mm512_min_epi16(_mm512_max_epi16(in[i + 1], zero), cap);
      const __m512i b1 = _mm512_min_epi16(in[i + 1 + HALF], cap);

      const __m512i p0 = _mm512_mulhi_epi16(_mm512_slli_epi16(a0, 16 - FT_SHIFT), b0);
      const __m512i p1 = _mm512_mulhi_epi16(_mm512_slli_epi16(a1, 16 - FT_SHIFT), b1);

      const __m512i o0 = _mm512_packus_epi16(p0, p1);

      out[i / 2] = o0;

      const uint32_t m0 = _mm512_test_epi32_mask(o0, o0);

      count = StoreNNZ(nnz, count, &base, increment, m0 & 0xFF);
      count = StoreNNZ(nnz, count, &base, increment, m0 >> 8);
    }
  }

  return count;
}
#elif defined(__AVX2__)
#include <immintrin.h>

INLINE uint32_t NonZeroMask256(__m256i v) {
  const __m256i eq = _mm256_cmpeq_epi32(v, _mm256_setzero_si256());
  return (~(uint32_t) _mm256_movemask_ps(_mm256_castsi256_ps(eq))) & 0xFF;
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t HALF   = N_HIDDEN / 2 / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m256i zero = _mm256_setzero_si256();
  const __m256i cap  = _mm256_set1_epi16(FT_MAX);

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 2) {
      const __m256i a0 = _mm256_min_epi16(_mm256_max_epi16(in[i + 0], zero), cap);
      const __m256i b0 = _mm256_min_epi16(in[i + 0 + HALF], cap);
      const __m256i a1 = _mm256_min_epi16(_mm256_max_epi16(in[i + 1], zero), cap);
      const __m256i b1 = _mm256_min_epi16(in[i + 1 + HALF], cap);

      const __m256i p0 = _mm256_mulhi_epi16(_mm256_slli_epi16(a0, 16 - FT_SHIFT), b0);
      const __m256i p1 = _mm256_mulhi_epi16(_mm256_slli_epi16(a1, 16 - FT_SHIFT), b1);

      const __m256i o0 = _mm256_packus_epi16(p0, p1);

      out[i / 2] = o0;

      count = StoreNNZ(nnz, count, &base, increment, NonZeroMask256(o0));
    }
  }

  return count;
}
#elif defined(__SSE4_1__)
#include <immintrin.h>

INLINE uint32_t NonZeroMask128(__m128i v) {
  const __m128i eq = _mm_cmpeq_epi32(v, _mm_setzero_si128());
  return (~(uint32_t) _mm_movemask_ps(_mm_castsi128_ps(eq))) & 0xF;
}

INLINE __m128i PairwiseProduct128(__m128i a, __m128i b, __m128i zero, __m128i cap) {
  const __m128i lo = _mm_min_epi16(_mm_max_epi16(a, zero), cap);
  const __m128i hi = _mm_min_epi16(b, cap);

  return _mm_mulhi_epi16(_mm_slli_epi16(lo, 16 - FT_SHIFT), hi);
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t HALF   = N_HIDDEN / 2 / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m128i zero = _mm_setzero_si128();
  const __m128i cap  = _mm_set1_epi16(FT_MAX);

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[(N_HIDDEN / 2) * v];

    // Four accumulator vectors a time, because StoreNNZ wants a byte of mask and
    // one 16-byte output vector only yields four int32 lanes.
    for (size_t i = 0; i < HALF; i += 4) {
      const __m128i p0 = PairwiseProduct128(in[i + 0], in[i + 0 + HALF], zero, cap);
      const __m128i p1 = PairwiseProduct128(in[i + 1], in[i + 1 + HALF], zero, cap);
      const __m128i p2 = PairwiseProduct128(in[i + 2], in[i + 2 + HALF], zero, cap);
      const __m128i p3 = PairwiseProduct128(in[i + 3], in[i + 3 + HALF], zero, cap);

      const __m128i o0 = _mm_packus_epi16(p0, p1);
      const __m128i o1 = _mm_packus_epi16(p2, p3);

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      const uint32_t m0 = NonZeroMask128(o0);
      const uint32_t m1 = NonZeroMask128(o1);

      count = StoreNNZ(nnz, count, &base, increment, m0 | (m1 << 4));
    }
  }

  return count;
}
#elif defined(__ARM_NEON__)
#include <arm_neon.h>

// NEON has no mulhi, so the product is widened to int32 and shifted back down.
// vshrn_n_s32 is an arithmetic shift, so a negative product floors exactly as
// mulhi does on x86, and vqmovun_s16 below clamps it to zero.
INLINE int16x8_t PairwiseProductNeon(int16x8_t a, int16x8_t b, int16x8_t zero, int16x8_t cap) {
  const int16x8_t lo = vminq_s16(vmaxq_s16(a, zero), cap);
  const int16x8_t hi = vminq_s16(b, cap);

  const int32x4_t p0 = vmull_s16(vget_low_s16(lo), vget_low_s16(hi));
  const int32x4_t p1 = vmull_high_s16(lo, hi);

  return vcombine_s16(vshrn_n_s32(p0, FT_SHIFT), vshrn_n_s32(p1, FT_SHIFT));
}

INLINE size_t InputPairwise8(uint8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = 8;
  const size_t HALF   = N_HIDDEN / 2 / WIDTH;
  const int views[2]  = {stm, !stm};

  const int16x8_t zero = vdupq_n_s16(0);
  const int16x8_t cap  = vdupq_n_s16(FT_MAX);

  const uint32_t lanes[4]    = {1, 2, 4, 8};
  const uint16x8_t increment = vdupq_n_u16(8);
  uint16x8_t base            = {0};
  size_t count               = 0;

  for (int v = 0; v < 2; v++) {
    const int16x8_t* in = (int16x8_t*) acc->values[views[v]];
    uint8x16_t* out     = (uint8x16_t*) &outputs[(N_HIDDEN / 2) * v];

    for (size_t i = 0; i < HALF; i += 4) {
      const int16x8_t p0 = PairwiseProductNeon(in[i + 0], in[i + 0 + HALF], zero, cap);
      const int16x8_t p1 = PairwiseProductNeon(in[i + 1], in[i + 1 + HALF], zero, cap);
      const int16x8_t p2 = PairwiseProductNeon(in[i + 2], in[i + 2 + HALF], zero, cap);
      const int16x8_t p3 = PairwiseProductNeon(in[i + 3], in[i + 3 + HALF], zero, cap);

      const uint8x16_t o0 = vcombine_u8(vqmovun_s16(p0), vqmovun_s16(p1));
      const uint8x16_t o1 = vcombine_u8(vqmovun_s16(p2), vqmovun_s16(p3));

      out[i / 2 + 0] = o0;
      out[i / 2 + 1] = o1;

      const uint32x4_t c0 = vreinterpretq_u32_u8(o0);
      const uint32x4_t c1 = vreinterpretq_u32_u8(o1);

      const uint32_t m0 = vaddvq_u32(vandq_u32(vtstq_u32(c0, c0), vld1q_u32(lanes)));
      const uint32_t m1 = vaddvq_u32(vandq_u32(vtstq_u32(c1, c1), vld1q_u32(lanes)));

      const uint32_t lookup    = m0 | (m1 << 4);
      const uint16x8_t offsets = vld1q_u16((uint16_t*) &LOOKUP_INDICES[lookup]);
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

  (void) nnz; // the scalar L1 affine walks every input

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
  __m512i p0 = _mm512_maddubs_epi16(a, b);
  p0         = _mm512_madd_epi16(p0, _mm512_set1_epi16(1));
  *acc       = _mm512_add_epi32(*acc, p0);
}

INLINE void m512_add_dpbusd_epi32x2(__m512i* acc, __m512i a0, __m512i b0, __m512i a1, __m512i b1) {
  __m512i p0 = _mm512_maddubs_epi16(a0, b0);
  __m512i p1 = _mm512_maddubs_epi16(a1, b1);

  p0   = _mm512_madd_epi16(_mm512_add_epi16(p0, p1), _mm512_set1_epi16(1));
  *acc = _mm512_add_epi32(*acc, p0);
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights) {
  const size_t OUT_WIDTH  = sizeof(__m512i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  __m512i* out          = (__m512i*) dest;

  __m512i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = _mm512_setzero_si512();

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m512i f0 = _mm512_set1_epi32(in32[i0]);
    const __m512i f1 = _mm512_set1_epi32(in32[i1]);

    const __m512i* c0 = (__m512i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m512i* c1 = (__m512i*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m512i f0  = _mm512_set1_epi32(in32[i0]);
    const __m512i* c0 = (__m512i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#elif defined(__AVX2__)
INLINE void m256_add_dpbusd_epi32(__m256i* acc, __m256i a, __m256i b) {
  __m256i p0 = _mm256_maddubs_epi16(a, b);
  p0         = _mm256_madd_epi16(p0, _mm256_set1_epi16(1));
  *acc       = _mm256_add_epi32(*acc, p0);
}

INLINE void m256_add_dpbusd_epi32x2(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
  __m256i p0 = _mm256_maddubs_epi16(a0, b0);
  __m256i p1 = _mm256_maddubs_epi16(a1, b1);

  p0   = _mm256_madd_epi16(_mm256_add_epi16(p0, p1), _mm256_set1_epi16(1));
  *acc = _mm256_add_epi32(*acc, p0);
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights) {
  const size_t OUT_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  __m256i* out          = (__m256i*) dest;

  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = _mm256_setzero_si256();

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m256i f0 = _mm256_set1_epi32(in32[i0]);
    const __m256i f1 = _mm256_set1_epi32(in32[i1]);

    const __m256i* c0 = (__m256i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m256i* c1 = (__m256i*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m256i f0  = _mm256_set1_epi32(in32[i0]);
    const __m256i* c0 = (__m256i*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
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

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights) {
  const size_t OUT_WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  __m128i* out          = (__m128i*) dest;

  __m128i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = _mm_setzero_si128();

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
#elif defined(__ARM_NEON__)
// The activations are unsigned and reach 255, so the s8 x s8 widening multiply
// the signed byte range allowed would read half of them as negative. Widen to
// int16 first: a product still fits (255 * 127 = 32385) but the pairwise sums do
// not, so they accumulate in int32. Lane grouping matches the old form -- four
// consecutive products per output lane, which is SPARSE_CHUNK_SIZE.
INLINE void int8x16_add_dpbusd(int32x4_t* acc, uint8x16_t a, int8x16_t b) {
  const int16x8_t p0 = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a))), vmovl_s8(vget_low_s8(b)));
  const int16x8_t p1 = vmulq_s16(vreinterpretq_s16_u16(vmovl_high_u8(a)), vmovl_high_s8(b));

  *acc = vaddq_s32(*acc, vpaddq_s32(vpaddlq_s16(p0), vpaddlq_s16(p1)));
}

INLINE void int8x16_add_dpbusd_x2(int32x4_t* acc, uint8x16_t a0, int8x16_t b0, uint8x16_t a1, int8x16_t b1) {
  int8x16_add_dpbusd(acc, a0, b0);
  int8x16_add_dpbusd(acc, a1, b1);
}

INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights) {
  const size_t OUT_WIDTH  = 4;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32     = (int32_t*) src;
  int32x4_t* out          = (int32x4_t*) dest;

  int32x4_t regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = vdupq_n_s32(0);

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const uint8x16_t f0 = vreinterpretq_u8_u32(vdupq_n_u32(in32[i0]));
    const uint8x16_t f1 = vreinterpretq_u8_u32(vdupq_n_u32(in32[i1]));

    const int8x16_t* c0 = (int8x16_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const int8x16_t* c1 = (int8x16_t*) &weights[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      int8x16_add_dpbusd_x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0   = nnz[i];
    const uint8x16_t f0 = vreinterpretq_u8_u32(vdupq_n_u32(in32[i0]));
    const int8x16_t* c0 = (int8x16_t*) &weights[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      int8x16_add_dpbusd(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}
#else
INLINE void L1Affine(int32_t* dest, uint8_t* src, const uint16_t* nnz, const size_t count, const int8_t* weights) {
  (void) nnz;
  (void) count;

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = 0;

  for (size_t i = 0; i < N_L1; i++) {
    if (!src[i])
      continue;

    for (size_t j = 0; j < N_L2; j++)
      dest[j] += src[i] * weights[j * N_L1 + i];
  }
}
#endif

// The float half. Each loop below is written in the one order `net.rs` uses, so
// vectorising it across outputs or lanes -- which reorders nothing -- is the only
// thing a compiler may do to it.

INLINE float Clamp01(const float x) {
  return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}

// L1's int32 sums back to floats, clipped.
INLINE void L1Activate(float* restrict dest, const int32_t* restrict src, const float* restrict biases) {
  for (size_t i = 0; i < N_L2; i++) {
    const float x = (float) src[i] * L1_NORM + biases[i];

    dest[i] = Clamp01(x);
  }
}

// Every output accumulates its inputs in order 0..N_L1_ACT, starting from its bias.
INLINE void L2Affine(float* restrict dest, const float* restrict src, const float* restrict weights,
                     const float* restrict biases) {
  for (size_t j = 0; j < N_L3; j++)
    dest[j] = biases[j];

  for (size_t i = 0; i < N_L1_ACT; i++)
    for (size_t j = 0; j < N_L3; j++)
      dest[j] = dest[j] + src[i] * weights[i * N_L3 + j];

  for (size_t j = 0; j < N_L3; j++) {
    const float c = Clamp01(dest[j]);
    dest[j]       = c * c;
  }
}

// Lane k sums inputs k, k + 8, k + 16, ... of L2's output; the eight lanes then
// fold (k + 4), (k + 2), (k + 1) before the bias is added.
INLINE float L3Transform(const float* restrict l2, const float* restrict weights, const float bias) {
  float lanes[L3_LANES] = {0};

  for (size_t g = 0; g < N_L3_IN; g += L3_LANES)
    for (size_t k = 0; k < L3_LANES; k++)
      lanes[k] = lanes[k] + l2[g + k] * weights[g + k];

  float s4[4], s2[2];
  for (size_t k = 0; k < 4; k++)
    s4[k] = lanes[k] + lanes[k + 4];
  for (size_t k = 0; k < 2; k++)
    s2[k] = s4[k] + s4[k + 2];

  return (s2[0] + s2[1]) + bias;
}

INLINE int PropagateView(Accumulator* accumulator, const int stm, const int bucket) {
  uint8_t x0[N_L1] ALIGN;
  // StoreNNZ always writes a full 8 entry group, so leave room for the tail.
  uint16_t nnz[N_L1 / SPARSE_CHUNK_SIZE + 8] ALIGN;
  int32_t sums[N_L2] ALIGN;
  float l1[N_L1_ACT] ALIGN;
  float l2[N_L3] ALIGN;

  const size_t count = InputPairwise8(x0, nnz, accumulator, stm);
  L1Affine(sums, x0, nnz, count, L1_WEIGHTS[bucket]);
  L1Activate(l1, sums, L1_BIASES[bucket]);
  L2Affine(l2, l1, L2_WEIGHTS[bucket], L2_BIASES[bucket]);

  return (int) (L3Transform(l2, L3_WEIGHTS[bucket], L3_BIASES[bucket]) * EVAL_SCALE);
}

// bullet's MaterialCount<8>: (pieces - 2) / 4, kings counted, so bucket 0 is two
// to five pieces and bucket 7 is thirty or more.
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

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN +              // input weights
                            sizeof(int16_t) * N_HIDDEN +                           // input biases
                            sizeof(int8_t) * N_OUTPUT_BUCKETS * N_L1 * N_L2 +      // L1 weights
                            sizeof(float) * N_OUTPUT_BUCKETS * N_L2 +              // L1 biases
                            sizeof(float) * N_OUTPUT_BUCKETS * N_L3 * N_L1_ACT +   // L2 weights
                            sizeof(float) * N_OUTPUT_BUCKETS * N_L3 +              // L2 biases
                            sizeof(float) * N_OUTPUT_BUCKETS * N_L3_IN +           // L3 weights
                            sizeof(float) * N_OUTPUT_BUCKETS;                      // L3 biases                            // output bias

#if defined(__SSE4_1__) || defined(__ARM_NEON__)
INLINE int WeightIdxScrambled(int idx) {
  return ((idx / SPARSE_CHUNK_SIZE) % (N_L1 / SPARSE_CHUNK_SIZE) * N_L2 * SPARSE_CHUNK_SIZE) +
         (idx / N_L1 * SPARSE_CHUNK_SIZE) + (idx % SPARSE_CHUNK_SIZE);
}

#endif

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

#if defined(__SSE4_1__) || defined(__ARM_NEON__)
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

  // On disk [bucket][output][input]; held [bucket][input][output].
  for (int b = 0; b < N_OUTPUT_BUCKETS; b++)
    for (int o = 0; o < N_L3; o++)
      for (int i = 0; i < N_L1_ACT; i++) {
        memcpy(&L2_WEIGHTS[b][i * N_L3 + o], &in[offset], sizeof(float));
        offset += sizeof(float);
      }

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

  // The makefile's default EVALFILE predates this format and is shorter than it,
  // so a build that forgot EVALFILE would read past the end of the network.
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
