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

float FEATURE_WEIGHTS[N_FEATURES * N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
float HIDDEN_WEIGHTS[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
float HIDDEN_BIASES[N_HIDDEN] __attribute__((aligned(ALIGN_ON)));
float OUTPUT_BIAS;

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
float ApplySecondLayer(Accumulator hidden) {
  const __m256 zero = _mm256_setzero_ps();
  __m256 r8 = _mm256_setzero_ps();

  for (size_t j = 0; j < N_HIDDEN; j += 8) {
    const __m256 activated = _mm256_max_ps(_mm256_load_ps(hidden + j), zero);
    const __m256 weights = _mm256_load_ps(HIDDEN_WEIGHTS + j);

    r8 = _mm256_add_ps(r8, _mm256_mul_ps(activated, weights));
  }

  const __m128 r4 = _mm_add_ps(_mm256_castps256_ps128(r8), _mm256_extractf128_ps(r8, 1));
  const __m128 r2 = _mm_add_ps(r4, _mm_movehl_ps(r4, r4));
  const __m128 r1 = _mm_add_ss(r2, _mm_shuffle_ps(r2, r2, 0x1));
  const float sum = _mm_cvtss_f32(r1);

  return OUTPUT_BIAS + sum;
}
#elif defined(__SSE__)
float ApplySecondLayer(Accumulator hidden) {
  const __m128 zero = _mm_setzero_ps();
  __m128 r4 = _mm_setzero_ps();

  for (size_t j = 0; j < N_HIDDEN; j += 4) {
    const __m128 activated = _mm_max_ps(_mm_load_ps(hidden + j), zero);
    const __m128 weights = _mm_load_ps(HIDDEN_WEIGHTS + j);

    r4 = _mm_add_ps(r4, _mm_mul_ps(activated, weights));
  }

  const __m128 r2 = _mm_add_ps(r4, _mm_movehl_ps(r4, r4));
  const __m128 r1 = _mm_add_ss(r2, _mm_shuffle_ps(r2, r2, 0x1));
  const float sum = _mm_cvtss_f32(r1);
  return OUTPUT_BIAS + sum;
}
#else
float ApplySecondLayer(Accumulator hidden) {
  float result = OUTPUT_BIAS;
  for (int i = 0; i < N_HIDDEN; i++)
    result += fmax(hidden[i], 0.0f) * HIDDEN_WEIGHTS[i];

  return result;
}
#endif

float NNPredict(Board* board) {
  Accumulator hidden;

  ApplyFirstLayer(board, hidden, board->side);
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

void LoadDefaultNN() {
  size_t i = 24;

  memcpy(FEATURE_WEIGHTS, EmbedData + i, sizeof(float) * N_FEATURES * N_HIDDEN);
  i += sizeof(float) * N_FEATURES * N_HIDDEN;

  memcpy(HIDDEN_BIASES, EmbedData + i, sizeof(float) * N_HIDDEN);
  i += sizeof(float) * N_HIDDEN;

  memcpy(HIDDEN_WEIGHTS, EmbedData + i, sizeof(float) * N_HIDDEN);
  i += sizeof(float) * N_HIDDEN;

  memcpy(&OUTPUT_BIAS, EmbedData + i, sizeof(float));
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

  fread(FEATURE_WEIGHTS, sizeof(float), N_FEATURES * N_HIDDEN, fp);
  fread(HIDDEN_BIASES, sizeof(float), N_HIDDEN, fp);
  fread(HIDDEN_WEIGHTS, sizeof(float), N_HIDDEN, fp);
  fread(&OUTPUT_BIAS, sizeof(float), N_OUTPUT, fp);

  fclose(fp);

  printf("info string Succesfully loaded net %s\n", path);
}
