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

const int QUANT_BITS = 5;

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

float L2_WEIGHTS_F[N_L2 * N_L3] ALIGN;
float L2_BIASES_F[N_L3] ALIGN;

float OUTPUT_WEIGHTS_F[N_L3 * N_OUTPUT] ALIGN;
float OUTPUT_BIAS_F;

int16_t L2_WEIGHTS[N_L2 * N_L3] ALIGN;
int32_t L2_BIASES[N_L3] ALIGN;

int16_t OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
int32_t OUTPUT_BIAS;

uint16_t LOOKUP_INDICES[256][8] ALIGN;

#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m512i* in = (__m512i*) acc->values[views[v]];
    __m512i* out      = (__m512i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m512i s0 = _mm512_srai_epi16(in[2 * i + 0], QUANT_BITS);
      __m512i s1 = _mm512_srai_epi16(in[2 * i + 1], QUANT_BITS);
      __m512i s2 = _mm512_srai_epi16(in[2 * i + 2], QUANT_BITS);
      __m512i s3 = _mm512_srai_epi16(in[2 * i + 3], QUANT_BITS);

      out[i]     = _mm512_max_epi8(_mm512_packs_epi16(s0, s1), _mm512_setzero_si512());
      out[i + 1] = _mm512_max_epi8(_mm512_packs_epi16(s2, s3), _mm512_setzero_si512());
    }
  }
}
#elif defined(__AVX2__)
#include <immintrin.h>
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m256i s0 = _mm256_srai_epi16(in[2 * i + 0], QUANT_BITS);
      __m256i s1 = _mm256_srai_epi16(in[2 * i + 1], QUANT_BITS);
      __m256i s2 = _mm256_srai_epi16(in[2 * i + 2], QUANT_BITS);
      __m256i s3 = _mm256_srai_epi16(in[2 * i + 3], QUANT_BITS);

      out[i]     = _mm256_max_epi8(_mm256_packs_epi16(s0, s1), _mm256_setzero_si256());
      out[i + 1] = _mm256_max_epi8(_mm256_packs_epi16(s2, s3), _mm256_setzero_si256());
    }
  }
}
#elif defined(__SSE2__)
#include <immintrin.h>
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  const __m128i k0x80s = _mm_set1_epi8(-128);

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i++) {
      __m128i s0 = _mm_srai_epi16(in[2 * i + 0], QUANT_BITS);
      __m128i s1 = _mm_srai_epi16(in[2 * i + 1], QUANT_BITS);

      out[i] = _mm_subs_epi8(_mm_adds_epi8(_mm_packs_epi16(s0, s1), k0x80s), k0x80s);
    }
  }
}
#else
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};
  const int max      = 127 << 5;

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    int8_t* out     = &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < N_HIDDEN; i++)
      out[i] = Min(max, Max(0, in[i] >> QUANT_BITS));
  }
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

INLINE uint32_t NNZ(__m512i chunk) {
  return _mm512_cmpgt_epi32_mask(chunk, _mm512_setzero_si512());
}

INLINE size_t FindNNZ(uint16_t* dest, const int32_t* inputs, const size_t chunks) {
  const size_t IN_WIDTH      = sizeof(__m512i) / sizeof(int32_t);
  const size_t CHUNK_SIZE    = 16;
  const size_t NUM_CHUNKS    = chunks / CHUNK_SIZE;
  const size_t IN_PER_CHUNK  = CHUNK_SIZE / IN_WIDTH;
  const size_t OUT_PER_CHUNK = CHUNK_SIZE / 8;

  const __m512i* in = (__m512i*) inputs;

  size_t count = 0;

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();

  for (size_t i = 0; i < NUM_CHUNKS; i++) {
    uint32_t nnz = 0;

    for (size_t j = 0; j < IN_PER_CHUNK; j++) {
      const __m512i inputChunk = in[i * IN_PER_CHUNK + j];
      nnz |= NNZ(inputChunk) << (j * IN_WIDTH);
    }

    for (size_t j = 0; j < OUT_PER_CHUNK; j++) {
      const uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
      const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
      _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(base, offsets));
      count += BitCount(lookup);
      base = _mm_add_epi16(base, increment);
    }
  }

  return count;
}

