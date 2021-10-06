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

#include "bits.h"
#include "board.h"
#include "nn.h"

const int NETWORK_MAGIC = 'B' | 'Z' << 8 | 1 << 16 | 0 << 24;
const int NETWORK_ID = 0;
const int INPUT_SIZE = N_FEATURES;
const int OUTPUT_SIZE = N_OUTPUT;
const int N_HIDDEN_LAYERS = 1;
const int HIDDEN_SIZES[1] = {N_HIDDEN};

float FEATURE_WEIGHTS[N_FEATURES * N_HIDDEN] __attribute__ ((aligned (64)));
float HIDDEN_WEIGHTS[N_HIDDEN] __attribute__ ((aligned (64)));
float HIDDEN_BIASES[N_HIDDEN] __attribute__ ((aligned (64)));
float OUTPUT_BIAS;

void ApplyFirstLayer(Board* board, float* output) {
  memcpy(output, HIDDEN_BIASES, sizeof(float) * N_HIDDEN);

  for (int sq = 0; sq < 64; sq++) {
    int pc = board->squares[sq];
    if (pc == NO_PIECE) continue;

    for (int i = 0; i < N_HIDDEN; i++)
      output[i] += FEATURE_WEIGHTS[FeatureIdx(pc, sq) * N_HIDDEN + i];
  }
}

float ApplySecondLayer(float* hidden) {
  float result = 0.0f;
  for (int j = 0; j < N_HIDDEN; j++)
    result += fmax(0.0f, hidden[j]) * HIDDEN_WEIGHTS[j];

  return result + OUTPUT_BIAS;
}

float NNPredict(Board* board) {
  float hidden[N_HIDDEN] __attribute__ ((aligned (64)));

  ApplyFirstLayer(board, hidden);
  return ApplySecondLayer(hidden);
}

void AddUpdate(int feature, int c, NNUpdate* updates) {
  updates->features[updates->n] = feature;
  updates->coeffs[updates->n] = c;

  updates->n++;
}

void ApplyUpdates(NNUpdate* updates, float* curr, float* output) {
  memcpy(output, curr, sizeof(float) * N_HIDDEN);

  for (int i = 0; i < updates->n; i++)
    for (int j = 0; j < N_HIDDEN; j++)
      output[j] += updates->coeffs[i] * FEATURE_WEIGHTS[updates->features[i] * N_HIDDEN + j];
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

  fread(FEATURE_WEIGHTS, 4, N_FEATURES * N_HIDDEN, fp);
  fread(HIDDEN_BIASES, 4, N_HIDDEN, fp);
  fread(HIDDEN_WEIGHTS, 4, N_HIDDEN, fp);
  fread(&OUTPUT_BIAS, 4, N_OUTPUT, fp);

  fclose(fp);

  printf("info string Succesfully loaded net %s\n", path);
}
