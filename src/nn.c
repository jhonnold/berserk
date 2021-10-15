// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__AVX__)
#include <immintrin.h>
#elif defined(__SSE__)
#include <xmmintrin.h>
#endif

#include "bits.h"
#include "board.h"
#include "nn.h"
#include "util.h"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

#ifndef EVALFILE
#define EVALFILE "default.nn"
#endif

INCBIN(Embed, EVALFILE);

const int NETWORK_MAGIC = 'B' | 'Z' << 8 | 1 << 16 | 0 << 24;
const int NETWORK_ID = 0;
const int INPUT_SIZE = N_FEATURES;
const int OUTPUT_SIZE = N_OUTPUT;
const int N_HIDDEN_LAYERS = 1;
const int HIDDEN_SIZES[1] = {N_HIDDEN};
const int QUANTIZATION_PRECISION_IN = 32;
const int QUANTIZATION_PRECISION_OUT = 512;

Weight FEATURE_WEIGHTS[N_FEATURES * N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
Weight HIDDEN_BIASES[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
Weight HIDDEN_WEIGHTS[2 * N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
int OUTPUT_BIAS;

void ApplyFirstLayer(Board* board, Accumulator output, int perspective) {
  memcpy(output, HIDDEN_BIASES, sizeof(Accumulator));

  for (int sq = 0; sq < 64; sq++) {
    int pc = board->squares[sq];
    if (pc == NO_PIECE)
      continue;

    for (int i = 0; i < N_HIDDEN; i++)
      output[i] += FEATURE_WEIGHTS[FeatureIdx(pc, sq, perspective) * N_HIDDEN + i];
  }
}

#if defined(__AVX__)
int ApplySecondLayer(Accumulator a1, Accumulator a2) {
  int result = OUTPUT_BIAS * QUANTIZATION_PRECISION_IN;

  const __m256i zero = _mm256_setzero_si256();
  __m256i s0 = _mm256_setzero_si256();
  __m256i s1 = _mm256_setzero_si256();

  for (size_t j = 0; j < N_HIDDEN / 16; j++) {
    const __m256i ac0 = _mm256_max_epi16(((__m256i*)a1)[j], zero);
    const __m256i ac1 = _mm256_max_epi16(((__m256i*)a2)[j], zero);

    s0 = _mm256_add_epi32(s0, _mm256_madd_epi16(ac0, ((__m256i*)HIDDEN_WEIGHTS)[j]));
    s1 = _mm256_add_epi32(s1, _mm256_madd_epi16(ac1, ((__m256i*)HIDDEN_WEIGHTS)[j + N_HIDDEN / 16]));
  }

  const __m256i r8 = _mm256_add_epi32(s0, s1);
  const __m128i r4 = _mm_add_epi32(_mm256_castsi256_si128(r8), _mm256_extractf128_si256(r8, 1));
  const __m128i r2 = _mm_add_epi32(r4, _mm_srli_si128(r4, 8));
  const __m128i r1 = _mm_add_epi32(r2, _mm_srli_si128(r2, 4));

  result += _mm_cvtsi128_si32(r1);
  return result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}
#elif defined(__SSE__)
int ApplySecondLayer(Accumulator a1, Accumulator a2) {
  int result = OUTPUT_BIAS * QUANTIZATION_PRECISION_IN;

  const __m128i zero = _mm_setzero_si128();
  __m128i s0 = _mm_setzero_si128();
  __m128i s1 = _mm_setzero_si128();

  for (size_t j = 0; j < N_HIDDEN / 8; j++) {
    const __m128i ac0 = _mm_max_epi16(((__m128i*)a1)[j], zero);
    const __m128i ac1 = _mm_max_epi16(((__m128i*)a2)[j], zero);

    s0 = _mm_add_epi32(s0, _mm_madd_epi16(ac0, ((__m128i*)HIDDEN_WEIGHTS)[j]));
    s1 = _mm_add_epi32(s1, _mm_madd_epi16(ac1, ((__m128i*)HIDDEN_WEIGHTS)[j + N_HIDDEN / 8]));
  }

  const __m128i r4 = _mm_add_epi32(s0, s1);
  const __m128i r2 = _mm_add_epi32(r4, _mm_srli_si128(r4, 8));
  const __m128i r1 = _mm_add_epi32(r2, _mm_srli_si128(r2, 4));

  result += _mm_cvtsi128_si32(r1);
  return result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}
#else
int ApplySecondLayer(Accumulator a1, Accumulator a2) {
  int result = OUTPUT_BIAS * QUANTIZATION_PRECISION_IN;

  for (int i = 0; i < N_HIDDEN; i++) {
    result += max(a1[i], 0) * HIDDEN_WEIGHTS[i];
    result += max(a2[i], 0) * HIDDEN_WEIGHTS[i + N_HIDDEN];
  }

  return result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}
#endif

int NNPredict(Board* board) {
  Accumulator white;
  Accumulator black;

  ApplyFirstLayer(board, white, WHITE);
  ApplyFirstLayer(board, black, BLACK);

  Weight* stm = board->side == WHITE ? white : black;
  Weight* xstm = board->side == WHITE ? black : white;

  return ApplySecondLayer(stm, xstm);
}

void AddUpdate(int feature, int c, NNUpdate* updates) {
  updates->features[updates->n] = feature;
  updates->coeffs[updates->n] = c;

  updates->n++;
}

void ApplyUpdates(NNUpdate* updates, Accumulator output) {
  for (int i = 0; i < updates->n; i++) {
    const int c = updates->coeffs[i];
    const int f = updates->features[i];

    for (int j = 0; j < N_HIDDEN; j++)
      output[j] += c * FEATURE_WEIGHTS[f * N_HIDDEN + j];
  }
}

inline Weight LoadWeight(float v, int in) {
  return round(v * (in ? QUANTIZATION_PRECISION_IN : QUANTIZATION_PRECISION_OUT));
}

void LoadDefaultNN() {
  float* data = (float*)EmbedData + 6;

  for (int j = 0; j < N_FEATURES * N_HIDDEN; j++) {
    FEATURE_WEIGHTS[j] = LoadWeight(*data++, 1);
  }

  for (int j = 0; j < N_HIDDEN; j++) {
    HIDDEN_BIASES[j] = LoadWeight(*data++, 1);
  }

  for (int j = 0; j < N_HIDDEN * 2; j++) {
    HIDDEN_WEIGHTS[j] = LoadWeight(*data++, 0);
  }

  OUTPUT_BIAS = round(*data * QUANTIZATION_PRECISION_OUT);
}
