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
const int QUANTIZATION_PRECISION = 128;

Weight FEATURE_WEIGHTS[N_FEATURES * N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
Weight HIDDEN_WEIGHTS[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
Weight HIDDEN_BIASES[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
Weight OUTPUT_BIAS;

void ApplyFirstLayer(Board* board, Accumulator output) {
  memcpy(output, HIDDEN_BIASES, sizeof(Accumulator));

  for (int sq = 0; sq < 64; sq++) {
    int pc = board->squares[sq];
    if (pc == NO_PIECE)
      continue;

    for (int i = 0; i < N_HIDDEN; i++)
      output[i] += FEATURE_WEIGHTS[FeatureIdx(pc, sq) * N_HIDDEN + i];
  }
}

#if defined(__AVX__)
Weight ApplySecondLayer(Accumulator hidden) {
  Weight result = OUTPUT_BIAS * QUANTIZATION_PRECISION;

  const __m256i zero = _mm256_setzero_si256();
  __m256i r8 = _mm256_setzero_si256();

  for (size_t j = 0; j < N_HIDDEN / 8; j++) {
    const __m256i activated = _mm256_max_epi32(((__m256i*)hidden)[j], zero);

    r8 = _mm256_add_epi32(r8, _mm256_mullo_epi32(activated, ((__m256i*)HIDDEN_WEIGHTS)[j]));
  }

  const __m128i r4 = _mm_add_epi32(_mm256_castsi256_si128(r8), _mm256_extractf128_si256(r8, 1));
  const __m128i r2 = _mm_add_epi32(r4, _mm_srli_si128(r4, 8));
  const __m128i r1 = _mm_add_epi32(r2, _mm_srli_si128(r2, 4));

  result += _mm_cvtsi128_si32(r1);
  return result / QUANTIZATION_PRECISION / QUANTIZATION_PRECISION;
}
#elif defined(__SSE__)
Weight ApplySecondLayer(Accumulator hidden) {
  Weight result = OUTPUT_BIAS * QUANTIZATION_PRECISION;

  const __m128i zero = _mm_setzero_si128();
  __m128i r4 = _mm_setzero_si128();

  for (size_t j = 0; j < N_HIDDEN / 4; j++) {
    const __m128i activated = _mm_max_epi32(((__m128i*)hidden)[j], zero);

    r4 = _mm_add_epi32(r4, _mm_mullo_epi32(activated, ((__m128i*)HIDDEN_WEIGHTS)[j]));
  }

  const __m128i r2 = _mm_add_epi32(r4, _mm_srli_si128(r4, 8));
  const __m128i r1 = _mm_add_epi32(r2, _mm_srli_si128(r2, 4));

  result += _mm_cvtsi128_si32(r1);
  return result / QUANTIZATION_PRECISION / QUANTIZATION_PRECISION;
}
#else
Weight ApplySecondLayer(Accumulator hidden) {
  Weight result = OUTPUT_BIAS * QUANTIZATION_PRECISION;

  for (int i = 0; i < N_HIDDEN; i++)
    result += max(hidden[i], 0) * HIDDEN_WEIGHTS[i];

  return result / QUANTIZATION_PRECISION / QUANTIZATION_PRECISION;
}
#endif

Weight NNPredict(Board* board) {
  Accumulator hidden;

  ApplyFirstLayer(board, hidden);
  return ApplySecondLayer(hidden);
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

inline Weight LoadWeight(float v) { return round(v * QUANTIZATION_PRECISION); }

void LoadDefaultNN() {
  float* data = (float*)EmbedData + 6;

  for (int j = 0; j < N_FEATURES * N_HIDDEN; j++)
    FEATURE_WEIGHTS[j] = LoadWeight(*data++);

  for (int j = 0; j < N_HIDDEN; j++)
    HIDDEN_BIASES[j] = LoadWeight(*data++);

  for (int j = 0; j < N_HIDDEN; j++)
    HIDDEN_WEIGHTS[j] = LoadWeight(*data++);

  OUTPUT_BIAS = LoadWeight(*data);
}

void LoadNN(char* path) {
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    printf("Unable to read network at %s!\n", path);
    exit(1);
  }

  int magic;
  fread(&magic, 4, 1, fp);

  if (magic != NETWORK_MAGIC) {
    printf("Magic header does not match!\n");
    exit(1);
  }

  // Skip past the topology as we only support one
  int temp;
  fread(&temp, 4, 1, fp);
  fread(&temp, 4, 1, fp);
  fread(&temp, 4, 1, fp);
  fread(&temp, 4, 1, fp);
  fread(&temp, 4, 1, fp);

  float featureWeights[N_FEATURES * N_HIDDEN];
  fread(featureWeights, sizeof(float), N_FEATURES * N_HIDDEN, fp);

  for (int j = 0; j < N_FEATURES * N_HIDDEN; j++)
    FEATURE_WEIGHTS[j] = LoadWeight(featureWeights[j]);

  float hiddenBiases[N_HIDDEN];
  fread(hiddenBiases, sizeof(float), N_HIDDEN, fp);

  for (int j = 0; j < N_HIDDEN; j++)
    HIDDEN_BIASES[j] = LoadWeight(hiddenBiases[j]);

  float hiddenWeights[N_HIDDEN];
  fread(hiddenWeights, sizeof(float), N_HIDDEN, fp);

  for (int j = 0; j < N_HIDDEN; j++)
    HIDDEN_WEIGHTS[j] = LoadWeight(hiddenWeights[j]);

  float outputBias;
  fread(&outputBias, sizeof(float), N_OUTPUT, fp);

  OUTPUT_BIAS = LoadWeight(outputBias);

  fclose(fp);

  printf("info string Succesfully loaded net %s\n", path);
}
