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

#if defined(__AVX2__)
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
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

      out[i]     = _mm256_packus_epi16(s0, s1);
      out[i + 1] = _mm256_packus_epi16(s2, s3);
    }
  }
}
#elif defined(__SSE2__)
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i++) {
      __m128i s0 = _mm_srai_epi16(in[2 * i + 0], 6);
      __m128i s1 = _mm_srai_epi16(in[2 * i + 1], 6);

      out[i] = _mm_packus_epi16(s0, s1);
    }
  }
}
#else
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    uint8_t* out    = &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < N_HIDDEN; i++)
      out[i] = Max(0, in[i]) >> 6;
  }
}
#endif

#if defined(__AVX2__)
INLINE void m256_add_dpbusd_epi32x4(__m256i* acc, const __m256i* inputs, const __m256i* weights) {
  __m256i tmp0 = _mm256_maddubs_epi16(inputs[0], weights[0]);
  __m256i tmp1 = _mm256_maddubs_epi16(inputs[1], weights[1]);
  __m256i tmp2 = _mm256_maddubs_epi16(inputs[2], weights[2]);
  __m256i tmp3 = _mm256_maddubs_epi16(inputs[3], weights[3]);

  tmp0 = _mm256_add_epi16(tmp0, _mm256_add_epi16(tmp1, _mm256_add_epi16(tmp2, tmp3)));
  *acc = _mm256_add_epi32(*acc, _mm256_madd_epi16(tmp0, _mm256_set1_epi16(1)));
}

INLINE void m256_hadd_epi32x4(__m256i* regs) {
  regs[0] = _mm256_hadd_epi32(regs[0], regs[1]);
  regs[2] = _mm256_hadd_epi32(regs[2], regs[3]);
  regs[0] = _mm256_hadd_epi32(regs[0], regs[2]);
}

INLINE void L1AffineReLU(float* dest, uint8_t* src) {
  const size_t IN_WIDTH   = sizeof(__m256i) / sizeof(uint8_t);
  const size_t IN_CHUNKS  = N_L1 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L2 / OUT_CC;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) L1_WEIGHTS;
  const __m256i* biases  = (__m256i*) L1_BIASES;
  __m256* out            = (__m256*) dest;

  __m256i regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm256_setzero_si256();

    for (size_t j = 0; j < IN_CHUNKS; j += 4)
      for (size_t k = 0; k < OUT_CC; k++)
        m256_add_dpbusd_epi32x4(&regs[k], &in[j], &weights[j + IN_CHUNKS * (OUT_CC * i + k)]);

    m256_hadd_epi32x4(regs);
    m256_hadd_epi32x4(&regs[4]);

    const __m128i t0 = _mm_add_epi32(_mm256_castsi256_si128(regs[0]), _mm256_extracti128_si256(regs[0], 1));
    const __m128i t4 = _mm_add_epi32(_mm256_castsi256_si128(regs[4]), _mm256_extracti128_si256(regs[4], 1));
    __m256i sum      = _mm256_inserti128_si256(_mm256_castsi128_si256(t0), t4, 1);
    out[i]           = _mm256_cvtepi32_ps(_mm256_max_epi32(_mm256_add_epi32(sum, biases[i]), _mm256_setzero_si256()));
  }
}

#elif defined(__SSSE3__)
INLINE void m128_add_dpbusd_epi32x4(__m128i* acc, const __m128i* inputs, const __m128i* weights) {
  __m128i tmp0 = _mm_maddubs_epi16(inputs[0], weights[0]);
  __m128i tmp1 = _mm_maddubs_epi16(inputs[1], weights[1]);
  __m128i tmp2 = _mm_maddubs_epi16(inputs[2], weights[2]);
  __m128i tmp3 = _mm_maddubs_epi16(inputs[3], weights[3]);

  tmp0 = _mm_add_epi16(tmp0, _mm_add_epi16(tmp1, _mm_add_epi16(tmp2, tmp3)));
  *acc = _mm_add_epi32(*acc, _mm_madd_epi16(tmp0, _mm_set1_epi16(1)));
}

INLINE void m128_hadd_epi32x4(__m128i* regs) {
  regs[0] = _mm_hadd_epi32(regs[0], regs[1]);
  regs[2] = _mm_hadd_epi32(regs[2], regs[3]);
  regs[0] = _mm_hadd_epi32(regs[0], regs[2]);
}

