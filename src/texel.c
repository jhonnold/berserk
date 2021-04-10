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

#ifdef TUNE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "random.h"
#include "search.h"
#include "texel.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\texel-set.epd"

#define THREADS 32
#define Alpha 0.003
#define Beta1 0.9
#define Beta2 0.999
#define Epsilon 1e-8

#define Batch 3250000
#define MaxPositions 3250000

#define QS 0
#define FILTER 1
#define REMOVE_CHECKS 0

#define TUNE_MATERIAL 0
#define TUNE_PAWN_PSQT 0
#define TUNE_KNIGHT_PSQT 0
#define TUNE_BISHOP_PSQT 0
#define TUNE_ROOK_PSQT 0
#define TUNE_QUEEN_PSQT 0
#define TUNE_KING_PSQT 0
#define TUNE_MINOR_PARAMS 1
#define TUNE_KNIGHT_MOBILITIES 0
#define TUNE_BISHOP_MOBILITIES 0
#define TUNE_ROOK_MOBILITIES 0
#define TUNE_QUEEN_MOBILITIES 0
#define TUNE_PAWN_PARAMS 0
#define TUNE_ROOK_PARAMS 0
#define TUNE_THREATS 0
#define TUNE_SHELTER_STORM 0
#define TUNE_TEMPO 0
#define TUNE_KING_SAFETY 0

#define CHOOSE_K 1

// 1.0111283200 quiet-set
// 1.1123604100 lichess-new-labeled
double K = 1.0111283200;

void Texel() {
  TTInit(1);

  int numParams = 0;
  TexelParam params[1024];

  addParams(params, &numParams);

  int n = 0;
  Position* positions = LoadPositions(&n);

  if (CHOOSE_K)
    determineK(positions, n);

  SGD(params, numParams, positions, n);
  free(positions);
}

void SGD(TexelParam* params, int numParams, Position* positions, int numPositions) {
  double a = Alpha;
  const double b1 = Beta1;
  const double b2 = Beta2;
  const double epsilon = 1.0e-8;

  double best = totalError(positions, numPositions);

  double* M = calloc(2048, sizeof(double));
  double* V = calloc(2048, sizeof(double));

  for (int epoch = 1; epoch <= 100000; epoch++) {
    shufflePositions(positions, numPositions);

    double base = totalError(positions, min(numPositions, Batch));

    CalculateGradients(params, numParams, positions, min(numPositions, Batch), base);

    for (int i = 0; i < numParams; i++) {
      TexelParam param = params[i];

      for (int j = 0; j < param.count; j++) {
        TScore* tScore = param.vals[j];

        for (int p = MG; p <= EG; p++) {
          Score original = (*tScore)[p];
          double gradient = param.gradients[j][p];
          int idx = param.idx + 2 * j + p;

          M[idx] = b1 * M[idx] + (1.0 - b1) * gradient;
          V[idx] = b2 * V[idx] + (1.0 - b2) * gradient * gradient;

          double mHat = M[idx] / (1 - pow(b1, epoch));
          double vHat = V[idx] / (1 - pow(b2, epoch));

          double delta = a * mHat / (sqrt(vHat) + epsilon);

          Score sScore = scaleDown(original, param.min, param.max);
          sScore = max(0.0, min(1.0, sScore - delta));
          (*tScore)[p] = scaleUp(sScore, param.min, param.max);
        }
      }
    }

    double curr = totalError(positions, min(numPositions, Batch));
    PrintParams(params, numParams, base, curr, epoch);

    if (curr == base)
      break;
    else if (curr > base) {
      a /= 1.25;
      printf("Adjusted learning rate to %.4f\n", a);
    }
  }
}

void CalculateGradients(TexelParam* params, int numParams, Position* positions, int numPositions, double base) {
  for (int i = 0; i < numParams; i++)
    CalculateSingleGradient(params[i], positions, numPositions, base);
}

void CalculateSingleGradient(TexelParam param, Position* positions, int numPositions, double base) {
  const double dx = 1;

  for (int i = 0; i < param.count; i++) {
    TScore* tScore = param.vals[i];

    for (int p = MG; p <= EG; p++) {
      Score original = (*tScore)[p];

      (*tScore)[p] = original + dx;
      double ep1 = totalError(positions, numPositions);
      param.gradients[i][p] = (ep1 - base) / dx;

      (*tScore)[p] = original;
    }
  }
}

