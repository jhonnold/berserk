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

#define THREADS 32

const int sideScalar[] = {1, -1};
double K = 1;

void addParam(char* name, Score* p, TexelParam* params, int* n) {
  params[*n].name = name;
  params[*n].param = p;

  *n += 1;
}

void Texel() {
  int n = 0;
  Position* positions = loadPositions(&n);

  // K = -1.035509;
  determineK(positions, n);

  int numParams = 0;
  TexelParam params[128];

  addParam("MATERIAL_VALUES_PAWN[MG]", &MATERIAL_VALUES[PAWN_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_PAWN[EG]", &MATERIAL_VALUES[PAWN_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_KNIGHT[MG]", &MATERIAL_VALUES[KNIGHT_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_KNIGHT[EG]", &MATERIAL_VALUES[KNIGHT_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_BISHOP[MG]", &MATERIAL_VALUES[BISHOP_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_BISHOP[EG]", &MATERIAL_VALUES[BISHOP_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_ROOK[MG]", &MATERIAL_VALUES[ROOK_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_ROOK[EG]", &MATERIAL_VALUES[ROOK_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_QUEEN[MG]", &MATERIAL_VALUES[QUEEN_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_QUEEN[EG]", &MATERIAL_VALUES[QUEEN_TYPE][EG], params, &numParams);

  SGD(params, numParams, positions, n);

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
    printf("\nBase: %.8f, Current: %.8f\n", base, curr);

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
  double* errors = calloc(n, sizeof(double));

  pthread_t threads[THREADS];
  BatchJob jobs[THREADS];

  int chunkSize = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    jobs[t].errors = errors + (t * chunkSize);
    jobs[t].positions = positions + (t * chunkSize);
    jobs[t].n = t != THREADS - 1 ? chunkSize : (n - ((THREADS - 1) * chunkSize));
    jobs[t].threadid = t;

    pthread_create(&threads[t], NULL, batchError, (void*)(jobs + t));
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  double totalE = 0;
  for (int i = 0; i < n; i++)
    totalE += errors[i];

  free(errors);
  return totalE / n;
}

void* batchError(void* arg) {
  BatchJob* batch = (BatchJob*)arg;

  for (int i = 0; i < batch->n; i++) {
    batch->errors[i] = error(batch->positions + i);
  }

  pthread_exit(NULL);
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