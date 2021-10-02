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

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "random.h"
#include "tuner/util.h"

float pawnNetData[1041] = {
#include "pawnnet.h"
};

Network* PAWN_NET;

float ApplyNetwork(int inputs[N_FEATURES], Network* network) {
  memcpy(network->hidden, network->biases0, sizeof(float) * N_HIDDEN);

  for (int i = 0; i < N_FEATURES; i++)
    if (inputs[i])
      for (int j = 0; j < N_HIDDEN; j++)
        network->hidden[j] += network->weights0[j * N_FEATURES + i];

  for (int i = 0; i < N_HIDDEN; i++)
    network->hidden[i] = ReLu(network->hidden[i]);

  float result = network->biases1[0];
  for (int i = 0; i < N_HIDDEN; i++)
    result += network->weights1[i] * network->hidden[i];

  return result;
}

void SaveNetwork(char* path, Network* network) {
  FILE* fp = fopen(path, "w");
  if (fp == NULL) {
    printf("info string Unable to save network!\n");
    return;
  }

  for (int i = 0; i < N_FEATURES * N_HIDDEN; i++)
    fprintf(fp, "%f,", network->weights0[i]);

  for (int i = 0; i < N_HIDDEN * N_OUTPUT; i++)
    fprintf(fp, "%f,", network->weights1[i]);

  for (int i = 0; i < N_HIDDEN; i++)
    fprintf(fp, "%f,", network->biases0[i]);

  for (int i = 0; i < N_OUTPUT; i++)
    fprintf(fp, "%f,", network->biases1[i]);

  fclose(fp);
}

void InitNetwork() {
  PAWN_NET = malloc(sizeof(Network));
  int n = 0;

  for (int i = 0; i < N_FEATURES * N_HIDDEN; i++)
    PAWN_NET->weights0[i] = pawnNetData[n++];

  for (int i = 0; i < N_HIDDEN * N_OUTPUT; i++)
    PAWN_NET->weights1[i] = pawnNetData[n++];

  for (int i = 0; i < N_HIDDEN; i++)
    PAWN_NET->biases0[i] =pawnNetData[n++];

  for (int i = 0; i < N_OUTPUT; i++)
    PAWN_NET->biases1[i] = pawnNetData[n++];
}