Position* LoadPositions(int* n) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = malloc(sizeof(Position) * MaxPositions);
  Board* board = malloc(sizeof(Board));

  char buffer[128];

  int p = 0;
  while (p < MaxPositions && fgets(buffer, 128, fp)) {
    int i;
    for (i = 0; i < 128; i++)
      if (!strncmp(buffer + i, "c2", 2))
        break;

    // copy in fen
    memcpy(positions[p].fen, buffer, i * sizeof(char));
    sscanf(buffer + i, "c2 \"%lf\"", &positions[p].result);

    p++;
  }

  if (FILTER) {
    SearchData* data = malloc(sizeof(SearchData));

    SearchParams* params = malloc(sizeof(SearchParams));
    params->endTime = 0;

    PV* pv = malloc(sizeof(PV));

    double totalPhase = 0;
    int i = 0;
    while (i < p) {
      Position* pos = &positions[i];
      ParseFen(pos->fen, board);

      ClearSearchData(data);
      data->board = board;

      pv->count = 0;
      Quiesce(-CHECKMATE, CHECKMATE, params, data, pv);

      for (int m = 0; m < pv->count; m++)
        MakeMove(pv->moves[m], board);

      if (REMOVE_CHECKS && board->checkers) {
        positions[i] = positions[--p];
        continue;
      } else {
        BoardToFen(pos->fen, board);

        totalPhase += GetPhase(board) / MaxPhase();

        i++;

        if (!(i & 4095))
          printf("Running search... (%d of %d)\n", i, p);
      }
    }

    printf("Average phase: %f\n", totalPhase / p);
  }

  printf("Successfully loaded %d positions!\n", p);

  *n = p;
  return positions;
}

