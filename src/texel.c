#ifdef TUNE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "eval.h"
#include "texel.h"
#include "types.h"

const int sideScalar[] = {1, -1};
double K = 1;

void Texel() {
  int n = 0;
  Position* positions = loadPositions(&n);

  K = -1.035509;
  // determineK(positions, n);

  TexelParam params[128];

  params[0].name = "KS_ATTACKER_WEIGHTS[1]";
  params[0].param = &KS_ATTACKER_WEIGHTS[1];

  params[1].name = "KS_ATTACKER_WEIGHTS[2]";
  params[1].param = &KS_ATTACKER_WEIGHTS[2];

  params[2].name = "KS_ATTACKER_WEIGHTS[3]";
  params[2].param = &KS_ATTACKER_WEIGHTS[3];

  params[3].name = "KS_ATTACKER_WEIGHTS[4]";
  params[3].param = &KS_ATTACKER_WEIGHTS[4];

  params[4].name = "KS_ATTACK";
  params[4].param = &KS_ATTACK;

  params[5].name = "KS_SAFE_CHECK";
  params[5].param = &KS_SAFE_CHECK;

  params[6].name = "KS_UNSAFE_CHECK";
  params[6].param = &KS_UNSAFE_CHECK;

  params[7].name = "KS_ENEMY_QUEEN";
  params[7].param = &KS_ENEMY_QUEEN;

  params[8].name = "KS_ALLIES";
  params[8].param = &KS_ALLIES;

  SGD(params, 9, positions, n);

  free(positions);
}

void SGD(TexelParam* params, int numParams, Position* positions, int numPositions) {
  const double a = 0.3;
  const double b1 = 0.9;
  const double b2 = 0.999;
  const double epsilon = 1.0e-8;

  double best = totalError(positions, numPositions);

  double* gradients = calloc(numParams, sizeof(double));
  double* M = calloc(numParams, sizeof(double));
  double* V = calloc(numParams, sizeof(double));

  for (int epoch = 1; epoch <= 100000; epoch++) {
    printf("\n\nEpoch %d\n\n", epoch);

    double base = totalError(positions, numPositions);

    CalculateGradients(gradients, params, numParams, positions, numPositions, base);

    for (int i = 0; i < numParams; i++) {
      double gradient = gradients[i];

      M[i] = b1 * M[i] + (1.0 - b1) * gradient;
      V[i] = b2 * V[i] + (1.0 - b2) * gradient * gradient;

      double rate = a * (1.0 - pow(b2, epoch)) / (1.0 - pow(b1, epoch));
      double delta = rate * M[i] / (sqrt(V[i]) + epsilon);

      const Score oldValue = *params[i].param;
      *params[i].param -= delta;

      if (*params[i].param != oldValue)
        printf("%45s: (%.8f, %.8f)\n", params[i].name, oldValue, *params[i].param);
    }

    double curr = totalError(positions, numPositions);
    printf("Base: %.8f, Current: %.8f\n", base, curr);

    if (epoch % 10 == 0) {
      double completed = totalError(positions, numPositions);
      PrintParams(params, numParams);
      if (completed == best)
        break;
      else
        best = completed;
    }
  }
}

void CalculateGradients(double* gradients, TexelParam* params, int numParams, Position* positions, int numPositions,
                        double base) {
  for (int i = 0; i < numParams; i++) {
    TexelParam param = params[i];
    const Score oldValue = *param.param;

    *param.param += 1;

    double ep1 = totalError(positions, numPositions);
    gradients[i] = ep1 - base;

    *param.param = oldValue;
  }
}

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

void PrintParams(TexelParam* params, int numParams) {
  printf("\n\nCurrent Values:\n");
  for (int i = 0; i < numParams; i++)
    printf("%45s: %.8f\n", params[i].name, *params[i].param);
}
#endif