INLINE void L1AffineReLU(int32_t* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m512i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m512i* biases = (__m512i*) L1_BIASES;
  __m512i* out          = (__m512i*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

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
    out[i] = _mm512_max_epi32(_mm512_srai_epi32(regs[i], QUANT_BITS), _mm512_setzero_si512());
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

INLINE uint32_t NNZ(__m256i chunk) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
}

INLINE size_t FindNNZ(uint16_t* dest, const int32_t* inputs, const size_t chunks) {
  const size_t IN_WIDTH      = sizeof(__m256i) / sizeof(int32_t);
  const size_t CHUNK_SIZE    = IN_WIDTH;
  const size_t NUM_CHUNKS    = chunks / CHUNK_SIZE;
  const size_t IN_PER_CHUNK  = CHUNK_SIZE / IN_WIDTH;
  const size_t OUT_PER_CHUNK = CHUNK_SIZE / 8;

  const __m256i* in = (__m256i*) inputs;

  size_t count = 0;

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();

  for (size_t i = 0; i < NUM_CHUNKS; i++) {
    uint32_t nnz = 0;

    for (size_t j = 0; j < IN_PER_CHUNK; j++) {
      const __m256i inputChunk = in[i * IN_PER_CHUNK + j];
      nnz |= NNZ(inputChunk) << (j * IN_WIDTH);
    }

    for (size_t j = 0; j < OUT_PER_CHUNK; j++) {
      const uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
      const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
      _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(base, offsets));
      count += BitCount(lookup);
      base = _mm_add_epi16(base, increment);
    }
  }

  return count;
}

INLINE void L1Affine(int32_t* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m256i* biases = (__m256i*) L1_BIASES;
  __m256i* out          = (__m256i*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

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
    out[i] = _mm256_srai_epi32(regs[i], QUANT_BITS);
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

INLINE uint32_t NNZ(__m128i chunk) {
  return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(chunk, _mm_setzero_si128())));
}

INLINE size_t FindNNZ(uint16_t* dest, const int32_t* inputs, const size_t chunks) {
  const size_t IN_WIDTH      = sizeof(__m128i) / sizeof(int32_t);
  const size_t CHUNK_SIZE    = 8;
  const size_t NUM_CHUNKS    = chunks / CHUNK_SIZE;
  const size_t IN_PER_CHUNK  = CHUNK_SIZE / IN_WIDTH;
  const size_t OUT_PER_CHUNK = CHUNK_SIZE / 8;

  const __m128i* in = (__m128i*) inputs;

  size_t count = 0;

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();

  for (size_t i = 0; i < NUM_CHUNKS; i++) {
    uint32_t nnz = 0;

    for (size_t j = 0; j < IN_PER_CHUNK; j++) {
      const __m128i inputChunk = in[i * IN_PER_CHUNK + j];
      nnz |= NNZ(inputChunk) << (j * IN_WIDTH);
    }

    for (size_t j = 0; j < OUT_PER_CHUNK; j++) {
      const uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
      const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
      _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(base, offsets));
      count += BitCount(lookup);
      base = _mm_add_epi16(base, increment);
    }
  }

  return count;
}

