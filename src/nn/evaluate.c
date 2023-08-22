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

int8_t L1_WEIGHTS[N_LAYERS][N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_LAYERS][N_L2] ALIGN;

float L2_WEIGHTS[N_LAYERS][N_L2 * N_L3] ALIGN;
float L2_BIASES[N_LAYERS][N_L3] ALIGN;

float OUTPUT_WEIGHTS[N_LAYERS][N_L3 * N_OUTPUT] ALIGN;
float OUTPUT_BIAS[N_LAYERS] ALIGN;

int32_t PSQT_WEIGHTS[N_FEATURES * N_LAYERS] ALIGN;

#if defined(__AVX2__)
INLINE void InputCReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;

  const __m256i zero = _mm256_setzero_si256();

  const int views[2] = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      out[i]     = _mm256_max_epi8(zero, _mm256_packs_epi16(in[2 * i + 0], in[2 * i + 1]));
      out[i + 1] = _mm256_max_epi8(zero, _mm256_packs_epi16(in[2 * i + 2], in[2 * i + 3]));
    }
  }
}
#elif defined(__SSE2__)
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  // TODO: Update for CReLU
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
INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  // TODO: Update for CReLU
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

INLINE void L1AffineReLU(float* dest, int8_t* src, int8_t* wei, int32_t* bia) {
  const size_t IN_WIDTH   = sizeof(__m256i) / sizeof(int8_t);
  const size_t IN_CHUNKS  = N_L1 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L2 / OUT_CC;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) wei;
  const __m256i* biases  = (__m256i*) bia;
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