void determineK(Position* positions, int n) {
  double min = -10, max = 10, delta = 1, best = 1, error = 100;

  for (int p = 0; p < 10; p++) {
    printf("Determining K: (%.10f, %.10f, %.10f)\n", min, max, delta);

    while (min < max) {
      K = min;
      double e = totalError(positions, n);
      if (e < error) {
        error = e;
        best = K;
        printf("New best K of %.10f, Error %.10f\n", K, error);
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
    jobs[t].positions = positions + (t * chunkSize);
    jobs[t].n = t != THREADS - 1 ? chunkSize : (n - ((THREADS - 1) * chunkSize));
    jobs[t].error = 0;

    pthread_create(&threads[t], NULL, batchError, (void*)(jobs + t));
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  double sum = 0;
  for (int t = 0; t < THREADS; t++)
    sum += jobs[t].error;

  free(errors);
  return sum / n;
}

void* batchError(void* arg) {
  BatchJob* batch = (BatchJob*)arg;
  batch->error = 0;

  for (int i = 0; i < batch->n; i++) {
    batch->error += error(batch->positions + i);
  }

  pthread_exit(NULL);

  return NULL;
}

double error(Position* p) {
  Board* board = malloc(sizeof(Board));
  ParseFen(p->fen, board);

  Score score;
  if (QS) {
    SearchData* data = malloc(sizeof(SearchData));
    ClearSearchData(data);
    data->board = board;

    SearchParams* params = malloc(sizeof(SearchParams));
    params->endTime = 0;

    PV* pv = malloc(sizeof(PV));
    pv->count = 0;

    score = Quiesce(-CHECKMATE, CHECKMATE, params, data, pv);

    free(data);
    free(params);
    free(pv);
  } else {
    score = Evaluate(board);
  }

  if (board->side)
    score = -score;

  score /= 100;

  free(board);
  return pow(p->result - sigmoid(score), 2);
}

double sigmoid(Score score) { return 1.0 / (1.0 + exp(-K * score)); }

void PrintParams(TexelParam* params, int numParams, double best, double current, int epoch) {
  printf("\n\nCurrent Values at Epoch %d:\n", epoch);
  printf("Base: %9.8f, Diff: %9.8f, Diff: %12.8f\n\n", best, current, (best - current) * 10e6);

  for (int i = 0; i < numParams; i++) {
    TexelParam param = params[i];

    printf("%s", param.name);
    for (int j = 0; j < param.count; j++) {
      if (j % 8 == 0)
        printf("\n");

      printf(" {%6.2f,%6.2f},", param.vals[j][MG], param.vals[j][EG]);
    }
    printf("\n\n");
  }

  FILE* fp;

  fp = fopen("texel-out.log", "a");
  if (fp == NULL)
    return;

  fprintf(fp, "\n// Epoch: %d, Base: %9.8f, Diff: %9.8f, Diff: %12.8f\n\n", epoch, best, current,
          (best - current) * 10e6);
  for (int i = 0; i < numParams; i++) {
    TexelParam param = params[i];

    if (param.count > 1) {
      fprintf(fp, "TScore %s[%d] = {", param.name, param.count);
      for (int j = 0; j < param.count; j++) {

        fprintf(fp, "{%d,%d}", (int)param.vals[j][MG], (int)param.vals[j][EG]);
        if (j < param.count - 1)
          fprintf(fp, ", ");
      }
      fprintf(fp, "};\n");
    } else {
      fprintf(fp, "TScore %s = {%d,%d};\n", param.name, (int)param.vals[0][MG], (int)param.vals[0][EG]);
    }
  }

  fclose(fp);
}

// 0 - 1 based on scale
double scaleDown(Score val, Score min, Score max) { return (val - min) / (max - min); }

Score scaleUp(Score val, Score min, Score max) { return (val * (max - min)) + min; }

void addParamBounded(char* name, int count, TScore* score, Score min, Score max, TexelParam* params, int* n) {
  int idx = (*n == 0) ? 0 : params[*n - 1].idx + params[*n - 1].count * 2;

  params[*n].idx = idx;
  params[*n].count = count;

  params[*n].min = min;
  params[*n].max = max;

  params[*n].name = name;
  params[*n].vals = score;
  params[*n].gradients = malloc(count * sizeof(TexelGradient));

  *n += 1;

  printf("Tuning param %s with count %d, starting at %d\n", name, count, idx);
}

void addParams(TexelParam* params, int* numParams) {
  if (TUNE_MATERIAL)
    addParamBounded("MATERIAL_VALUES", 5, &MATERIAL_VALUES, 0, 2000, params, numParams);

  if (TUNE_PAWN_PSQT)
    addParamBounded("PAWN_PSQT", 32, &PAWN_PSQT, -200, 200, params, numParams);

  if (TUNE_KNIGHT_PSQT)
    addParamBounded("KNIGHT_PSQT", 32, &KNIGHT_PSQT, -200, 200, params, numParams);

  if (TUNE_BISHOP_PSQT)
    addParamBounded("BISHOP_PSQT", 32, &BISHOP_PSQT, -200, 200, params, numParams);

  if (TUNE_ROOK_PSQT)
    addParamBounded("ROOK_PSQT", 32, &ROOK_PSQT, -200, 200, params, numParams);

  if (TUNE_QUEEN_PSQT)
    addParamBounded("QUEEN_PSQT", 32, &QUEEN_PSQT, -200, 200, params, numParams);

  if (TUNE_KING_PSQT)
    addParamBounded("KING_PSQT", 32, &KING_PSQT, -200, 200, params, numParams);

  if (TUNE_MINOR_PARAMS) {
    // addParamBounded("BISHOP_PAIR", 1, &BISHOP_PAIR, 0, 100, params, numParams);
    // addParamBounded("BISHOP_TRAPPED", 1, &BISHOP_TRAPPED, -200, 0, params, numParams);
    // addParamBounded("BISHOP_POST_PSQT", 32, &BISHOP_POST_PSQT, 0, 100, params, numParams);
    // addParamBounded("KNIGHT_POST_PSQT", 32, &KNIGHT_POST_PSQT, 0, 100, params, numParams);
    addParamBounded("KNIGHT_OUTPOST_REACHABLE", 1, &KNIGHT_OUTPOST_REACHABLE, 0, 100, params, numParams);
    addParamBounded("BISHOP_OUTPOST_REACHABLE", 1, &BISHOP_OUTPOST_REACHABLE, 0, 100, params, numParams);
  }

  if (TUNE_KNIGHT_MOBILITIES)
    addParamBounded("KNIGHT_MOBILITIES", 9, &KNIGHT_MOBILITIES, -200, 200, params, numParams);

  if (TUNE_BISHOP_MOBILITIES)
    addParamBounded("BISHOP_MOBILITIES", 14, &BISHOP_MOBILITIES, -200, 200, params, numParams);

  if (TUNE_ROOK_MOBILITIES)
    addParamBounded("ROOK_MOBILITIES", 15, &ROOK_MOBILITIES, -200, 200, params, numParams);

  if (TUNE_QUEEN_MOBILITIES)
    addParamBounded("QUEEN_MOBILITIES", 28, &QUEEN_MOBILITIES, -200, 200, params, numParams);

  if (TUNE_PAWN_PARAMS) {
    addParamBounded("DOUBLED_PAWN", 1, &DOUBLED_PAWN, -50, 0, params, numParams);
    addParamBounded("OPPOSED_ISOLATED_PAWN", 1, &OPPOSED_ISOLATED_PAWN, -50, 0, params, numParams);
    addParamBounded("OPEN_ISOLATED_PAWN", 1, &OPEN_ISOLATED_PAWN, -50, 0, params, numParams);
    addParamBounded("BACKWARDS_PAWN", 1, &BACKWARDS_PAWN, -50, 0, params, numParams);

    addParamBounded("CONNECTED_PAWN", 8, &CONNECTED_PAWN, 0, 100, params, numParams);
    addParamBounded("PASSED_PAWN", 8, &PASSED_PAWN, 0, 200, params, numParams);

    addParamBounded("PASSED_PAWN_ADVANCE_DEFENDED", 1, &PASSED_PAWN_ADVANCE_DEFENDED, 0, 50, params, numParams);
    addParamBounded("PASSED_PAWN_EDGE_DISTANCE", 1, &PASSED_PAWN_EDGE_DISTANCE, -50, 0, params, numParams);
    addParamBounded("PASSED_PAWN_KING_PROXIMITY", 1, &PASSED_PAWN_KING_PROXIMITY, -50, 50, params, numParams);
  }

  if (TUNE_ROOK_PARAMS) {
    addParamBounded("ROOK_OPEN_FILE", 1, &ROOK_OPEN_FILE, 0, 50, params, numParams);
    addParamBounded("ROOK_SEMI_OPEN", 1, &ROOK_SEMI_OPEN, 0, 50, params, numParams);
    addParamBounded("ROOK_SEVENTH_RANK", 1, &ROOK_SEVENTH_RANK, 0, 50, params, numParams);
    addParamBounded("ROOK_OPPOSITE_KING", 1, &ROOK_OPPOSITE_KING, -50, 50, params, numParams);
    addParamBounded("ROOK_ADJACENT_KING", 1, &ROOK_ADJACENT_KING, -50, 50, params, numParams);
    addParamBounded("ROOK_TRAPPED", 1, &ROOK_TRAPPED, -200, 0, params, numParams);
  }

  if (TUNE_THREATS) {
    addParamBounded("KNIGHT_THREATS", 6, &KNIGHT_THREATS, -200, 200, params, numParams);
    addParamBounded("BISHOP_THREATS", 6, &BISHOP_THREATS, -200, 200, params, numParams);
    addParamBounded("ROOK_THREATS", 6, &ROOK_THREATS, -200, 200, params, numParams);
    addParamBounded("KING_THREATS", 6, &KING_THREATS, -200, 200, params, numParams);
  }

  if (TUNE_SHELTER_STORM) {
    // TODO
  }

  if (TUNE_TEMPO)
    addParamBounded("TEMPO", 1, &TEMPO, 0, 50, params, numParams);

  if (TUNE_KING_SAFETY) {
    // TODO
  }
}

void shufflePositions(Position* positions, int n) {
  for (int i = 0; i < n; i++) {
    int A = RandomUInt64() % n;
    int B = RandomUInt64() % n;

    Position temp = positions[A];
    positions[A] = positions[B];
    positions[B] = temp;
  }
}
#endif