INLINE void L1AffineReLU(int32_t* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m128i* biases = (__m128i*) L1_BIASES;
  __m128i* out          = (__m128i*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

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
    out[i] = _mm_max_epi32(_mm_srai_epi32(regs[i], QUANT_BITS), _mm_setzero_si128());
}
#else
INLINE void L1AffineReLU(int32_t* dest, int8_t* src) {
  for (size_t i = 0; i < N_L2; i++)
    dest[i] = L1_BIASES[i];

  for (size_t i = 0; i < N_L1; i++) {
    if (!src[i])
      continue;

    for (size_t j = 0; j < N_L2; j++)
      dest[j] += src[i] * L1_WEIGHTS[j * N_L1 + i];
  }

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = Max(0, dest[i] >> QUANT_BITS);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE __m128i m512_hadd_epi32x4(__m512i* regs) {
  __m512i regs01l = _mm512_unpacklo_epi32(regs[0], regs[1]);
  __m512i regs01h = _mm512_unpackhi_epi32(regs[0], regs[1]);

  __m512i regs23l = _mm512_unpacklo_epi32(regs[2], regs[3]);
  __m512i regs23h = _mm512_unpackhi_epi32(regs[2], regs[3]);

  __m512i regs01 = _mm512_add_epi32(regs01l, regs01h);
  __m512i regs23 = _mm512_add_epi32(regs23l, regs23h);

  __m512i regs0123l = _mm512_unpacklo_epi32(regs01, regs23);
  __m512i regs0123h = _mm512_unpackhi_epi32(regs01, regs23);

  __m512i sum = _mm512_add_epi32(regs0123l, regs0123h);

  __m256i sum256lo = _mm512_castsi512_si256(sum);
  __m256i sum256hi = _mm512_extracti64x4_epi64(sum, 1);

  sum256lo = _mm256_add_epi32(sum256lo, sum256hi);

  __m128i sum128lo = _mm256_castsi256_si128(sum256lo);
  __m128i sum128hi = _mm256_extractf128_si256(sum256lo, 1);

  return _mm_add_epi32(sum128lo, sum128hi);
}

INLINE void L2AffineReLU(int32_t* dest, int32_t* src) {
  const size_t IN_WIDTH   = sizeof(__m512i) / sizeof(int32_t);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8; // 16 is possible, but slower.
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m512i* in      = (__m512i*) src;
  const __m512i* weights = (__m512i*) L2_WEIGHTS;
  const __m256i* biases  = (__m256i*) L2_BIASES;
  __m256i* out           = (__m256i*) dest;

  __m512i regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm512_setzero_si512();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm512_add_epi32(regs[k], _mm512_mullo_epi32(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)]));

    const __m128i s0      = m512_hadd_epi32x4(regs);
    const __m128i s1      = m512_hadd_epi32x4(&regs[4]);
    const __m256i sum     = _mm256_insertf128_si256(_mm256_castsi128_si256(s0), s1, 1);
    const __m256i shifted = _mm256_srai_epi32(_mm256_add_epi32(sum, biases[i]), QUANT_BITS);
    out[i]                = _mm256_max_epi32(shifted, _mm256_setzero_si256());
  }
}

#elif defined(__AVX2__)
INLINE __m128i m256_hadd_epi32x4(__m256i* regs) {
  regs[0] = _mm256_hadd_epi32(regs[0], regs[1]);
  regs[2] = _mm256_hadd_epi32(regs[2], regs[3]);

  regs[0] = _mm256_hadd_epi32(regs[0], regs[2]);

  __m128i sum128lo = _mm256_castsi256_si128(regs[0]);
  __m128i sum128hi = _mm256_extractf128_si256(regs[0], 1);

  return _mm_add_epi32(sum128lo, sum128hi);
}

INLINE void L2Affine(int32_t* dest, int16_t* src) {
  const size_t IN_WIDTH   = sizeof(__m256i) / sizeof(int16_t);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) L2_WEIGHTS;
  const __m256i* biases  = (__m256i*) L2_BIASES;
  __m256i* out           = (__m256i*) dest;

  __m256i regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm256_setzero_si256();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm256_add_epi32(regs[k], _mm256_madd_epi16(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)]));

    const __m128i s0  = m256_hadd_epi32x4(regs);
    const __m128i s1  = m256_hadd_epi32x4(&regs[4]);
    const __m256i sum = _mm256_insertf128_si256(_mm256_castsi128_si256(s0), s1, 1);
    out[i]            = _mm256_srai_epi32(_mm256_add_epi32(sum, biases[i]), QUANT_BITS);
  }
}
#elif defined(__SSE4_1__)
INLINE __m128i m128_hadd_epi32x4(__m128i* regs) {
  regs[0] = _mm_hadd_epi32(regs[0], regs[1]);
  regs[2] = _mm_hadd_epi32(regs[2], regs[3]);

  return _mm_hadd_epi32(regs[0], regs[2]);
}