INLINE void L1AffineReLU(float* dest, int8_t* src, int8_t* wei, int32_t* bia) {
  const size_t IN_WIDTH   = sizeof(__m128i) / sizeof(int8_t);
  const size_t IN_CHUNKS  = N_L1 / IN_WIDTH;
  const size_t OUT_CC     = 4;
  const size_t OUT_CHUNKS = N_L2 / OUT_CC;

  const __m128i* in      = (__m128i*) src;
  const __m128i* weights = (__m128i*) wei;
  const __m128i* biases  = (__m128i*) bia;
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
INLINE void L1AffineReLU(float* dest, int8_t* src, int8_t* wei, int32_t* bia) {
  for (int i = 0; i < N_L2; i++) {
    const int offset = i * N_L1;

    dest[i] = bia[i];
    for (int j = 0; j < N_L1; j++)
      dest[i] += src[j] * wei[offset + j];

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

INLINE void L2AffineReLU(float* dest, float* src, float* wei, float* bia) {
  const size_t IN_WIDTH   = sizeof(__m256) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) wei;
  const __m256* biases  = (__m256*) bia;
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

INLINE void L2AffineReLU(float* dest, float* src, float* wei, float* bia) {
  const size_t IN_WIDTH   = sizeof(__m128) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 4;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m128* in      = (__m128*) src;
  const __m128* weights = (__m128*) wei;
  const __m128* biases  = (__m128*) bia;
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
INLINE void L2AffineReLU(float* dest, float* src, float* wei, float* bia) {
  for (int i = 0; i < N_L3; i++) {
    const int offset = i * N_L2;

    dest[i] = bia[i];
    for (int j = 0; j < N_L2; j++)
      dest[i] += src[j] * wei[offset + j];

    dest[i] = Max(0, dest[i]);
  }
}
#endif

#if defined(__AVX2__)
INLINE float L3Transform(float* src, float* wei, float bia) {
  const size_t WIDTH  = sizeof(__m256) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) wei;

  __m256 a0 = _mm256_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm256_fmadd_ps(in[i], weights[i], a0);

  const __m128 a4 = _mm_add_ps(_mm256_castps256_ps128(a0), _mm256_extractf128_ps(a0, 1));
  const __m128 a2 = _mm_add_ps(a4, _mm_movehl_ps(a4, a4));
  const __m128 a1 = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + bia;
}
#elif defined(__SSE__)
INLINE float L3Transform(float* src, float* wei, float bia) {
  const size_t WIDTH  = sizeof(__m128) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m128* in      = (__m128*) src;
  const __m128* weights = (__m128*) wei;

  __m128 a0 = _mm_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm_add_ps(a0, _mm_mul_ps(in[i], weights[i]));

  const __m128 a2 = _mm_add_ps(a0, _mm_movehl_ps(a0, a0));
  const __m128 a1 = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + bia;
}
#else
INLINE float L3Transform(float* src, float* wei, float bia) {
  float result = bia;

  for (int i = 0; i < N_L3; i++)
    result += src[i] * wei[i];

  return result;
}
#endif

int NNEvaluate(Board* board) {
  const int stm    = board->stm;
  const int layer  = (BitCount(OccBB(BOTH)) - 1) / 4;
  Accumulator* acc = board->accumulators;

  int psqt = (acc->psqt[stm][layer] - acc->psqt[!stm][layer]) / 2;

  int8_t x0[N_L1] ALIGN;
  float x1[N_L2] ALIGN;
  float x2[N_L3] ALIGN;

  InputCReLU(x0, acc, stm);
  L1AffineReLU(x1, x0, L1_WEIGHTS[layer], L1_BIASES[layer]);
  L2AffineReLU(x2, x1, L2_WEIGHTS[layer], L2_BIASES[layer]);
  float positional = L3Transform(x2, OUTPUT_WEIGHTS[layer], OUTPUT_BIAS[layer]);

  return (positional + psqt) / 64;
}

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN +  // input weights
                            sizeof(int16_t) * N_HIDDEN +               // input biases
                            N_LAYERS * (sizeof(int8_t) * N_L1 * N_L2 + // l1 weights
                                        sizeof(int32_t) * N_L2 +       // l1 biases
                                        sizeof(float) * N_L2 * N_L3 +  // l2 weights
                                        sizeof(float) * N_L3 +         // l2 biases
                                        sizeof(float) * N_L3 +         // output weights
                                        sizeof(float) +                // output bias
                                        sizeof(int32_t) * N_FEATURES); // psqt weights

INLINE uint8_t* CopyInto8(int8_t* dest, uint8_t* src, const size_t count) {
  memcpy(dest, src, count * sizeof(int8_t));
  return src + count * sizeof(int8_t);
}

INLINE uint8_t* CopyInto16(int16_t* dest, uint8_t* src, const size_t count) {
  memcpy(dest, src, count * sizeof(int16_t));
  return src + count * sizeof(int16_t);
}

INLINE uint8_t* CopyInto32(int32_t* dest, uint8_t* src, const size_t count) {
  memcpy(dest, src, count * sizeof(int32_t));
  return src + count * sizeof(int32_t);
}

INLINE uint8_t* CopyIntoF(float* dest, uint8_t* src, const size_t count) {
  memcpy(dest, src, count * sizeof(float));
  return src + count * sizeof(float);
}

INLINE void CopyData(const uint8_t* in) {
  uint8_t* next = (uint8_t*) in;

  next = CopyInto16(INPUT_WEIGHTS, next, N_FEATURES * N_HIDDEN);
  next = CopyInto16(INPUT_BIASES, next, N_HIDDEN);

  next = CopyInto8((int8_t*) L1_WEIGHTS, next, N_L1 * N_L2 * N_LAYERS);
  next = CopyInto32((int32_t*) L1_BIASES, next, N_L2 * N_LAYERS);

  next = CopyIntoF((float*) L2_WEIGHTS, next, N_L2 * N_L3 * N_LAYERS);
  next = CopyIntoF((float*) L2_BIASES, next, N_L3 * N_LAYERS);

  next = CopyIntoF((float*) OUTPUT_WEIGHTS, next, N_L3 * N_OUTPUT * N_LAYERS);
  next = CopyIntoF((float*) OUTPUT_BIAS, next, N_OUTPUT * N_LAYERS);

  next = CopyInto32(PSQT_WEIGHTS, next, N_FEATURES * N_LAYERS);

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
