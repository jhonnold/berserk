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

#define QUANT1_BITS 5
#define QUANT2_BITS 12

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

int16_t L2_WEIGHTS[N_L2 * N_L3] ALIGN;
int32_t L2_BIASES[N_L3] ALIGN;

int16_t OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
int32_t OUTPUT_BIAS;

uint16_t LOOKUP_INDICES[256][8] ALIGN;

// Every clipped-ReLU output byte is in [0, 127], so a 4-byte input chunk is
// non-zero exactly when its int32 reinterpretation is > 0. That lets the
// sparse-input index list be built in the same pass that produces the bytes,
// instead of streaming all of L1 a second time.
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
INLINE size_t InputCReLU8(int8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m512i zero      = _mm512_setzero_si512();
  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m512i* in = (__m512i*) acc->values[views[v]];
    __m512i* out      = (__m512i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m512i s0 = _mm512_srai_epi16(in[2 * i + 0], QUANT1_BITS);
      __m512i s1 = _mm512_srai_epi16(in[2 * i + 1], QUANT1_BITS);
      __m512i s2 = _mm512_srai_epi16(in[2 * i + 2], QUANT1_BITS);
      __m512i s3 = _mm512_srai_epi16(in[2 * i + 3], QUANT1_BITS);

      const __m512i o0 = _mm512_max_epi8(_mm512_packs_epi16(s0, s1), zero);
      const __m512i o1 = _mm512_max_epi8(_mm512_packs_epi16(s2, s3), zero);

      out[i]     = o0;
      out[i + 1] = o1;

      const uint32_t m0 = _mm512_cmpgt_epi32_mask(o0, zero);
      const uint32_t m1 = _mm512_cmpgt_epi32_mask(o1, zero);

      count = StoreNNZ(nnz, count, &base, increment, m0 & 0xFF);
      count = StoreNNZ(nnz, count, &base, increment, m0 >> 8);
      count = StoreNNZ(nnz, count, &base, increment, m1 & 0xFF);
      count = StoreNNZ(nnz, count, &base, increment, m1 >> 8);
    }
  }

  return count;
}
#elif defined(__AVX2__)
#include <immintrin.h>
INLINE size_t InputCReLU8(int8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m256i zero      = _mm256_setzero_si256();
  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m256i s0 = _mm256_srai_epi16(in[2 * i + 0], QUANT1_BITS);
      __m256i s1 = _mm256_srai_epi16(in[2 * i + 1], QUANT1_BITS);
      __m256i s2 = _mm256_srai_epi16(in[2 * i + 2], QUANT1_BITS);
      __m256i s3 = _mm256_srai_epi16(in[2 * i + 3], QUANT1_BITS);

      const __m256i o0 = _mm256_max_epi8(_mm256_packs_epi16(s0, s1), zero);
      const __m256i o1 = _mm256_max_epi8(_mm256_packs_epi16(s2, s3), zero);

      out[i]     = o0;
      out[i + 1] = o1;

      count = StoreNNZ(nnz, count, &base, increment, _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(o0, zero))));
      count = StoreNNZ(nnz, count, &base, increment, _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(o1, zero))));
    }
  }

  return count;
}
#elif defined(__SSE4_1__)
#include <immintrin.h>
INLINE size_t InputCReLU8(int8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m128i zero      = _mm_setzero_si128();
  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();
  size_t count            = 0;

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m128i s0 = _mm_srai_epi16(in[2 * i + 0], QUANT1_BITS);
      __m128i s1 = _mm_srai_epi16(in[2 * i + 1], QUANT1_BITS);
      __m128i s2 = _mm_srai_epi16(in[2 * i + 2], QUANT1_BITS);
      __m128i s3 = _mm_srai_epi16(in[2 * i + 3], QUANT1_BITS);

      const __m128i o0 = _mm_max_epi8(_mm_packs_epi16(s0, s1), zero);
      const __m128i o1 = _mm_max_epi8(_mm_packs_epi16(s2, s3), zero);

      out[i]     = o0;
      out[i + 1] = o1;

      const uint32_t m0 = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(o0, zero)));
      const uint32_t m1 = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(o1, zero)));

      count = StoreNNZ(nnz, count, &base, increment, m0 | (m1 << 4));
    }
  }

  return count;
}
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
INLINE size_t InputCReLU8(int8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const size_t WIDTH  = 8;
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  const int8x16_t zero       = {0};
  const uint32_t lanes[4]    = {1, 2, 4, 8};
  const uint16x8_t increment = vdupq_n_u16(8);
  uint16x8_t base            = {0};
  size_t count               = 0;

  for (int v = 0; v < 2; v++) {
    const int16x8_t* in = (int16x8_t*) acc->values[views[v]];
    int8x16_t* out      = (int8x16_t*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      int16x8_t s0 = vshrq_n_s16(in[2 * i + 0], QUANT1_BITS);
      int16x8_t s1 = vshrq_n_s16(in[2 * i + 1], QUANT1_BITS);
      int16x8_t s2 = vshrq_n_s16(in[2 * i + 2], QUANT1_BITS);
      int16x8_t s3 = vshrq_n_s16(in[2 * i + 3], QUANT1_BITS);

      const int8x16_t o0 = vmaxq_s8(vcombine_s8(vqmovn_s16(s0), vqmovn_s16(s1)), zero);
      const int8x16_t o1 = vmaxq_s8(vcombine_s8(vqmovn_s16(s2), vqmovn_s16(s3)), zero);

      out[i]     = o0;
      out[i + 1] = o1;

      const uint32x4_t c0 = vreinterpretq_u32_s8(o0);
      const uint32x4_t c1 = vreinterpretq_u32_s8(o1);

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
INLINE size_t InputCReLU8(int8_t* outputs, uint16_t* nnz, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};
  const int max      = 127 << 5;

  (void) nnz; // the scalar L1 affine walks every input

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    int8_t* out     = &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < N_HIDDEN; i++)
      out[i] = Min(max, Max(0, in[i])) >> QUANT1_BITS;
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

INLINE void L1Affine(int32_t* dest, int8_t* src, const uint16_t* nnz, const size_t count) {
  const size_t OUT_WIDTH  = sizeof(__m512i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m512i* biases = (__m512i*) L1_BIASES;
  __m512i* out          = (__m512i*) dest;


  __m512i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m512i f0 = _mm512_set1_epi32(in32[i0]);
    const __m512i f1 = _mm512_set1_epi32(in32[i1]);

    const __m512i* c0 = (__m512i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m512i* c1 = (__m512i*) &L1_WEIGHTS[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m512i f0  = _mm512_set1_epi32(in32[i0]);
    const __m512i* c0 = (__m512i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = _mm512_srai_epi32(regs[i], QUANT1_BITS);
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

INLINE void L1Affine(int32_t* dest, int8_t* src, const uint16_t* nnz, const size_t count) {
  const size_t OUT_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m256i* biases = (__m256i*) L1_BIASES;
  __m256i* out          = (__m256i*) dest;


  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m256i f0 = _mm256_set1_epi32(in32[i0]);
    const __m256i f1 = _mm256_set1_epi32(in32[i1]);

    const __m256i* c0 = (__m256i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m256i* c1 = (__m256i*) &L1_WEIGHTS[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m256i f0  = _mm256_set1_epi32(in32[i0]);
    const __m256i* c0 = (__m256i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = _mm256_srai_epi32(regs[i], QUANT1_BITS);
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

INLINE void L1Affine(int32_t* dest, int8_t* src, const uint16_t* nnz, const size_t count) {
  const size_t OUT_WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m128i* biases = (__m128i*) L1_BIASES;
  __m128i* out          = (__m128i*) dest;


  __m128i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m128i f0 = _mm_set1_epi32(in32[i0]);
    const __m128i f1 = _mm_set1_epi32(in32[i1]);

    const __m128i* c0 = (__m128i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const __m128i* c1 = (__m128i*) &L1_WEIGHTS[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m128_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m128i f0  = _mm_set1_epi32(in32[i0]);
    const __m128i* c0 = (__m128i*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m128_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = _mm_srai_epi32(regs[i], QUANT1_BITS);
}
#elif defined(__ARM_NEON__)
INLINE void int8x16_add_dpbusd(int32x4_t* acc, int8x16_t a, int8x16_t b) {
  int16x8_t p0 = vmull_s8(vget_low_s8(a), vget_low_s8(b));
  int16x8_t p1 = vmull_high_s8(a, b);

  *acc = vpadalq_s16(*acc, vpaddq_s16(p0, p1));
}

INLINE void int8x16_add_dpbusd_x2(int32x4_t* acc, int8x16_t a0, int8x16_t b0, int8x16_t a1, int8x16_t b1) {
  int16x8_t p0 = vmull_s8(vget_low_s8(a0), vget_low_s8(b0));
  int16x8_t p1 = vmull_high_s8(a0, b0);
  int16x8_t p2 = vmull_s8(vget_low_s8(a1), vget_low_s8(b1));
  int16x8_t p3 = vmull_high_s8(a1, b1);

  *acc = vpadalq_s16(*acc, vaddq_s16(vpaddq_s16(p0, p1), vpaddq_s16(p2, p3)));
}

INLINE void L1Affine(int32_t* dest, int8_t* src, const uint16_t* nnz, const size_t count) {
  const size_t OUT_WIDTH  = 4;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32     = (int32_t*) src;
  const int32x4_t* biases = (int32x4_t*) L1_BIASES;
  int32x4_t* out          = (int32x4_t*) dest;


  int32x4_t regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const int8x16_t f0 = vreinterpretq_s8_u32(vdupq_n_u32(in32[i0]));
    const int8x16_t f1 = vreinterpretq_s8_u32(vdupq_n_u32(in32[i1]));

    const int8x16_t* c0 = (int8x16_t*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];
    const int8x16_t* c1 = (int8x16_t*) &L1_WEIGHTS[i1 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      int8x16_add_dpbusd_x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0   = nnz[i];
    const int8x16_t f0  = vreinterpretq_s8_u32(vdupq_n_u32(in32[i0]));
    const int8x16_t* c0 = (int8x16_t*) &L1_WEIGHTS[i0 * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      int8x16_add_dpbusd(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = vshrq_n_s32(regs[i], QUANT1_BITS);
}
#else
INLINE void L1Affine(int32_t* dest, int8_t* src, const uint16_t* nnz, const size_t count) {
  (void) nnz;
  (void) count;

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = L1_BIASES[i];

  for (size_t i = 0; i < N_L1; i++) {
    if (!src[i])
      continue;

    for (size_t j = 0; j < N_L2; j++)
      dest[j] += src[i] * L1_WEIGHTS[j * N_L1 + i];
  }

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = dest[i] >> QUANT1_BITS;
}
#endif

// The L2 weights are stored transposed into (input pair, output block) order,
// so a broadcast input pair accumulates straight into the output lanes. That
// removes the horizontal reduction the row-major layout needed.
#if defined(__AVX2__)
INLINE void L2Affine(int32_t* dest, int16_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t OUT_CHUNKS = N_L3 / OUT_WIDTH;
  const size_t IN_PAIRS   = N_L2 / 2;

  const int32_t* in      = (int32_t*) src;
  const __m256i* weights = (__m256i*) L2_WEIGHTS;
  const __m256i* biases  = (__m256i*) L2_BIASES;
  __m256i* out           = (__m256i*) dest;

  __m256i regs[OUT_CHUNKS];
  for (size_t i = 0; i < OUT_CHUNKS; i++)
    regs[i] = biases[i];

  for (size_t j = 0; j < IN_PAIRS; j++) {
    const __m256i f = _mm256_set1_epi32(in[j]);

    for (size_t i = 0; i < OUT_CHUNKS; i++)
      regs[i] = _mm256_add_epi32(regs[i], _mm256_madd_epi16(f, weights[j * OUT_CHUNKS + i]));
  }

  for (size_t i = 0; i < OUT_CHUNKS; i++)
    out[i] = _mm256_srai_epi32(regs[i], QUANT1_BITS);
}
#elif defined(__SSE4_1__)
INLINE void L2Affine(int32_t* dest, int16_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t OUT_CHUNKS = N_L3 / OUT_WIDTH;
  const size_t IN_PAIRS   = N_L2 / 2;

  const int32_t* in      = (int32_t*) src;
  const __m128i* weights = (__m128i*) L2_WEIGHTS;
  const __m128i* biases  = (__m128i*) L2_BIASES;
  __m128i* out           = (__m128i*) dest;

  __m128i regs[OUT_CHUNKS];
  for (size_t i = 0; i < OUT_CHUNKS; i++)
    regs[i] = biases[i];

  for (size_t j = 0; j < IN_PAIRS; j++) {
    const __m128i f = _mm_set1_epi32(in[j]);

    for (size_t i = 0; i < OUT_CHUNKS; i++)
      regs[i] = _mm_add_epi32(regs[i], _mm_madd_epi16(f, weights[j * OUT_CHUNKS + i]));
  }

  for (size_t i = 0; i < OUT_CHUNKS; i++)
    out[i] = _mm_srai_epi32(regs[i], QUANT1_BITS);
}
#elif defined(__ARM_NEON__)
INLINE void L2Affine(int32_t* dest, int16_t* src) {
  const size_t OUT_WIDTH  = 4;
  const size_t OUT_CHUNKS = N_L3 / OUT_WIDTH;
  const size_t IN_PAIRS   = N_L2 / 2;

  const int32_t* in        = (int32_t*) src;
  const int16x8_t* weights = (int16x8_t*) L2_WEIGHTS;
  const int32x4_t* biases  = (int32x4_t*) L2_BIASES;
  int32x4_t* out           = (int32x4_t*) dest;

  int32x4_t regs[OUT_CHUNKS];
  for (size_t i = 0; i < OUT_CHUNKS; i++)
    regs[i] = biases[i];

  for (size_t j = 0; j < IN_PAIRS; j++) {
    const int16x8_t f = vreinterpretq_s16_s32(vdupq_n_s32(in[j]));

    for (size_t i = 0; i < OUT_CHUNKS; i++) {
      int32x4_t p0 = vmull_s16(vget_low_s16(f), vget_low_s16(weights[j * OUT_CHUNKS + i]));
      int32x4_t p1 = vmull_high_s16(f, weights[j * OUT_CHUNKS + i]);
      regs[i]      = vaddq_s32(regs[i], vpaddq_s32(p0, p1));
    }
  }

  for (size_t i = 0; i < OUT_CHUNKS; i++)
    out[i] = vshrq_n_s32(regs[i], QUANT1_BITS);
}
#else
INLINE void L2Affine(int32_t* dest, int16_t* src) {
  for (int i = 0; i < N_L3; i++) {
    const int offset = i * N_L2;

    dest[i] = L2_BIASES[i];
    for (int j = 0; j < N_L2; j++)
      dest[i] += src[j] * L2_WEIGHTS[offset + j];

    dest[i] = dest[i] >> QUANT1_BITS;
  }
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE int32_t L3Transform(int16_t* src) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(int16_t);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m512i* in      = (__m512i*) src;
  const __m512i* weights = (__m512i*) OUTPUT_WEIGHTS;

  __m512i a0 = _mm512_setzero_si512();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm512_add_epi32(a0, _mm512_madd_epi16(in[i], weights[i]));

  const __m256i a8 = _mm256_add_epi32(_mm512_castsi512_si256(a0), _mm512_extracti64x4_epi64(a0, 1));
  const __m128i a4 = _mm_add_epi32(_mm256_castsi256_si128(a8), _mm256_extracti128_si256(a8, 1));
  const __m128i a2 = _mm_add_epi32(a4, _mm_shuffle_epi32(a4, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1) + OUTPUT_BIAS;
}
#elif defined(__AVX2__)
INLINE int32_t L3Transform(int16_t* src) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(int16_t);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) OUTPUT_WEIGHTS;

  __m256i a0 = _mm256_setzero_si256();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm256_add_epi32(a0, _mm256_madd_epi16(in[i], weights[i]));

  const __m128i a4 = _mm_add_epi32(_mm256_castsi256_si128(a0), _mm256_extracti128_si256(a0, 1));
  const __m128i a2 = _mm_add_epi32(a4, _mm_shuffle_epi32(a4, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1) + OUTPUT_BIAS;
}
#elif defined(__SSE4_1__)
INLINE int32_t L3Transform(int16_t* src) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(int16_t);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m128i* in      = (__m128i*) src;
  const __m128i* weights = (__m128i*) OUTPUT_WEIGHTS;

  __m128i a0 = _mm_setzero_si128();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm_add_epi32(a0, _mm_madd_epi16(in[i], weights[i]));

  const __m128i a2 = _mm_add_epi32(a0, _mm_shuffle_epi32(a0, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1) + OUTPUT_BIAS;
}
#elif defined(__ARM_NEON__)
INLINE int32_t L3Transform(int16_t* src) {
  const size_t WIDTH  = 8;
  const size_t CHUNKS = N_L3 / WIDTH;

  const int16x8_t* in      = (int16x8_t*) src;
  const int16x8_t* weights = (int16x8_t*) OUTPUT_WEIGHTS;

  int32x4_t a0 = {0};
  for (size_t i = 0; i < CHUNKS; i++) {
    int32x4_t p0 = vmull_s16(vget_low_s16(in[i]), vget_low_s16(weights[i]));
    int32x4_t p1 = vmull_high_s16(in[i], weights[i]);
    a0           = vaddq_s32(a0, vpaddq_s32(p0, p1));
  }

  return vaddvq_s32(a0) + OUTPUT_BIAS;
}
#else
INLINE int32_t L3Transform(int16_t* src) {
  int32_t result = OUTPUT_BIAS;

  for (int i = 0; i < N_L3; i++)
    result += src[i] * OUTPUT_WEIGHTS[i];

  return result;
}
#endif

#if defined(__AVX2__)
INLINE void ReLU16(int16_t* dest, int32_t* src, const size_t n) {
  const size_t IN_WIDTH = sizeof(__m256i) / sizeof(int32_t);
  const size_t CHUNKS   = n / IN_WIDTH;

  const __m256i* in = (__m256i*) src;
  __m256i* out      = (__m256i*) dest;

  for (size_t i = 0; i < CHUNKS / 2; i++) {
    const __m256i a0 = _mm256_permute4x64_epi64(_mm256_packs_epi32(in[2 * i], in[2 * i + 1]), 0b11011000);
    out[i]           = _mm256_max_epi16(a0, _mm256_setzero_si256());
  }
}
#elif defined(__SSE4_1__)
INLINE void ReLU16(int16_t* dest, int32_t* src, const size_t n) {
  const size_t IN_WIDTH = sizeof(__m128i) / sizeof(int32_t);
  const size_t CHUNKS   = n / IN_WIDTH;

  const __m128i* in = (__m128i*) src;
  __m128i* out      = (__m128i*) dest;

  for (size_t i = 0; i < CHUNKS / 2; i++) {
    const __m128i a0 = _mm_packs_epi32(in[2 * i], in[2 * i + 1]);
    out[i]           = _mm_max_epi16(a0, _mm_setzero_si128());
  }
}
#elif defined(__ARM_NEON__)
INLINE void ReLU16(int16_t* dest, int32_t* src, const size_t n) {
  const size_t IN_WIDTH = 4;
  const size_t CHUNKS   = n / IN_WIDTH;

  const int32x4_t* in = (int32x4_t*) src;
  int16x8_t* out      = (int16x8_t*) dest;

  const int16x8_t zero = {0};

  for (size_t i = 0; i < CHUNKS / 2; i++) {
    const int16x8_t a0 = vcombine_s16(vqmovn_s32(in[2 * i]), vqmovn_s32(in[2 * i + 1]));
    out[i]             = vmaxq_s16(a0, zero);
  }
}
#else
INLINE void ReLU16(int16_t* dest, int32_t* src, const size_t n) {
  for (size_t i = 0; i < n; i++)
    dest[i] = Max(0, src[i]);
}
#endif

INLINE int PropagateView(Accumulator* accumulator, const int stm) {
  int8_t x0[N_L1] ALIGN;
  // StoreNNZ always writes a full 8 entry group, so leave room for the tail.
  uint16_t nnz[N_L1 / SPARSE_CHUNK_SIZE + 8] ALIGN;
  int32_t dest[N_L3] ALIGN; // assumes N_L3 > N_L2
  int16_t act[N_L3] ALIGN;

  const size_t count = InputCReLU8(x0, nnz, accumulator, stm);
  L1Affine(dest, x0, nnz, count);
  ReLU16(act, dest, N_L2);
  L2Affine(dest, act);
  ReLU16(act, dest, N_L3);
  return L3Transform(act) >> QUANT2_BITS;
}

// Both call sites pass a literal side, so specialising on it lets the pair of
// perspective indices fold away instead of being read back from a local array.
int Propagate(Accumulator* accumulator, const int stm) {
  return stm == WHITE ? PropagateView(accumulator, WHITE) : PropagateView(accumulator, BLACK);
}

int Predict(Board* board) {
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  return board->stm == WHITE ? Propagate(board->accumulators, WHITE) : Propagate(board->accumulators, BLACK);
}

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN + // input weights
                            sizeof(int16_t) * N_HIDDEN +              // input biases
                            sizeof(int8_t) * N_L1 * N_L2 +            // input biases
                            sizeof(int32_t) * N_L2 +                  // input biases
                            sizeof(int16_t) * N_L2 * N_L3 +             // input biases
                            sizeof(int32_t) * N_L3 +                    // input biases
                            sizeof(int16_t) * N_L3 +                    // output weights
                            sizeof(int32_t);                            // output bias

#if defined(__SSE4_1__) || defined(__ARM_NEON__)
INLINE int WeightIdxScrambled(int idx) {
  return ((idx / SPARSE_CHUNK_SIZE) % (N_L1 / SPARSE_CHUNK_SIZE) * N_L2 * SPARSE_CHUNK_SIZE) +
         (idx / N_L1 * SPARSE_CHUNK_SIZE) + (idx % SPARSE_CHUNK_SIZE);
}

// Outputs held per vector by L2Affine.
#if defined(__AVX2__)
#define L2_OUT_WIDTH 8
#else
#define L2_OUT_WIDTH 4
#endif

// Row major [output][input] becomes [input pair][output block][output][pair].
INLINE int L2WeightIdxScrambled(int idx) {
  const int o = idx / N_L2;
  const int k = idx % N_L2;

  const int outChunks = N_L3 / L2_OUT_WIDTH;

  return ((k / 2) * outChunks + o / L2_OUT_WIDTH) * (2 * L2_OUT_WIDTH) + (o % L2_OUT_WIDTH) * 2 + (k % 2);
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

  memcpy(l1, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
  offset += N_L1 * N_L2 * sizeof(int8_t);
  memcpy(L1_BIASES, &in[offset], N_L2 * sizeof(int32_t));
  offset += N_L2 * sizeof(int32_t);

  int16_t l2[N_L2 * N_L3];
  memcpy(l2, &in[offset], N_L2 * N_L3 * sizeof(int16_t));
  offset += N_L2 * N_L3 * sizeof(int16_t);

#if defined(__SSE4_1__) || defined(__ARM_NEON__)
  for (int i = 0; i < N_L2 * N_L3; i++)
    L2_WEIGHTS[L2WeightIdxScrambled(i)] = l2[i];
#else
  memcpy(L2_WEIGHTS, l2, N_L2 * N_L3 * sizeof(int16_t));
#endif
  memcpy(L2_BIASES, &in[offset], N_L3 * sizeof(int32_t));
  offset += N_L3 * sizeof(int32_t);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L3 * N_OUTPUT * sizeof(int16_t));
  offset += N_L3 * N_OUTPUT * sizeof(int16_t);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(int32_t));

#if defined(__SSE4_1__) || defined(__ARM_NEON__)
  // Shuffle the L1 weights for sparse matmul
  for (int i = 0; i < N_L1 * N_L2; i++)
    L1_WEIGHTS[WeightIdxScrambled(i)] = l1[i];
#else
  for (int i = 0; i < N_L1 * N_L2; i++)
    L1_WEIGHTS[i] = l1[i];
#endif

  free(l1);

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
