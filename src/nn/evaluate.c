// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

#include <immintrin.h>
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

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

float L2_WEIGHTS[N_L2 * N_L3] ALIGN;
float L2_BIASES[N_L3] ALIGN;

float OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
float OUTPUT_BIAS;

uint16_t LOOKUP_INDICES[256][8] ALIGN;

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m512i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m512i* in = (__m512i*) acc->values[views[v]];
    __m512i* out      = (__m512i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m512i s0 = _mm512_srai_epi16(in[2 * i + 0], 6);
      __m512i s1 = _mm512_srai_epi16(in[2 * i + 1], 6);
      __m512i s2 = _mm512_srai_epi16(in[2 * i + 2], 6);
      __m512i s3 = _mm512_srai_epi16(in[2 * i + 3], 6);

      out[i]     = _mm512_max_epi8(_mm512_packs_epi16(s0, s1), _mm512_setzero_si512());
      out[i + 1] = _mm512_max_epi8(_mm512_packs_epi16(s2, s3), _mm512_setzero_si512());
    }
  }
}
#elif defined(__AVX2__)
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m256i s0 = _mm256_srai_epi16(in[2 * i + 0], 6);
      __m256i s1 = _mm256_srai_epi16(in[2 * i + 1], 6);
      __m256i s2 = _mm256_srai_epi16(in[2 * i + 2], 6);
      __m256i s3 = _mm256_srai_epi16(in[2 * i + 3], 6);

      out[i]     = _mm256_max_epi8(_mm256_packs_epi16(s0, s1), _mm256_setzero_si256());
      out[i + 1] = _mm256_max_epi8(_mm256_packs_epi16(s2, s3), _mm256_setzero_si256());
    }
  }
}
#elif defined(__SSE4_1__)
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i++) {
      __m128i s0 = _mm_srai_epi16(in[2 * i + 0], 6);
      __m128i s1 = _mm_srai_epi16(in[2 * i + 1], 6);

      out[i] = _mm_max_epi8(_mm_packs_epi16(s0, s1), _mm_setzero_si128());
    }
  }
}
#else
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};
  const int max      = 127 << 6;

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    int8_t* out     = &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < N_HIDDEN; i++)
      out[i] = Min(max, Max(0, in[i])) >> 6;
  }
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE void m512_add_dpbusd_epi32(__m512i* acc, __m512i a, __m512i b) {
  __m512i p0 = _mm512_maddubs_epi16(a, b);
  p0         = _mm512_madd_epi16(p0, _mm512_set1_epi16(1));
  *acc       = _mm512_add_epi32(*acc, p0);
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

INLINE void L1AffineReLU(float* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m512i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m512i* biases = (__m512i*) L1_BIASES;
  __m512* out           = (__m512*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

  __m512i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  for (size_t i = 0; i < count; i++) {
    const uint16_t inputId = nnz[i];
    const __m512i factor   = _mm512_set1_epi32(in32[inputId]);
    const __m512i* col     = (__m512i*) &L1_WEIGHTS[inputId * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m512_add_dpbusd_epi32(regs + j, factor, col[j]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = _mm512_cvtepi32_ps(_mm512_max_epi32(regs[i], _mm512_setzero_si512()));
}
#elif defined(__AVX2__)
INLINE void m256_add_dpbusd_epi32(__m256i* acc, __m256i a, __m256i b) {
  __m256i p0 = _mm256_maddubs_epi16(a, b);
  p0         = _mm256_madd_epi16(p0, _mm256_set1_epi16(1));
  *acc       = _mm256_add_epi32(*acc, p0);
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

INLINE void L1AffineReLU(float* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m256i* biases = (__m256i*) L1_BIASES;
  __m256* out           = (__m256*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  for (size_t i = 0; i < count; i++) {
    const uint16_t inputId = nnz[i];
    const __m256i factor   = _mm256_set1_epi32(in32[inputId]);
    const __m256i* col     = (__m256i*) &L1_WEIGHTS[inputId * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, factor, col[j]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = _mm256_cvtepi32_ps(_mm256_max_epi32(regs[i], _mm256_setzero_si256()));
}

#elif defined(__SSSE3__)
INLINE void m128_add_dpbusd_epi32(__m128i* acc, __m128i a, __m128i b) {
  __m128i p0 = _mm_maddubs_epi16(a, b);
  p0         = _mm_madd_epi16(p0, _mm_set1_epi16(1));
  *acc       = _mm_add_epi32(*acc, p0);
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

INLINE void L1AffineReLU(float* dest, int8_t* src) {
  const size_t OUT_WIDTH  = sizeof(__m128i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = N_L1 / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = N_L2 / OUT_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m128i* biases = (__m128i*) L1_BIASES;
  __m128* out           = (__m128*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

  __m128i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  for (size_t i = 0; i < count; i++) {
    const uint16_t inputId = nnz[i];
    const __m128i factor   = _mm_set1_epi32(in32[inputId]);
    const __m128i* col     = (__m128i*) &L1_WEIGHTS[inputId * N_L2 * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m128_add_dpbusd_epi32(regs + j, factor, col[j]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = _mm_cvtepi32_ps(_mm_max_epi32(regs[i], _mm_setzero_si128()));
}
#else
INLINE void L1AffineReLU(float* dest, int8_t* src) {
  for (size_t i = 0; i < N_L2; i++)
    dest[i] = L1_BIASES[i];

  for (size_t i = 0; i < N_L1; i++) {
    if (!src[i])
      continue;

    for (size_t j = 0; j < N_L2; j++)
      dest[j] += src[i] * L1_WEIGHTS[j * N_L1 + i];
  }

  for (size_t i = 0; i < N_L2; i++)
    dest[i] = Max(0, dest[i]);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE __m128 m512_hadd_psx4(__m512* regs) {
  __m512 regs01l = _mm512_unpacklo_ps(regs[0], regs[1]);
  __m512 regs01h = _mm512_unpackhi_ps(regs[0], regs[1]);

  __m512 regs23l = _mm512_unpacklo_ps(regs[2], regs[3]);
  __m512 regs23h = _mm512_unpackhi_ps(regs[2], regs[3]);

  __m512 regs01 = _mm512_add_ps(regs01l, regs01h);
  __m512 regs23 = _mm512_add_ps(regs23l, regs23h);

  __m512 regs0123l = _mm512_castpd_ps(_mm512_unpacklo_pd(_mm512_castps_pd(regs01), _mm512_castps_pd(regs23)));
  __m512 regs0123h = _mm512_castpd_ps(_mm512_unpackhi_pd(_mm512_castps_pd(regs01), _mm512_castps_pd(regs23)));

  __m512 sum = _mm512_add_ps(regs0123l, regs0123h);

  __m256 sum256lo = _mm512_castps512_ps256(sum);
  __m256 sum256hi = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(sum), 1));

  sum256lo = _mm256_add_ps(sum256lo, sum256hi);

  __m128 sum128lo = _mm256_castps256_ps128(sum256lo);
  __m128 sum128hi = _mm256_extractf128_ps(sum256lo, 1);

  return _mm_add_ps(sum128lo, sum128hi);
}

INLINE void L2AffineReLU(float* dest, float* src) {
  const size_t IN_WIDTH   = sizeof(__m512) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8; // 16 is possible, but slower.
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m512* in      = (__m512*) src;
  const __m512* weights = (__m512*) L2_WEIGHTS;
  const __m256* biases  = (__m256*) L2_BIASES;
  __m256* out           = (__m256*) dest;

  __m512 regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm512_setzero_ps();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm512_fmadd_ps(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)], regs[k]);

    const __m128 s0  = m512_hadd_psx4(regs);
    const __m128 s1  = m512_hadd_psx4(&regs[4]);
    const __m256 sum = _mm256_insertf128_ps(_mm256_castps128_ps256(s0), s1, 1);
    out[i]           = _mm256_max_ps(_mm256_add_ps(sum, biases[i]), _mm256_setzero_ps());
  }
}

#elif defined(__AVX2__)
INLINE __m128 m256_hadd_psx4(__m256* regs) {
  regs[0] = _mm256_hadd_ps(regs[0], regs[1]);
  regs[2] = _mm256_hadd_ps(regs[2], regs[3]);

  regs[0] = _mm256_hadd_ps(regs[0], regs[2]);

  __m128 sum128lo = _mm256_castps256_ps128(regs[0]);
  __m128 sum128hi = _mm256_extractf128_ps(regs[0], 1);

  return _mm_add_ps(sum128lo, sum128hi);
}

INLINE void L2AffineReLU(float* dest, float* src) {
  const size_t IN_WIDTH   = sizeof(__m256) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) L2_WEIGHTS;
  const __m256* biases  = (__m256*) L2_BIASES;
  __m256* out           = (__m256*) dest;

  __m256 regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm256_setzero_ps();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm256_fmadd_ps(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)], regs[k]);

    const __m128 s0  = m256_hadd_psx4(regs);
    const __m128 s1  = m256_hadd_psx4(&regs[4]);
    const __m256 sum = _mm256_insertf128_ps(_mm256_castps128_ps256(s0), s1, 1);
    out[i]           = _mm256_max_ps(_mm256_add_ps(sum, biases[i]), _mm256_setzero_ps());
  }
}
#elif defined(__SSE3__)
INLINE __m128 m128_hadd_psx4(__m128* regs) {
  regs[0] = _mm_hadd_ps(regs[0], regs[1]);
  regs[2] = _mm_hadd_ps(regs[2], regs[3]);

  return _mm_hadd_ps(regs[0], regs[2]);
}

INLINE void L2AffineReLU(float* dest, float* src) {
  const size_t IN_WIDTH   = sizeof(__m128) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 4;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m128* in      = (__m128*) src;
  const __m128* weights = (__m128*) L2_WEIGHTS;
  const __m128* biases  = (__m128*) L2_BIASES;
  __m128* out           = (__m128*) dest;

  __m128 regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm_setzero_ps();

    for (size_t j = 0; j < IN_CHUNKS; j++)
      for (size_t k = 0; k < OUT_CC; k++)
        regs[k] = _mm_add_ps(regs[k], _mm_mul_ps(in[j], weights[j + IN_CHUNKS * (OUT_CC * i + k)]));

    const __m128 sum = m128_hadd_psx4(regs);
    out[i]           = _mm_max_ps(_mm_add_ps(sum, biases[i]), _mm_setzero_ps());
  }
}
#else
INLINE void L2AffineReLU(float* dest, float* src) {
  for (int i = 0; i < N_L3; i++) {
    const int offset = i * N_L2;

    dest[i] = L2_BIASES[i];
    for (int j = 0; j < N_L2; j++)
      dest[i] += src[j] * L2_WEIGHTS[offset + j];

    dest[i] = Max(0, dest[i]);
  }
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
INLINE int L3Transform(float* src) {
  const size_t WIDTH  = sizeof(__m512) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m512* in      = (__m512*) src;
  const __m512* weights = (__m512*) OUTPUT_WEIGHTS;

  __m512 a0 = _mm512_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm512_fmadd_ps(in[i], weights[i], a0);

  const __m256 a8 =
    _mm256_add_ps(_mm512_castps512_ps256(a0), _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(a0), 1)));
  const __m128 a4 = _mm_add_ps(_mm256_castps256_ps128(a8), _mm256_extractf128_ps(a8, 1));
  const __m128 a2 = _mm_add_ps(a4, _mm_movehl_ps(a4, a4));
  const __m128 a1 = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + OUTPUT_BIAS;
}
#elif defined(__AVX2__)
INLINE int L3Transform(float* src) {
  const size_t WIDTH  = sizeof(__m256) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) OUTPUT_WEIGHTS;

  __m256 a0 = _mm256_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm256_fmadd_ps(in[i], weights[i], a0);

  const __m128 a4 = _mm_add_ps(_mm256_castps256_ps128(a0), _mm256_extractf128_ps(a0, 1));
  const __m128 a2 = _mm_add_ps(a4, _mm_movehl_ps(a4, a4));
  const __m128 a1 = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + OUTPUT_BIAS;
}
#elif defined(__SSE__)
INLINE float L3Transform(float* src) {
  const size_t WIDTH  = sizeof(__m128) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m128* in      = (__m128*) src;
  const __m128* weights = (__m128*) OUTPUT_WEIGHTS;

  __m128 a0 = _mm_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm_add_ps(a0, _mm_mul_ps(in[i], weights[i]));

  const __m128 a2 = _mm_add_ps(a0, _mm_movehl_ps(a0, a0));
  const __m128 a1 = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + OUTPUT_BIAS;
}
#else
INLINE float L3Transform(float* src) {
  float result = OUTPUT_BIAS;

  for (int i = 0; i < N_L3; i++)
    result += src[i] * OUTPUT_WEIGHTS[i];

  return result;
}
#endif

int Propagate(Accumulator* accumulator, const int stm) {
  int8_t x0[N_L1] ALIGN;
  float x1[N_L2] ALIGN;
  float x2[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);
  L1AffineReLU(x1, x0);
  L2AffineReLU(x2, x1);
  return L3Transform(x2) / 32;
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
                            sizeof(float) * N_L2 * N_L3 +             // input biases
                            sizeof(float) * N_L3 +                    // input biases
                            sizeof(float) * N_L3 +                    // output weights
                            sizeof(float);                            // output bias

INLINE int WeightIdxScrambled(int idx) {
  return ((idx / SPARSE_CHUNK_SIZE) % (N_L1 / SPARSE_CHUNK_SIZE) * N_L2 * SPARSE_CHUNK_SIZE) +
         (idx / N_L1 * SPARSE_CHUNK_SIZE) + (idx % SPARSE_CHUNK_SIZE);
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

  memcpy(l1, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
  offset += N_L1 * N_L2 * sizeof(int8_t);
  memcpy(L1_BIASES, &in[offset], N_L2 * sizeof(int32_t));
  offset += N_L2 * sizeof(int32_t);

  memcpy(L2_WEIGHTS, &in[offset], N_L2 * N_L3 * sizeof(float));
  offset += N_L2 * N_L3 * sizeof(float);
  memcpy(L2_BIASES, &in[offset], N_L3 * sizeof(float));
  offset += N_L3 * sizeof(float);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L3 * N_OUTPUT * sizeof(float));
  offset += N_L3 * N_OUTPUT * sizeof(float);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(float));

#if defined(__SSSE3__)
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
