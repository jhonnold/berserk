#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "texel.h"
#include "types.h"

const int sideScalar[] = {1, -1};
double K = 1;

Position* loadPositions(int* n) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = malloc(sizeof(Position) * 8000000);

  int buffersize = 100;
  char buffer[100];

  int p = 0;
  while (fgets(buffer, buffersize, fp)) {
    int i;
    for (i = 0; i < buffersize; i++)
      if (buffer[i] == '"')
        break;

    memcpy(positions[p].fen, buffer, i * sizeof(char));

    i++;
    if (buffer[i] == '0')
      positions[p].result = 0.0;
    else {
      i++;
      if (buffer[i] == '/')
        positions[p].result = 0.5;
      else
        positions[p].result = 1.0;
    }

    p++;
  }

  *n = p;

  return positions;
}

void texel() {
  int n = 0;
  Position* positions = loadPositions(&n);

  determineK(positions, n);

  free(positions);
}

// https://github.com/ed-apostol/InvictusChess/blob/master/src/tune.cpp#L84
void determineK(Position* positions, int n) {
  double min = -10, max = 10, delta = 1, best = 1, error = 100;

  for (int p = 0; p < 7; p++) {
    printf("Determining K: (%.6f, %.6f, %.7f)\n", min, max, delta);

    while (min < max) {
      K = min;
      double e = totalError(positions, n);
      if (e < error) {
        error = e;
        best = K;
        printf("New best K of %.6f, Error %.10f\n", K, error);
      }
      min += delta;
    }

    min = best - delta;
    max = best + delta;
    delta /= 10;
  }

  K = best;
  printf("Using K of %.6f\n", K);
}

double totalError(Position* positions, int n) {
  double t = 0;

  for (int i = 0; i < n; i++)
    t += error(&positions[i]);

  return t / n;
}

double error(Position* p) {
  Board board[1];
  parseFen(p->fen, board);

  int score = sideScalar[board->side] * Evaluate(board);
  return pow(p->result - sigmoid(score), 2);
}

double sigmoid(int score) { return (double)1 / (1 + pow(10, K * score / 400)); }