INLINE void L2AffineReLU(int32_t* dest, int32_t* src) {
  const size_t IN_WIDTH   = sizeof(__m128i) / sizeof(int32_t);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 4;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m128i* in      = (__m128i*) src;
  const __m128i* weights = (__m128i*) L2_WEIGHTS;
  const __m128i* biases  = (__m128i*) L2_BIASES;
  __m128i* out           = (__m128i*) dest;

  __m128i regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm_setzero_si128();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm_add_epi32(regs[k], _mm_mullo_epi32(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)]));

    const __m128i sum     = m128_hadd_epi32x4(regs);
    const __m128i shifted = _mm_srai_epi32(_mm_add_epi32(sum, biases[i]), QUANT_BITS);
    out[i]                = _mm_max_epi32(shifted, _mm_setzero_si128());
  }
}
#else
INLINE void L2Affine(int32_t* dest, int16_t* src) {
  for (int i = 0; i < N_L3; i++) {
    const int offset = i * N_L2;

    dest[i] = L2_BIASES[i];
    for (int j = 0; j < N_L2; j++)
      dest[i] += src[j] * L2_WEIGHTS_S[offset + j];

    dest[i] = dest[i] >> QUANT_BITS;
  }
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE int32_t L3Transform(int32_t* src) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(int32_t);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m512i* in      = (__m512i*) src;
  const __m512i* weights = (__m512i*) OUTPUT_WEIGHTS;

  __m512i a0 = _mm512_setzero_si512();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm512_add_epi32(a0, _mm512_mullo_epi32(in[i], weights[i]));

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
INLINE int32_t L3Transform(int32_t* src) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m128i* in      = (__m128i*) src;
  const __m128i* weights = (__m128i*) OUTPUT_WEIGHTS;

  __m128i a0 = _mm_setzero_si128();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm_add_epi32(a0, _mm_mullo_epi32(in[i], weights[i]));

  const __m128i a2 = _mm_add_epi32(a0, _mm_shuffle_epi32(a0, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1) + OUTPUT_BIAS;
}
#else
INLINE int32_t L3Transform(int16_t* src) {
  int32_t result = OUTPUT_BIAS;

  for (int i = 0; i < N_L3; i++)
    result += src[i] * OUTPUT_WEIGHTS_S[i];

  return result;
}
#endif

INLINE void ReLU16(int16_t* dest, int32_t* src, const size_t n) {
  const size_t IN_WIDTH = sizeof(__m256i) / sizeof(int32_t);
  const size_t CHUNKS   = n / IN_WIDTH;

  const __m256i* in = (__m256i*) src;
  __m256i* out      = (__m256i*) dest;

  for (size_t i = 0; i < CHUNKS / 2; i++) {
    const __m256i a0 = _mm256_permute4x64_epi64(_mm256_packs_epi32(in[2 * i], in[2 * i + 1]), 0b11011000);
    out[i] = _mm256_max_epi16(a0, _mm256_setzero_si256());
  }
}

int Propagate(Accumulator* accumulator, const int stm) {
  int8_t x0[N_L1] ALIGN;
  int32_t x1[N_L2] ALIGN;
  int16_t x1a[N_L2] ALIGN;
  int32_t x2[N_L3] ALIGN;
  int16_t x2a[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);
  L1Affine(x1, x0);
  ReLU16(x1a, x1, N_L2);
  L2Affine(x2, x1a);
  ReLU16(x2a, x2, N_L3);
  return L3Transform(x2a) >> QUANT_BITS;
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
                            sizeof(float) * N_L2 * N_L3 +           // input biases
                            sizeof(float) * N_L3 +                  // input biases
                            sizeof(float) * N_L3 +                  // output weights
                            sizeof(float);                          // output bias

#if defined(__SSE4_1__)
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

  memcpy(l1, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
  offset += N_L1 * N_L2 * sizeof(int8_t);
  memcpy(L1_BIASES, &in[offset], N_L2 * sizeof(int32_t));
  offset += N_L2 * sizeof(int32_t);

  memcpy(L2_WEIGHTS_F, &in[offset], N_L2 * N_L3 * sizeof(float));
  offset += N_L2 * N_L3 * sizeof(float);
  memcpy(L2_BIASES_F, &in[offset], N_L3 * sizeof(float));
  offset += N_L3 * sizeof(float);

  memcpy(OUTPUT_WEIGHTS_F, &in[offset], N_L3 * N_OUTPUT * sizeof(float));
  offset += N_L3 * N_OUTPUT * sizeof(float);
  memcpy(&OUTPUT_BIAS_F, &in[offset], sizeof(float));

  for (size_t i = 0; i < N_L2 * N_L3; i++)
    L2_WEIGHTS[i] = roundf(L2_WEIGHTS_F[i] * 32.0);
  for (size_t i = 0; i < N_L3; i++)
    L2_BIASES[i] = roundf(L2_BIASES_F[i]);
  for (size_t i = 0; i < N_L3 * N_OUTPUT; i++)
    OUTPUT_WEIGHTS[i] = roundf(OUTPUT_WEIGHTS_F[i] * 32.0);
  OUTPUT_BIAS = roundf(OUTPUT_BIAS_F);

#if defined(__SSE4_1__)
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