INLINE void L1AffineReLU(float* dest, uint8_t* src) {
  const size_t IN_WIDTH   = sizeof(__m128i) / sizeof(uint8_t);
  const size_t IN_CHUNKS  = N_L1 / IN_WIDTH;
  const size_t OUT_CC     = 4;
  const size_t OUT_CHUNKS = N_L2 / OUT_CC;

  const __m128i* in      = (__m128i*) src;
  const __m128i* weights = (__m128i*) L1_WEIGHTS;
  const __m128i* biases  = (__m128i*) L1_BIASES;
  __m128* out            = (__m128*) dest;

  __m128i regs[OUT_CC];

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    for (size_t k = 0; k < OUT_CC; k++)
      regs[k] = _mm_setzero_si128();

    for (size_t j = 0; j < IN_CHUNKS; j += 4)
      for (size_t k = 0; k < OUT_CC; k++)
        m128_add_dpbusd_epi32x4(&regs[k], &in[j], &weights[j + IN_CHUNKS * (OUT_CC * i + k)]);

    m128_hadd_epi32x4(regs);

    out[i] = _mm_max_ps(_mm_cvtepi32_ps(_mm_add_epi32(regs[0], biases[i])), _mm_setzero_ps());
  }
}
#else
INLINE void L1AffineReLU(float* dest, uint8_t* src) {
  for (int i = 0; i < N_L2; i++) {
    const int offset = i * N_L1;

    dest[i] = L1_BIASES[i];
    for (int j = 0; j < N_L1; j++)
      dest[i] += src[j] * L1_WEIGHTS[offset + j];

    dest[i] = Max(0, dest[i]);
  }
}
#endif

#if defined(__AVX2__)
INLINE void m256_hadd_psx4(__m256* regs) {
  regs[0] = _mm256_hadd_ps(regs[0], regs[1]);
  regs[2] = _mm256_hadd_ps(regs[2], regs[3]);
  regs[0] = _mm256_hadd_ps(regs[0], regs[2]);
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

    m256_hadd_psx4(regs);
    m256_hadd_psx4(&regs[4]);

    const __m128 t0 = _mm_add_ps(_mm256_castps256_ps128(regs[0]), _mm256_extractf128_ps(regs[0], 1));
    const __m128 t4 = _mm_add_ps(_mm256_castps256_ps128(regs[4]), _mm256_extractf128_ps(regs[4], 1));
    __m256 sum      = _mm256_insertf128_ps(_mm256_castps128_ps256(t0), t4, 1);
    out[i]          = _mm256_max_ps(_mm256_add_ps(sum, biases[i]), _mm256_setzero_ps());
  }
}
#elif defined(__SSE3__)
INLINE void m128_hadd_psx4(__m128* regs) {
  regs[0] = _mm_hadd_ps(regs[0], regs[1]);
  regs[2] = _mm_hadd_ps(regs[2], regs[3]);
  regs[0] = _mm_hadd_ps(regs[0], regs[2]);
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

    m128_hadd_psx4(regs);

    out[i] = _mm_max_ps(_mm_add_ps(regs[0], biases[i]), _mm_setzero_ps());
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

#if defined(__AVX2__)
INLINE float L3Transform(float* src) {
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
  uint8_t x0[N_L1] ALIGN;
  float x1[N_L2] ALIGN;
  float x2[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);
  L1AffineReLU(x1, x0);
  L2AffineReLU(x2, x1);
  return L3Transform(x2) / 32.0;
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

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);

  memcpy(L1_WEIGHTS, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
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

#if defined(__AVX2__)
  const size_t WIDTH         = sizeof(__m256i) / sizeof(int16_t);
  const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  __m256i* weights = (__m256i*) INPUT_WEIGHTS;
  __m256i* biases  = (__m256i*) INPUT_BIASES;

  for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
    __m128i a = _mm256_extracti128_si256(weights[i], 1);     // 64->128
    __m128i b = _mm256_extracti128_si256(weights[i + 1], 0); // 128->192

    weights[i]     = _mm256_inserti128_si256(weights[i], b, 1);     // insert 128->192 into 64->128
    weights[i + 1] = _mm256_inserti128_si256(weights[i + 1], a, 0); // insert 64->128 into 128->192
  }

  for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
    __m128i a = _mm256_extracti128_si256(biases[i], 1);     // 64->128
    __m128i b = _mm256_extracti128_si256(biases[i + 1], 0); // 128->192

    biases[i]     = _mm256_inserti128_si256(biases[i], b, 1);     // insert 128->192 into 64->128
    biases[i + 1] = _mm256_inserti128_si256(biases[i + 1], a, 0); // insert 64->128 into 128->192
  }
#endif
}

void LoadDefaultNN() {
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
