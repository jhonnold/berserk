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
#include "search.h"
#include "thread.h"
#include "tune.h"
#include "util.h"

const int MAX_POSITIONS = 4000000;

const int sideScalar[2] = {1, -1};

double ALPHA = 0.001;
const double BETA1 = 0.9;
const double BETA2 = 0.999;
const double EPSILON = 1e-8;

double K = 1;

void Tune() {
  Weights weights = {0};

  InitMaterialWeights(&weights);
  InitPsqtWeights(&weights);
  InitPostPsqtWeights(&weights);
  InitMobilityWeights(&weights);
  InitThreatWeights(&weights);
  InitPieceBonusWeights(&weights);
  InitPawnBonusWeights(&weights);
  InitPasserBonusWeights(&weights);

  int n = 0;
  Position* positions = LoadPositions(&n);
  ALPHA *= sqrt(n);

  ValidateEval(n, positions, &weights);
  DetermineK(n, positions);

  for (int epoch = 0; epoch < 10000; epoch++) {
    UpdateAndTrain(epoch, n, positions, &weights);

    if (epoch % 25 == 0)
      PrintWeights(&weights);
  }

  PrintWeights(&weights);
  free(positions);
}

void ValidateEval(int n, Position* positions, Weights* weights) {
#pragma omp parallel for schedule(static) num_threads(THREADS)
  for (int i = 0; i < n; i++) {
    double eval = EvaluateCoeffs(&positions[i], weights);
    if (fabs(positions[i].staticEval - eval) > 1) {
      printf("The coefficient based evaluation does NOT match the eval!\n");
      printf("Static: %d, Coeffs: %f\n", positions[i].staticEval, eval);
      exit(1);
    }

    if (i % 4096 == 0)
      printf("Validated %d position evaluations...\n", i);
  }
}

void DetermineK(int n, Position* positions) {
  double min = -10, max = 10, delta = 1, best = 1, error = 100;

  for (int p = 0; p < 10; p++) {
    printf("Determining K: (%.9f, %.9f, %.9f)\n", min, max, delta);

    while (min < max) {
      K = min;

      double e = 0;

#pragma omp parallel for schedule(static) num_threads(THREADS) reduction(+ : e)
      for (int i = 0; i < n; i++) {

        double sigmoid = Sigmoid(positions[i].staticEval);
        double difference = sigmoid - positions[i].result;
        e += pow(difference, 2);
      }

      e /= n;

      if (e < error) {
        error = e;
        best = K;
        printf("New best K of %.9f, Error %.9f\n", K, error);
      }
      min += delta;
    }

    min = best - delta;
    max = best + delta;
    delta /= 10;
  }

  K = best;
  printf("Using K of %.9f\n", K);
}

void UpdateParam(Param* p) {
  p->epoch++;

  if (!p->g)
    return;

  p->M = BETA1 * p->M + (1.0 - BETA1) * p->g;
  p->V = BETA2 * p->V + (1.0 - BETA2) * p->g * p->g;

  double mHat = p->M / (1 - pow(BETA1, p->epoch));
  double vHat = p->V / (1 - pow(BETA2, p->epoch));
  double delta = ALPHA * mHat / (sqrt(vHat) + EPSILON);

  p->value += delta;

  p->g = 0;
}

void UpdateWeights(Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    UpdateParam(&weights->material[pc].mg);
    UpdateParam(&weights->material[pc].eg);
  }

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      UpdateParam(&weights->psqt[pc][sq].mg);
      UpdateParam(&weights->psqt[pc][sq].eg);
    }
  }

  for (int sq = 0; sq < 32; sq++) {
    UpdateParam(&weights->knightPostPsqt[sq].mg);
    UpdateParam(&weights->knightPostPsqt[sq].eg);

    UpdateParam(&weights->bishopPostPsqt[sq].mg);
    UpdateParam(&weights->bishopPostPsqt[sq].eg);
  }

  for (int c = 0; c < 9; c++) {
    UpdateParam(&weights->knightMobilities[c].mg);
    UpdateParam(&weights->knightMobilities[c].eg);
  }

  for (int c = 0; c < 14; c++) {
    UpdateParam(&weights->bishopMobilities[c].mg);
    UpdateParam(&weights->bishopMobilities[c].eg);
  }

  for (int c = 0; c < 15; c++) {
    UpdateParam(&weights->rookMobilities[c].mg);
    UpdateParam(&weights->rookMobilities[c].eg);
  }

  for (int c = 0; c < 28; c++) {
    UpdateParam(&weights->queenMobilities[c].mg);
    UpdateParam(&weights->queenMobilities[c].eg);
  }

  for (int pc = 0; pc < 6; pc++) {
    UpdateParam(&weights->knightThreats[pc].mg);
    UpdateParam(&weights->knightThreats[pc].eg);

    UpdateParam(&weights->bishopThreats[pc].mg);
    UpdateParam(&weights->bishopThreats[pc].eg);

    UpdateParam(&weights->rookThreats[pc].mg);
    UpdateParam(&weights->rookThreats[pc].eg);

    UpdateParam(&weights->kingThreats[pc].mg);
    UpdateParam(&weights->kingThreats[pc].eg);
  }

  UpdateParam(&weights->hangingThreat.mg);
  UpdateParam(&weights->hangingThreat.eg);

  UpdateParam(&weights->bishopPair.mg);
  UpdateParam(&weights->bishopPair.eg);

  UpdateParam(&weights->knightPostReachable.mg);
  UpdateParam(&weights->knightPostReachable.eg);

  UpdateParam(&weights->bishopPostReachable.mg);
  UpdateParam(&weights->bishopPostReachable.eg);

  UpdateParam(&weights->bishopTrapped.mg);
  UpdateParam(&weights->bishopTrapped.eg);

  UpdateParam(&weights->rookTrapped.mg);
  UpdateParam(&weights->rookTrapped.eg);

  UpdateParam(&weights->rookOpenFile.mg);
  UpdateParam(&weights->rookOpenFile.eg);

  UpdateParam(&weights->rookSemiOpen.mg);
  UpdateParam(&weights->rookSemiOpen.eg);

  UpdateParam(&weights->rookOppositeKing.mg);
  UpdateParam(&weights->rookOppositeKing.eg);

  UpdateParam(&weights->rookAdjacentKing.mg);
  UpdateParam(&weights->rookAdjacentKing.eg);

  UpdateParam(&weights->doubledPawns.mg);
  UpdateParam(&weights->doubledPawns.eg);

  UpdateParam(&weights->opposedIsolatedPawns.mg);
  UpdateParam(&weights->opposedIsolatedPawns.eg);

  UpdateParam(&weights->openIsolatedPawns.mg);
  UpdateParam(&weights->openIsolatedPawns.eg);

  UpdateParam(&weights->backwardsPawns.mg);
  UpdateParam(&weights->backwardsPawns.eg);

  for (int r = 0; r < 8; r++) {
    UpdateParam(&weights->connectedPawn[r].mg);
    UpdateParam(&weights->connectedPawn[r].eg);

    UpdateParam(&weights->candidatePasser[r].mg);
    UpdateParam(&weights->candidatePasser[r].eg);
  }

  for (int r = 0; r < 8; r++) {
    UpdateParam(&weights->passedPawn[r].mg);
    UpdateParam(&weights->passedPawn[r].eg);
  }

  UpdateParam(&weights->passedPawnEdgeDistance.mg);
  UpdateParam(&weights->passedPawnEdgeDistance.eg);

  UpdateParam(&weights->passedPawnKingProximity.mg);
  UpdateParam(&weights->passedPawnKingProximity.eg);

  UpdateParam(&weights->passedPawnAdvance.mg);
  UpdateParam(&weights->passedPawnAdvance.eg);
}

void UpdateAndTrain(int epoch, int n, Position* positions, Weights* weights) {
  pthread_t threads[THREADS];
  GradientUpdate jobs[THREADS];
  Weights local[THREADS];

  int chunk = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    memcpy(&local[t], weights, sizeof(Weights));

    jobs[t].error = 0.0;
    jobs[t].n = t < THREADS - 1 ? chunk : (n - ((THREADS - 1) * chunk));
    jobs[t].positions = &positions[t * chunk];
    jobs[t].weights = &local[t];

    pthread_create(&threads[t], NULL, &UpdateGradients, &jobs[t]);
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  double error = 0;
  for (int t = 0; t < THREADS; t++) {
    error += jobs[t].error;

    Weights* w = jobs[t].weights;

    for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
      weights->material[pc].mg.g += w->material[pc].mg.g;
      weights->material[pc].eg.g += w->material[pc].eg.g;
    }

    weights->bishopPair.mg.g += w->bishopPair.mg.g;
    weights->bishopPair.eg.g += w->bishopPair.eg.g;

    for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
      for (int sq = 0; sq < 32; sq++) {
        weights->psqt[pc][sq].mg.g += w->psqt[pc][sq].mg.g;
        weights->psqt[pc][sq].eg.g += w->psqt[pc][sq].eg.g;
      }
    }

    for (int sq = 0; sq < 32; sq++) {
      weights->knightPostPsqt[sq].mg.g += w->knightPostPsqt[sq].mg.g;
      weights->knightPostPsqt[sq].eg.g += w->knightPostPsqt[sq].eg.g;

      weights->bishopPostPsqt[sq].mg.g += w->bishopPostPsqt[sq].mg.g;
      weights->bishopPostPsqt[sq].eg.g += w->bishopPostPsqt[sq].eg.g;
    }

    for (int c = 0; c < 9; c++) {
      weights->knightMobilities[c].mg.g += w->knightMobilities[c].mg.g;
      weights->knightMobilities[c].eg.g += w->knightMobilities[c].eg.g;
    }

    for (int c = 0; c < 14; c++) {
      weights->bishopMobilities[c].mg.g += w->bishopMobilities[c].mg.g;
      weights->bishopMobilities[c].eg.g += w->bishopMobilities[c].eg.g;
    }

    for (int c = 0; c < 15; c++) {
      weights->rookMobilities[c].mg.g += w->rookMobilities[c].mg.g;
      weights->rookMobilities[c].eg.g += w->rookMobilities[c].eg.g;
    }

    for (int c = 0; c < 28; c++) {
      weights->queenMobilities[c].mg.g += w->queenMobilities[c].mg.g;
      weights->queenMobilities[c].eg.g += w->queenMobilities[c].eg.g;
    }

    for (int pc = 0; pc < 6; pc++) {
      weights->knightThreats[pc].mg.g += w->knightThreats[pc].mg.g;
      weights->knightThreats[pc].eg.g += w->knightThreats[pc].eg.g;

      weights->bishopThreats[pc].mg.g += w->bishopThreats[pc].mg.g;
      weights->bishopThreats[pc].eg.g += w->bishopThreats[pc].eg.g;

      weights->rookThreats[pc].mg.g += w->rookThreats[pc].mg.g;
      weights->rookThreats[pc].eg.g += w->rookThreats[pc].eg.g;

      weights->kingThreats[pc].mg.g += w->kingThreats[pc].mg.g;
      weights->kingThreats[pc].eg.g += w->kingThreats[pc].eg.g;
    }

    weights->hangingThreat.mg.g += w->hangingThreat.mg.g;
    weights->hangingThreat.eg.g += w->hangingThreat.eg.g;

    weights->knightPostReachable.mg.g += w->knightPostReachable.mg.g;
    weights->knightPostReachable.eg.g += w->knightPostReachable.eg.g;

    weights->bishopPostReachable.mg.g += w->bishopPostReachable.mg.g;
    weights->bishopPostReachable.eg.g += w->bishopPostReachable.eg.g;

    weights->bishopTrapped.mg.g += w->bishopTrapped.mg.g;
    weights->bishopTrapped.eg.g += w->bishopTrapped.eg.g;

    weights->rookTrapped.mg.g += w->rookTrapped.mg.g;
    weights->rookTrapped.eg.g += w->rookTrapped.eg.g;

    weights->rookOpenFile.mg.g += w->rookOpenFile.mg.g;
    weights->rookOpenFile.eg.g += w->rookOpenFile.eg.g;

    weights->rookSemiOpen.mg.g += w->rookSemiOpen.mg.g;
    weights->rookSemiOpen.eg.g += w->rookSemiOpen.eg.g;

    weights->rookOppositeKing.mg.g += w->rookOppositeKing.mg.g;
    weights->rookOppositeKing.eg.g += w->rookOppositeKing.eg.g;

    weights->rookAdjacentKing.mg.g += w->rookAdjacentKing.mg.g;
    weights->rookAdjacentKing.eg.g += w->rookAdjacentKing.eg.g;

    weights->doubledPawns.mg.g += w->doubledPawns.mg.g;
    weights->doubledPawns.eg.g += w->doubledPawns.eg.g;

    weights->opposedIsolatedPawns.mg.g += w->opposedIsolatedPawns.mg.g;
    weights->opposedIsolatedPawns.eg.g += w->opposedIsolatedPawns.eg.g;

    weights->openIsolatedPawns.mg.g += w->openIsolatedPawns.mg.g;
    weights->openIsolatedPawns.eg.g += w->openIsolatedPawns.eg.g;

    weights->backwardsPawns.mg.g += w->backwardsPawns.mg.g;
    weights->backwardsPawns.eg.g += w->backwardsPawns.eg.g;

    for (int r = 0; r < 8; r++) {
      weights->connectedPawn[r].mg.g += w->connectedPawn[r].mg.g;
      weights->connectedPawn[r].eg.g += w->connectedPawn[r].eg.g;

      weights->candidatePasser[r].mg.g += w->candidatePasser[r].mg.g;
      weights->candidatePasser[r].eg.g += w->candidatePasser[r].eg.g;
    }

    for (int r = 0; r < 8; r++) {
      weights->passedPawn[r].mg.g += w->passedPawn[r].mg.g;
      weights->passedPawn[r].eg.g += w->passedPawn[r].eg.g;
    }

    weights->passedPawnEdgeDistance.mg.g += w->passedPawnEdgeDistance.mg.g;
    weights->passedPawnEdgeDistance.eg.g += w->passedPawnEdgeDistance.eg.g;

    weights->passedPawnKingProximity.mg.g += w->passedPawnKingProximity.mg.g;
    weights->passedPawnKingProximity.eg.g += w->passedPawnKingProximity.eg.g;

    weights->passedPawnAdvance.mg.g += w->passedPawnAdvance.mg.g;
    weights->passedPawnAdvance.eg.g += w->passedPawnAdvance.eg.g;
  }

  printf("Epoch: %5d, Error: %9.8f\n", epoch, error / n);

  UpdateWeights(weights);
}

void* UpdateGradients(void* arg) {
  GradientUpdate* job = (GradientUpdate*)arg;

  int n = job->n;
  Weights* weights = job->weights;
  Position* positions = job->positions;

  for (int i = 0; i < n; i++) {
    double actual = EvaluateCoeffs(&positions[i], weights);

    double sigmoid = Sigmoid(actual);
    double loss = (positions[i].result - sigmoid) * sigmoid * (1 - sigmoid);

    UpdateMaterialGradients(&positions[i], loss, weights);
    UpdatePsqtGradients(&positions[i], loss, weights);
    UpdatePostPsqtGradients(&positions[i], loss, weights);
    UpdateMobilityGradients(&positions[i], loss, weights);
    UpdateThreatGradients(&positions[i], loss, weights);
    UpdatePieceBonusGradients(&positions[i], loss, weights);
    UpdatePawnBonusGradients(&positions[i], loss, weights);
    UpdatePasserBonusGradients(&positions[i], loss, weights);

    job->error += pow(positions[i].result - sigmoid, 2);
  }

  return NULL;
}

void UpdateMaterialGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->material[pc].mg.g += (position->coeffs.pieces[WHITE][pc] - position->coeffs.pieces[BLACK][pc]) * mgBase;
    weights->material[pc].eg.g += (position->coeffs.pieces[WHITE][pc] - position->coeffs.pieces[BLACK][pc]) * egBase;
  }
}

void UpdatePsqtGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      weights->psqt[pc][sq].mg.g +=
          (position->coeffs.psqt[WHITE][pc][sq] - position->coeffs.psqt[BLACK][pc][sq]) * mgBase;
      weights->psqt[pc][sq].eg.g +=
          (position->coeffs.psqt[WHITE][pc][sq] - position->coeffs.psqt[BLACK][pc][sq]) * egBase;
    }
  }
}

void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int sq = 0; sq < 32; sq++) {
    weights->knightPostPsqt[sq].mg.g +=
        (position->coeffs.knightPostPsqt[WHITE][sq] - position->coeffs.knightPostPsqt[BLACK][sq]) * mgBase;
    weights->knightPostPsqt[sq].eg.g +=
        (position->coeffs.knightPostPsqt[WHITE][sq] - position->coeffs.knightPostPsqt[BLACK][sq]) * egBase;

    weights->bishopPostPsqt[sq].mg.g +=
        (position->coeffs.bishopPostPsqt[WHITE][sq] - position->coeffs.bishopPostPsqt[BLACK][sq]) * mgBase;
    weights->bishopPostPsqt[sq].eg.g +=
        (position->coeffs.bishopPostPsqt[WHITE][sq] - position->coeffs.bishopPostPsqt[BLACK][sq]) * egBase;
  }
}

void UpdateMobilityGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.g +=
        (position->coeffs.knightMobilities[WHITE][c] - position->coeffs.knightMobilities[BLACK][c]) * mgBase;
    weights->knightMobilities[c].eg.g +=
        (position->coeffs.knightMobilities[WHITE][c] - position->coeffs.knightMobilities[BLACK][c]) * egBase;
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.g +=
        (position->coeffs.bishopMobilities[WHITE][c] - position->coeffs.bishopMobilities[BLACK][c]) * mgBase;
    weights->bishopMobilities[c].eg.g +=
        (position->coeffs.bishopMobilities[WHITE][c] - position->coeffs.bishopMobilities[BLACK][c]) * egBase;
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.g +=
        (position->coeffs.rookMobilities[WHITE][c] - position->coeffs.rookMobilities[BLACK][c]) * mgBase;
    weights->rookMobilities[c].eg.g +=
        (position->coeffs.rookMobilities[WHITE][c] - position->coeffs.rookMobilities[BLACK][c]) * egBase;
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.g +=
        (position->coeffs.queenMobilities[WHITE][c] - position->coeffs.queenMobilities[BLACK][c]) * mgBase;
    weights->queenMobilities[c].eg.g +=
        (position->coeffs.queenMobilities[WHITE][c] - position->coeffs.queenMobilities[BLACK][c]) * egBase;
  }
}

void UpdateThreatGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.g +=
        (position->coeffs.knightThreats[WHITE][pc] - position->coeffs.knightThreats[BLACK][pc]) * mgBase;
    weights->knightThreats[pc].eg.g +=
        (position->coeffs.knightThreats[WHITE][pc] - position->coeffs.knightThreats[BLACK][pc]) * egBase;

    weights->bishopThreats[pc].mg.g +=
        (position->coeffs.bishopThreats[WHITE][pc] - position->coeffs.bishopThreats[BLACK][pc]) * mgBase;
    weights->bishopThreats[pc].eg.g +=
        (position->coeffs.bishopThreats[WHITE][pc] - position->coeffs.bishopThreats[BLACK][pc]) * egBase;

    weights->rookThreats[pc].mg.g +=
        (position->coeffs.rookThreats[WHITE][pc] - position->coeffs.rookThreats[BLACK][pc]) * mgBase;
    weights->rookThreats[pc].eg.g +=
        (position->coeffs.rookThreats[WHITE][pc] - position->coeffs.rookThreats[BLACK][pc]) * egBase;

    weights->kingThreats[pc].mg.g +=
        (position->coeffs.kingThreats[WHITE][pc] - position->coeffs.kingThreats[BLACK][pc]) * mgBase;
    weights->kingThreats[pc].eg.g +=
        (position->coeffs.kingThreats[WHITE][pc] - position->coeffs.kingThreats[BLACK][pc]) * egBase;
  }

  weights->hangingThreat.mg.g +=
      (position->coeffs.hangingThreat[WHITE] - position->coeffs.hangingThreat[BLACK]) * mgBase;
  weights->hangingThreat.eg.g +=
      (position->coeffs.hangingThreat[WHITE] - position->coeffs.hangingThreat[BLACK]) * egBase;
}

void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  weights->bishopPair.mg.g += (position->coeffs.bishopPair[WHITE] - position->coeffs.bishopPair[BLACK]) * mgBase;
  weights->bishopPair.eg.g += (position->coeffs.bishopPair[WHITE] - position->coeffs.bishopPair[BLACK]) * egBase;

  weights->knightPostReachable.mg.g +=
      (position->coeffs.knightPostReachable[WHITE] - position->coeffs.knightPostReachable[BLACK]) * mgBase;
  weights->knightPostReachable.eg.g +=
      (position->coeffs.knightPostReachable[WHITE] - position->coeffs.knightPostReachable[BLACK]) * egBase;

  weights->bishopPostReachable.mg.g +=
      (position->coeffs.bishopPostReachable[WHITE] - position->coeffs.bishopPostReachable[BLACK]) * mgBase;
  weights->bishopPostReachable.eg.g +=
      (position->coeffs.bishopPostReachable[WHITE] - position->coeffs.bishopPostReachable[BLACK]) * egBase;

  weights->bishopTrapped.mg.g +=
      (position->coeffs.bishopTrapped[WHITE] - position->coeffs.bishopTrapped[BLACK]) * mgBase;
  weights->bishopTrapped.eg.g +=
      (position->coeffs.bishopTrapped[WHITE] - position->coeffs.bishopTrapped[BLACK]) * egBase;

  weights->rookTrapped.mg.g += (position->coeffs.rookTrapped[WHITE] - position->coeffs.rookTrapped[BLACK]) * mgBase;
  weights->rookTrapped.eg.g += (position->coeffs.rookTrapped[WHITE] - position->coeffs.rookTrapped[BLACK]) * egBase;

  weights->rookOpenFile.mg.g += (position->coeffs.rookOpenFile[WHITE] - position->coeffs.rookOpenFile[BLACK]) * mgBase;
  weights->rookOpenFile.eg.g += (position->coeffs.rookOpenFile[WHITE] - position->coeffs.rookOpenFile[BLACK]) * egBase;

  weights->rookSemiOpen.mg.g += (position->coeffs.rookSemiOpen[WHITE] - position->coeffs.rookSemiOpen[BLACK]) * mgBase;
  weights->rookSemiOpen.eg.g += (position->coeffs.rookSemiOpen[WHITE] - position->coeffs.rookSemiOpen[BLACK]) * egBase;

  weights->rookOppositeKing.mg.g +=
      (position->coeffs.rookOppositeKing[WHITE] - position->coeffs.rookOppositeKing[BLACK]) * mgBase;
  weights->rookOppositeKing.eg.g +=
      (position->coeffs.rookOppositeKing[WHITE] - position->coeffs.rookOppositeKing[BLACK]) * egBase;

  weights->rookAdjacentKing.mg.g +=
      (position->coeffs.rookAdjacentKing[WHITE] - position->coeffs.rookAdjacentKing[BLACK]) * mgBase;
  weights->rookAdjacentKing.eg.g +=
      (position->coeffs.rookAdjacentKing[WHITE] - position->coeffs.rookAdjacentKing[BLACK]) * egBase;
}

void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  weights->doubledPawns.mg.g += (position->coeffs.doubledPawns[WHITE] - position->coeffs.doubledPawns[BLACK]) * mgBase;
  weights->doubledPawns.eg.g += (position->coeffs.doubledPawns[WHITE] - position->coeffs.doubledPawns[BLACK]) * egBase;

  weights->opposedIsolatedPawns.mg.g +=
      (position->coeffs.opposedIsolatedPawns[WHITE] - position->coeffs.opposedIsolatedPawns[BLACK]) * mgBase;
  weights->opposedIsolatedPawns.eg.g +=
      (position->coeffs.opposedIsolatedPawns[WHITE] - position->coeffs.opposedIsolatedPawns[BLACK]) * egBase;

  weights->openIsolatedPawns.mg.g +=
      (position->coeffs.openIsolatedPawns[WHITE] - position->coeffs.openIsolatedPawns[BLACK]) * mgBase;
  weights->openIsolatedPawns.eg.g +=
      (position->coeffs.openIsolatedPawns[WHITE] - position->coeffs.openIsolatedPawns[BLACK]) * egBase;

  weights->backwardsPawns.mg.g +=
      (position->coeffs.backwardsPawns[WHITE] - position->coeffs.backwardsPawns[BLACK]) * mgBase;
  weights->backwardsPawns.eg.g +=
      (position->coeffs.backwardsPawns[WHITE] - position->coeffs.backwardsPawns[BLACK]) * egBase;

  for (int r = 0; r < 8; r++) {
    weights->connectedPawn[r].mg.g +=
        (position->coeffs.connectedPawn[WHITE][r] - position->coeffs.connectedPawn[BLACK][r]) * mgBase;
    weights->connectedPawn[r].eg.g +=
        (position->coeffs.connectedPawn[WHITE][r] - position->coeffs.connectedPawn[BLACK][r]) * egBase;

    weights->candidatePasser[r].mg.g +=
        (position->coeffs.candidatePasser[WHITE][r] - position->coeffs.candidatePasser[BLACK][r]) * mgBase;
    weights->candidatePasser[r].eg.g +=
        (position->coeffs.candidatePasser[WHITE][r] - position->coeffs.candidatePasser[BLACK][r]) * egBase;
  }
}

void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.g +=
        (position->coeffs.passedPawn[WHITE][r] - position->coeffs.passedPawn[BLACK][r]) * mgBase;
    weights->passedPawn[r].eg.g +=
        (position->coeffs.passedPawn[WHITE][r] - position->coeffs.passedPawn[BLACK][r]) * egBase;
  }

  weights->passedPawnEdgeDistance.mg.g +=
      (position->coeffs.passedPawnEdgeDistance[WHITE] - position->coeffs.passedPawnEdgeDistance[BLACK]) * mgBase;
  weights->passedPawnEdgeDistance.eg.g +=
      (position->coeffs.passedPawnEdgeDistance[WHITE] - position->coeffs.passedPawnEdgeDistance[BLACK]) * egBase;

  weights->passedPawnKingProximity.mg.g +=
      (position->coeffs.passedPawnKingProximity[WHITE] - position->coeffs.passedPawnKingProximity[BLACK]) * mgBase;
  weights->passedPawnKingProximity.eg.g +=
      (position->coeffs.passedPawnKingProximity[WHITE] - position->coeffs.passedPawnKingProximity[BLACK]) * egBase;

  weights->passedPawnAdvance.mg.g +=
      (position->coeffs.passedPawnAdvance[WHITE] - position->coeffs.passedPawnAdvance[BLACK]) * mgBase;
  weights->passedPawnAdvance.eg.g +=
      (position->coeffs.passedPawnAdvance[WHITE] - position->coeffs.passedPawnAdvance[BLACK]) * egBase;
}

double EvaluateCoeffs(Position* position, Weights* weights) {
  double mg = 0, eg = 0;

  EvaluateMaterialValues(&mg, &eg, position, weights);
  EvaluatePsqtValues(&mg, &eg, position, weights);
  EvaluatePostPsqtValues(&mg, &eg, position, weights);
  EvaluateMobilityValues(&mg, &eg, position, weights);
  EvaluateThreatValues(&mg, &eg, position, weights);
  EvaluatePieceBonusValues(&mg, &eg, position, weights);
  EvaluatePawnBonusValues(&mg, &eg, position, weights);
  EvaluatePasserBonusValues(&mg, &eg, position, weights);

  mg += scoreMG(position->coeffs.ks[WHITE]) - scoreMG(position->coeffs.ks[BLACK]);
  eg += scoreEG(position->coeffs.ks[WHITE]) - scoreEG(position->coeffs.ks[BLACK]);

  int result = (mg * position->phase + eg * (24 - position->phase)) / 24;
  result *= position->scale;
  return result + (position->stm == WHITE ? TEMPO : -TEMPO);
}

void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    *mg += (position->coeffs.pieces[WHITE][pc] - position->coeffs.pieces[BLACK][pc]) * weights->material[pc].mg.value;
    *eg += (position->coeffs.pieces[WHITE][pc] - position->coeffs.pieces[BLACK][pc]) * weights->material[pc].eg.value;
  }
}

void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      *mg += (position->coeffs.psqt[WHITE][pc][sq] - position->coeffs.psqt[BLACK][pc][sq]) *
             weights->psqt[pc][sq].mg.value;
      *eg += (position->coeffs.psqt[WHITE][pc][sq] - position->coeffs.psqt[BLACK][pc][sq]) *
             weights->psqt[pc][sq].eg.value;
    }
  }
}

void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int sq = 0; sq < 32; sq++) {
    *mg += (position->coeffs.knightPostPsqt[WHITE][sq] - position->coeffs.knightPostPsqt[BLACK][sq]) *
           weights->knightPostPsqt[sq].mg.value;
    *eg += (position->coeffs.knightPostPsqt[WHITE][sq] - position->coeffs.knightPostPsqt[BLACK][sq]) *
           weights->knightPostPsqt[sq].eg.value;

    *mg += (position->coeffs.bishopPostPsqt[WHITE][sq] - position->coeffs.bishopPostPsqt[BLACK][sq]) *
           weights->bishopPostPsqt[sq].mg.value;
    *eg += (position->coeffs.bishopPostPsqt[WHITE][sq] - position->coeffs.bishopPostPsqt[BLACK][sq]) *
           weights->bishopPostPsqt[sq].eg.value;
  }
}

void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int c = 0; c < 9; c++) {
    *mg += (position->coeffs.knightMobilities[WHITE][c] - position->coeffs.knightMobilities[BLACK][c]) *
           weights->knightMobilities[c].mg.value;
    *eg += (position->coeffs.knightMobilities[WHITE][c] - position->coeffs.knightMobilities[BLACK][c]) *
           weights->knightMobilities[c].eg.value;
  }

  for (int c = 0; c < 14; c++) {
    *mg += (position->coeffs.bishopMobilities[WHITE][c] - position->coeffs.bishopMobilities[BLACK][c]) *
           weights->bishopMobilities[c].mg.value;
    *eg += (position->coeffs.bishopMobilities[WHITE][c] - position->coeffs.bishopMobilities[BLACK][c]) *
           weights->bishopMobilities[c].eg.value;
  }

  for (int c = 0; c < 15; c++) {
    *mg += (position->coeffs.rookMobilities[WHITE][c] - position->coeffs.rookMobilities[BLACK][c]) *
           weights->rookMobilities[c].mg.value;
    *eg += (position->coeffs.rookMobilities[WHITE][c] - position->coeffs.rookMobilities[BLACK][c]) *
           weights->rookMobilities[c].eg.value;
  }

  for (int c = 0; c < 28; c++) {
    *mg += (position->coeffs.queenMobilities[WHITE][c] - position->coeffs.queenMobilities[BLACK][c]) *
           weights->queenMobilities[c].mg.value;
    *eg += (position->coeffs.queenMobilities[WHITE][c] - position->coeffs.queenMobilities[BLACK][c]) *
           weights->queenMobilities[c].eg.value;
  }
}

void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    *mg += (position->coeffs.knightThreats[WHITE][pc] - position->coeffs.knightThreats[BLACK][pc]) *
           weights->knightThreats[pc].mg.value;
    *eg += (position->coeffs.knightThreats[WHITE][pc] - position->coeffs.knightThreats[BLACK][pc]) *
           weights->knightThreats[pc].eg.value;

    *mg += (position->coeffs.bishopThreats[WHITE][pc] - position->coeffs.bishopThreats[BLACK][pc]) *
           weights->bishopThreats[pc].mg.value;
    *eg += (position->coeffs.bishopThreats[WHITE][pc] - position->coeffs.bishopThreats[BLACK][pc]) *
           weights->bishopThreats[pc].eg.value;

    *mg += (position->coeffs.rookThreats[WHITE][pc] - position->coeffs.rookThreats[BLACK][pc]) *
           weights->rookThreats[pc].mg.value;
    *eg += (position->coeffs.rookThreats[WHITE][pc] - position->coeffs.rookThreats[BLACK][pc]) *
           weights->rookThreats[pc].eg.value;

    *mg += (position->coeffs.kingThreats[WHITE][pc] - position->coeffs.kingThreats[BLACK][pc]) *
           weights->kingThreats[pc].mg.value;
    *eg += (position->coeffs.kingThreats[WHITE][pc] - position->coeffs.kingThreats[BLACK][pc]) *
           weights->kingThreats[pc].eg.value;
  }

  *mg +=
      (position->coeffs.hangingThreat[WHITE] - position->coeffs.hangingThreat[BLACK]) * weights->hangingThreat.mg.value;
  *eg +=
      (position->coeffs.hangingThreat[WHITE] - position->coeffs.hangingThreat[BLACK]) * weights->hangingThreat.eg.value;
}

void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  *mg += (position->coeffs.bishopPair[WHITE] - position->coeffs.bishopPair[BLACK]) * weights->bishopPair.mg.value;
  *eg += (position->coeffs.bishopPair[WHITE] - position->coeffs.bishopPair[BLACK]) * weights->bishopPair.eg.value;
  
  *mg += (position->coeffs.knightPostReachable[WHITE] - position->coeffs.knightPostReachable[BLACK]) *
         weights->knightPostReachable.mg.value;
  *eg += (position->coeffs.knightPostReachable[WHITE] - position->coeffs.knightPostReachable[BLACK]) *
         weights->knightPostReachable.eg.value;

  *mg += (position->coeffs.bishopPostReachable[WHITE] - position->coeffs.bishopPostReachable[BLACK]) *
         weights->bishopPostReachable.mg.value;
  *eg += (position->coeffs.bishopPostReachable[WHITE] - position->coeffs.bishopPostReachable[BLACK]) *
         weights->bishopPostReachable.eg.value;

  *mg +=
      (position->coeffs.bishopTrapped[WHITE] - position->coeffs.bishopTrapped[BLACK]) * weights->bishopTrapped.mg.value;
  *eg +=
      (position->coeffs.bishopTrapped[WHITE] - position->coeffs.bishopTrapped[BLACK]) * weights->bishopTrapped.eg.value;

  *mg += (position->coeffs.rookTrapped[WHITE] - position->coeffs.rookTrapped[BLACK]) * weights->rookTrapped.mg.value;
  *eg += (position->coeffs.rookTrapped[WHITE] - position->coeffs.rookTrapped[BLACK]) * weights->rookTrapped.eg.value;

  *mg += (position->coeffs.rookOpenFile[WHITE] - position->coeffs.rookOpenFile[BLACK]) * weights->rookOpenFile.mg.value;
  *eg += (position->coeffs.rookOpenFile[WHITE] - position->coeffs.rookOpenFile[BLACK]) * weights->rookOpenFile.eg.value;

  *mg += (position->coeffs.rookSemiOpen[WHITE] - position->coeffs.rookSemiOpen[BLACK]) * weights->rookSemiOpen.mg.value;
  *eg += (position->coeffs.rookSemiOpen[WHITE] - position->coeffs.rookSemiOpen[BLACK]) * weights->rookSemiOpen.eg.value;

  *mg += (position->coeffs.rookOppositeKing[WHITE] - position->coeffs.rookOppositeKing[BLACK]) *
         weights->rookOppositeKing.mg.value;
  *eg += (position->coeffs.rookOppositeKing[WHITE] - position->coeffs.rookOppositeKing[BLACK]) *
         weights->rookOppositeKing.eg.value;

  *mg += (position->coeffs.rookAdjacentKing[WHITE] - position->coeffs.rookAdjacentKing[BLACK]) *
         weights->rookAdjacentKing.mg.value;
  *eg += (position->coeffs.rookAdjacentKing[WHITE] - position->coeffs.rookAdjacentKing[BLACK]) *
         weights->rookAdjacentKing.eg.value;
}

void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  *mg += (position->coeffs.doubledPawns[WHITE] - position->coeffs.doubledPawns[BLACK]) * weights->doubledPawns.mg.value;
  *eg += (position->coeffs.doubledPawns[WHITE] - position->coeffs.doubledPawns[BLACK]) * weights->doubledPawns.eg.value;

  *mg += (position->coeffs.opposedIsolatedPawns[WHITE] - position->coeffs.opposedIsolatedPawns[BLACK]) *
         weights->opposedIsolatedPawns.mg.value;
  *eg += (position->coeffs.opposedIsolatedPawns[WHITE] - position->coeffs.opposedIsolatedPawns[BLACK]) *
         weights->opposedIsolatedPawns.eg.value;

  *mg += (position->coeffs.openIsolatedPawns[WHITE] - position->coeffs.openIsolatedPawns[BLACK]) *
         weights->openIsolatedPawns.mg.value;
  *eg += (position->coeffs.openIsolatedPawns[WHITE] - position->coeffs.openIsolatedPawns[BLACK]) *
         weights->openIsolatedPawns.eg.value;

  *mg += (position->coeffs.backwardsPawns[WHITE] - position->coeffs.backwardsPawns[BLACK]) *
         weights->backwardsPawns.mg.value;
  *eg += (position->coeffs.backwardsPawns[WHITE] - position->coeffs.backwardsPawns[BLACK]) *
         weights->backwardsPawns.eg.value;

  for (int r = 0; r < 8; r++) {
    *mg += (position->coeffs.connectedPawn[WHITE][r] - position->coeffs.connectedPawn[BLACK][r]) *
           weights->connectedPawn[r].mg.value;
    *eg += (position->coeffs.connectedPawn[WHITE][r] - position->coeffs.connectedPawn[BLACK][r]) *
           weights->connectedPawn[r].eg.value;

    *mg += (position->coeffs.candidatePasser[WHITE][r] - position->coeffs.candidatePasser[BLACK][r]) *
           weights->candidatePasser[r].mg.value;
    *eg += (position->coeffs.candidatePasser[WHITE][r] - position->coeffs.candidatePasser[BLACK][r]) *
           weights->candidatePasser[r].eg.value;
  }
}

void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int r = 0; r < 8; r++) {
    *mg += (position->coeffs.passedPawn[WHITE][r] - position->coeffs.passedPawn[BLACK][r]) *
           weights->passedPawn[r].mg.value;
    *eg += (position->coeffs.passedPawn[WHITE][r] - position->coeffs.passedPawn[BLACK][r]) *
           weights->passedPawn[r].eg.value;
  }

  *mg += (position->coeffs.passedPawnEdgeDistance[WHITE] - position->coeffs.passedPawnEdgeDistance[BLACK]) *
         weights->passedPawnEdgeDistance.mg.value;
  *eg += (position->coeffs.passedPawnEdgeDistance[WHITE] - position->coeffs.passedPawnEdgeDistance[BLACK]) *
         weights->passedPawnEdgeDistance.eg.value;

  *mg += (position->coeffs.passedPawnKingProximity[WHITE] - position->coeffs.passedPawnKingProximity[BLACK]) *
         weights->passedPawnKingProximity.mg.value;
  *eg += (position->coeffs.passedPawnKingProximity[WHITE] - position->coeffs.passedPawnKingProximity[BLACK]) *
         weights->passedPawnKingProximity.eg.value;

  *mg += (position->coeffs.passedPawnAdvance[WHITE] - position->coeffs.passedPawnAdvance[BLACK]) *
         weights->passedPawnAdvance.mg.value;
  *eg += (position->coeffs.passedPawnAdvance[WHITE] - position->coeffs.passedPawnAdvance[BLACK]) *
         weights->passedPawnAdvance.eg.value;
}

void LoadPosition(Board* board, Position* position) {
  ResetCoeffs();
  int phase = GetPhase(board);

  position->phaseMg = phase / 24.0;
  position->phaseEg = 1 - phase / 24.0;
  position->phase = phase;

  position->stm = board->side;

  Score eval = Evaluate(board);
  position->staticEval = sideScalar[board->side] * eval;

  position->scale = Scale(board, C.ss) / 100.0;

  CopyCoeffsInto(&position->coeffs);
}

void ResetCoeffs() {
  for (int side = WHITE; side <= BLACK; side++) {
    for (int pc = 0; pc < 5; pc++)
      C.pieces[side][pc] = 0;

    for (int pc = 0; pc < 6; pc++)
      for (int sq = 0; sq < 32; sq++)
        C.psqt[side][pc][sq] = 0;

    C.bishopPair[side] = 0;
    C.bishopTrapped[side] = 0;

    for (int sq = 0; sq < 32; sq++) {
      C.knightPostPsqt[side][sq] = 0;
      C.bishopPostPsqt[side][sq] = 0;
    }

    C.knightPostReachable[side] = 0;
    C.bishopPostReachable[side] = 0;

    for (int c = 0; c < 9; c++)
      C.knightMobilities[side][c] = 0;
    for (int c = 0; c < 14; c++)
      C.bishopMobilities[side][c] = 0;
    for (int c = 0; c < 15; c++)
      C.rookMobilities[side][c] = 0;
    for (int c = 0; c < 28; c++)
      C.queenMobilities[side][c] = 0;

    C.doubledPawns[side] = 0;
    C.opposedIsolatedPawns[side] = 0;
    C.openIsolatedPawns[side] = 0;
    C.backwardsPawns[side] = 0;

    for (int r = 0; r < 8; r++) {
      C.connectedPawn[side][r] = 0;
      C.candidatePasser[side][r] = 0;
      C.passedPawn[side][r] = 0;
    }

    C.passedPawnAdvance[side] = 0;
    C.passedPawnEdgeDistance[side] = 0;
    C.passedPawnKingProximity[side] = 0;

    C.rookOpenFile[side] = 0;
    C.rookSemiOpen[side] = 0;
    C.rookOppositeKing[side] = 0;
    C.rookAdjacentKing[side] = 0;
    C.rookTrapped[side] = 0;
    C.rookOpenFile[side] = 0;

    for (int i = 0; i < 6; i++) {
      C.knightThreats[side][i] = 0;
      C.bishopThreats[side][i] = 0;
      C.rookThreats[side][i] = 0;
      C.kingThreats[side][i] = 0;
    }

    C.hangingThreat[side] = 0;

    C.ks[side] = 0;
  }
}

void CopyCoeffsInto(EvalCoeffs* c) {
  for (int side = WHITE; side <= BLACK; side++) {
    for (int pc = 0; pc < 5; pc++)
      c->pieces[side][pc] = C.pieces[side][pc];

    for (int pc = 0; pc < 6; pc++)
      for (int sq = 0; sq < 32; sq++)
        c->psqt[side][pc][sq] = C.psqt[side][pc][sq];

    c->bishopPair[side] = C.bishopPair[side];
    c->bishopTrapped[side] = C.bishopTrapped[side];

    for (int sq = 0; sq < 32; sq++) {
      c->knightPostPsqt[side][sq] = C.knightPostPsqt[side][sq];
      c->bishopPostPsqt[side][sq] = C.bishopPostPsqt[side][sq];
    }

    c->knightPostReachable[side] = C.knightPostReachable[side];
    c->bishopPostReachable[side] = C.bishopPostReachable[side];

    for (int d = 0; d < 9; d++)
      c->knightMobilities[side][d] = C.knightMobilities[side][d];
    for (int d = 0; d < 14; d++)
      c->bishopMobilities[side][d] = C.bishopMobilities[side][d];
    for (int d = 0; d < 15; d++)
      c->rookMobilities[side][d] = C.rookMobilities[side][d];
    for (int d = 0; d < 28; d++)
      c->queenMobilities[side][d] = C.queenMobilities[side][d];

    c->doubledPawns[side] = C.doubledPawns[side];
    c->opposedIsolatedPawns[side] = C.opposedIsolatedPawns[side];
    c->openIsolatedPawns[side] = C.openIsolatedPawns[side];
    c->backwardsPawns[side] = C.backwardsPawns[side];

    for (int r = 0; r < 8; r++) {
      c->connectedPawn[side][r] = C.connectedPawn[side][r];
      c->candidatePasser[side][r] = C.candidatePasser[side][r];
      c->passedPawn[side][r] = C.passedPawn[side][r];
    }

    c->passedPawnAdvance[side] = C.passedPawnAdvance[side];
    c->passedPawnEdgeDistance[side] = C.passedPawnEdgeDistance[side];
    c->passedPawnKingProximity[side] = C.passedPawnKingProximity[side];

    c->rookOpenFile[side] = C.rookOpenFile[side];
    c->rookSemiOpen[side] = C.rookSemiOpen[side];
    c->rookOppositeKing[side] = C.rookOppositeKing[side];
    c->rookAdjacentKing[side] = C.rookAdjacentKing[side];
    c->rookTrapped[side] = C.rookTrapped[side];
    c->rookOpenFile[side] = C.rookOpenFile[side];

    for (int i = 0; i < 6; i++) {
      c->knightThreats[side][i] = C.knightThreats[side][i];
      c->bishopThreats[side][i] = C.bishopThreats[side][i];
      c->rookThreats[side][i] = C.rookThreats[side][i];
      c->kingThreats[side][i] = C.kingThreats[side][i];
    }

    c->hangingThreat[side] = C.hangingThreat[side];

    c->ks[side] = C.ks[side];
  }
}

Position* LoadPositions(int* n) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = calloc(MAX_POSITIONS, sizeof(Position));

  Board board;
  ThreadData* threads = CreatePool(1);
  SearchParams params = {0};
  PV pv;

  char buffer[128];

  int p = 0;
  while (p < MAX_POSITIONS && fgets(buffer, 128, fp)) {
    int i;
    for (i = 0; i < 128; i++)
      if (!strncmp(buffer + i, "c2", 2))
        break;

    sscanf(buffer + i, "c2 \"%f\"", &positions[p].result);

    ParseFen(buffer, &board);
    ResetThreadPool(&board, &params, threads);
    Quiesce(-CHECKMATE, CHECKMATE, threads, &pv);

    for (int m = 0; m < pv.count; m++)
      MakeMove(pv.moves[m], &board);

    // if (board.checkers)
    //   continue;

    if (!(board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    if (bits(board.occupancies[BOTH]) == 3 && (board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    LoadPosition(&board, &positions[p]);
    if (positions[p].staticEval > 3000)
      continue;

    if (!(++p & 4095))
      printf("Loaded %d positions...\n", p);
  }

  printf("Successfully loaded %d positions.\n", p);

  *n = p;

  positions = realloc(positions, sizeof(Position) * p);
  return positions;
}

void InitMaterialWeights(Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->material[pc].mg.value = scoreMG(MATERIAL_VALUES[pc]);
    weights->material[pc].eg.value = scoreEG(MATERIAL_VALUES[pc]);
  }
}

void InitPsqtWeights(Weights* weights) {
  for (int sq = 0; sq < 32; sq++) {
    weights->psqt[PAWN_TYPE][sq].mg.value = scoreMG(PAWN_PSQT[sq]);
    weights->psqt[PAWN_TYPE][sq].eg.value = scoreEG(PAWN_PSQT[sq]);

    weights->psqt[KNIGHT_TYPE][sq].mg.value = scoreMG(KNIGHT_PSQT[sq]);
    weights->psqt[KNIGHT_TYPE][sq].eg.value = scoreEG(KNIGHT_PSQT[sq]);

    weights->psqt[BISHOP_TYPE][sq].mg.value = scoreMG(BISHOP_PSQT[sq]);
    weights->psqt[BISHOP_TYPE][sq].eg.value = scoreEG(BISHOP_PSQT[sq]);

    weights->psqt[ROOK_TYPE][sq].mg.value = scoreMG(ROOK_PSQT[sq]);
    weights->psqt[ROOK_TYPE][sq].eg.value = scoreEG(ROOK_PSQT[sq]);

    weights->psqt[QUEEN_TYPE][sq].mg.value = scoreMG(QUEEN_PSQT[sq]);
    weights->psqt[QUEEN_TYPE][sq].eg.value = scoreEG(QUEEN_PSQT[sq]);

    weights->psqt[KING_TYPE][sq].mg.value = scoreMG(KING_PSQT[sq]);
    weights->psqt[KING_TYPE][sq].eg.value = scoreEG(KING_PSQT[sq]);
  }
}

void InitPostPsqtWeights(Weights* weights) {
  for (int sq = 0; sq < 32; sq++) {
    weights->knightPostPsqt[sq].mg.value = scoreMG(KNIGHT_POST_PSQT[sq]);
    weights->knightPostPsqt[sq].eg.value = scoreEG(KNIGHT_POST_PSQT[sq]);

    weights->bishopPostPsqt[sq].mg.value = scoreMG(BISHOP_POST_PSQT[sq]);
    weights->bishopPostPsqt[sq].eg.value = scoreEG(BISHOP_POST_PSQT[sq]);
  }
}

void InitMobilityWeights(Weights* weights) {
  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.value = scoreMG(KNIGHT_MOBILITIES[c]);
    weights->knightMobilities[c].eg.value = scoreEG(KNIGHT_MOBILITIES[c]);
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.value = scoreMG(BISHOP_MOBILITIES[c]);
    weights->bishopMobilities[c].eg.value = scoreEG(BISHOP_MOBILITIES[c]);
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.value = scoreMG(ROOK_MOBILITIES[c]);
    weights->rookMobilities[c].eg.value = scoreEG(ROOK_MOBILITIES[c]);
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.value = scoreMG(QUEEN_MOBILITIES[c]);
    weights->queenMobilities[c].eg.value = scoreEG(QUEEN_MOBILITIES[c]);
  }
}

void InitThreatWeights(Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.value = scoreMG(KNIGHT_THREATS[pc]);
    weights->knightThreats[pc].eg.value = scoreEG(KNIGHT_THREATS[pc]);

    weights->bishopThreats[pc].mg.value = scoreMG(BISHOP_THREATS[pc]);
    weights->bishopThreats[pc].eg.value = scoreEG(BISHOP_THREATS[pc]);

    weights->rookThreats[pc].mg.value = scoreMG(ROOK_THREATS[pc]);
    weights->rookThreats[pc].eg.value = scoreEG(ROOK_THREATS[pc]);

    weights->kingThreats[pc].mg.value = scoreMG(KING_THREATS[pc]);
    weights->kingThreats[pc].eg.value = scoreEG(KING_THREATS[pc]);
  }

  weights->hangingThreat.mg.value = scoreMG(HANGING_THREAT);
  weights->hangingThreat.eg.value = scoreEG(HANGING_THREAT);
}

void InitPieceBonusWeights(Weights* weights) {
  weights->bishopPair.mg.value = scoreMG(BISHOP_PAIR);
  weights->bishopPair.eg.value = scoreEG(BISHOP_PAIR);

  weights->knightPostReachable.mg.value = scoreMG(KNIGHT_OUTPOST_REACHABLE);
  weights->knightPostReachable.eg.value = scoreEG(KNIGHT_OUTPOST_REACHABLE);

  weights->bishopPostReachable.mg.value = scoreMG(BISHOP_OUTPOST_REACHABLE);
  weights->bishopPostReachable.eg.value = scoreEG(BISHOP_OUTPOST_REACHABLE);

  weights->bishopTrapped.mg.value = scoreMG(BISHOP_TRAPPED);
  weights->bishopTrapped.eg.value = scoreEG(BISHOP_TRAPPED);

  weights->rookTrapped.mg.value = scoreMG(ROOK_TRAPPED);
  weights->rookTrapped.eg.value = scoreEG(ROOK_TRAPPED);

  weights->rookOpenFile.mg.value = scoreMG(ROOK_OPEN_FILE);
  weights->rookOpenFile.eg.value = scoreEG(ROOK_OPEN_FILE);

  weights->rookSemiOpen.mg.value = scoreMG(ROOK_SEMI_OPEN);
  weights->rookSemiOpen.eg.value = scoreEG(ROOK_SEMI_OPEN);

  weights->rookOppositeKing.mg.value = scoreMG(ROOK_OPPOSITE_KING);
  weights->rookOppositeKing.eg.value = scoreEG(ROOK_OPPOSITE_KING);

  weights->rookAdjacentKing.mg.value = scoreMG(ROOK_ADJACENT_KING);
  weights->rookAdjacentKing.eg.value = scoreEG(ROOK_ADJACENT_KING);
}

void InitPawnBonusWeights(Weights* weights) {
  weights->doubledPawns.mg.value = scoreMG(DOUBLED_PAWN);
  weights->doubledPawns.eg.value = scoreEG(DOUBLED_PAWN);

  weights->opposedIsolatedPawns.mg.value = scoreMG(OPPOSED_ISOLATED_PAWN);
  weights->opposedIsolatedPawns.eg.value = scoreEG(OPPOSED_ISOLATED_PAWN);

  weights->openIsolatedPawns.mg.value = scoreMG(OPEN_ISOLATED_PAWN);
  weights->openIsolatedPawns.eg.value = scoreEG(OPEN_ISOLATED_PAWN);

  weights->backwardsPawns.mg.value = scoreMG(BACKWARDS_PAWN);
  weights->backwardsPawns.eg.value = scoreEG(BACKWARDS_PAWN);

  for (int r = 0; r < 8; r++) {
    weights->connectedPawn[r].mg.value = scoreMG(CONNECTED_PAWN[r]);
    weights->connectedPawn[r].eg.value = scoreEG(CONNECTED_PAWN[r]);

    weights->candidatePasser[r].mg.value = scoreMG(CANDIDATE_PASSER[r]);
    weights->candidatePasser[r].eg.value = scoreEG(CANDIDATE_PASSER[r]);
  }
}

void InitPasserBonusWeights(Weights* weights) {
  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.value = scoreMG(PASSED_PAWN[r]);
    weights->passedPawn[r].eg.value = scoreEG(PASSED_PAWN[r]);
  }

  weights->passedPawnEdgeDistance.mg.value = scoreMG(PASSED_PAWN_EDGE_DISTANCE);
  weights->passedPawnEdgeDistance.eg.value = scoreEG(PASSED_PAWN_EDGE_DISTANCE);

  weights->passedPawnKingProximity.mg.value = scoreMG(PASSED_PAWN_KING_PROXIMITY);
  weights->passedPawnKingProximity.eg.value = scoreEG(PASSED_PAWN_KING_PROXIMITY);

  weights->passedPawnAdvance.mg.value = scoreMG(PASSED_PAWN_ADVANCE_DEFENDED);
  weights->passedPawnAdvance.eg.value = scoreEG(PASSED_PAWN_ADVANCE_DEFENDED);
}

double Sigmoid(double s) { return 1.0 / (1.0 + exp(-K * s / 400.0)); }

void PrintWeights(Weights* weights) {
  printf("\n\nconst Score MATERIAL_VALUES[7] = {");
  PrintWeightArray(weights->material, 5, 0);
  printf(" S(   0,   0), S(   0,   0) };\n");

  printf("\nconst Score BISHOP_PAIR = ");
  PrintWeight(&weights->bishopPair);

  printf("\nconst Score PAWN_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[PAWN_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score KNIGHT_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[KNIGHT_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score BISHOP_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[BISHOP_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score ROOK_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[ROOK_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score QUEEN_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[QUEEN_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score KING_PSQT[32] = {\n");
  PrintWeightArray(weights->psqt[KING_TYPE], 32, 4);
  printf("};\n");

  printf("\nconst Score KNIGHT_POST_PSQT[32] = {\n");
  PrintWeightArray(weights->knightPostPsqt, 32, 4);
  printf("};\n");

  printf("\nconst Score BISHOP_POST_PSQT[32] = {\n");
  PrintWeightArray(weights->bishopPostPsqt, 32, 4);
  printf("};\n");

  printf("\nconst Score KNIGHT_MOBILITIES[9] = {\n");
  PrintWeightArray(weights->knightMobilities, 9, 4);
  printf("};\n");

  printf("\nconst Score BISHOP_MOBILITIES[14] = {\n");
  PrintWeightArray(weights->bishopMobilities, 14, 4);
  printf("};\n");

  printf("\nconst Score ROOK_MOBILITIES[15] = {\n");
  PrintWeightArray(weights->rookMobilities, 15, 4);
  printf("};\n");

  printf("\nconst Score QUEEN_MOBILITIES[28] = {\n");
  PrintWeightArray(weights->queenMobilities, 28, 4);
  printf("};\n");

  printf("\nconst Score KNIGHT_OUTPOST_REACHABLE = ");
  PrintWeight(&weights->knightPostReachable);

  printf("\nconst Score BISHOP_OUTPOST_REACHABLE = ");
  PrintWeight(&weights->bishopPostReachable);

  printf("\nconst Score BISHOP_TRAPPED = ");
  PrintWeight(&weights->bishopTrapped);

  printf("\nconst Score ROOK_TRAPPED = ");
  PrintWeight(&weights->rookTrapped);

  printf("\nconst Score ROOK_OPEN_FILE = ");
  PrintWeight(&weights->rookOpenFile);

  printf("\nconst Score ROOK_SEMI_OPEN = ");
  PrintWeight(&weights->rookSemiOpen);

  printf("\nconst Score ROOK_OPPOSITE_KING = ");
  PrintWeight(&weights->rookOppositeKing);

  printf("\nconst Score ROOK_ADJACENT_KING = ");
  PrintWeight(&weights->rookAdjacentKing);

  printf("\nconst Score DOUBLED_PAWN = ");
  PrintWeight(&weights->doubledPawns);

  printf("\nconst Score OPPOSED_ISOLATED_PAWN = ");
  PrintWeight(&weights->opposedIsolatedPawns);

  printf("\nconst Score OPEN_ISOLATED_PAWN = ");
  PrintWeight(&weights->openIsolatedPawns);

  printf("\nconst Score BACKWARDS_PAWN = ");
  PrintWeight(&weights->backwardsPawns);

  printf("\nconst Score CONNECTED_PAWN[8] = {\n");
  PrintWeightArray(weights->connectedPawn, 8, 4);
  printf("};\n");

  printf("\nconst Score CANDIDATE_PASSER[8] = {\n");
  PrintWeightArray(weights->candidatePasser, 8, 4);
  printf("};\n");

  printf("\nconst Score PASSED_PAWN[8] = {\n");
  PrintWeightArray(weights->passedPawn, 8, 4);
  printf("};\n");

  printf("\nconst Score PASSED_PAWN_EDGE_DISTANCE = ");
  PrintWeight(&weights->passedPawnEdgeDistance);

  printf("\nconst Score PASSED_PAWN_KING_PROXIMITY = ");
  PrintWeight(&weights->passedPawnKingProximity);

  printf("\nconst Score PASSED_PAWN_ADVANCE_DEFENDED = ");
  PrintWeight(&weights->passedPawnAdvance);

  printf("\nconst Score KNIGHT_THREATS[6] = {");
  PrintWeightArray(weights->knightThreats, 6, 0);
  printf("};\n");

  printf("\nconst Score BISHOP_THREATS[6] = {");
  PrintWeightArray(weights->bishopThreats, 6, 0);
  printf("};\n");

  printf("\nconst Score ROOK_THREATS[6] = {");
  PrintWeightArray(weights->rookThreats, 6, 0);
  printf("};\n");

  printf("\nconst Score KING_THREATS[6] = {");
  PrintWeightArray(weights->kingThreats, 6, 0);
  printf("};\n");

  printf("\nconst Score HANGING_THREAT = ");
  PrintWeight(&weights->hangingThreat);

  printf("\n");
}

void PrintWeightArray(Weight* weights, int n, int wrap) {
  for (int i = 0; i < n; i++) {
    if (wrap)
      printf(" S(%4d,%4d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));
    else
      printf(" S(%d, %d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));

    if (wrap && i % wrap == wrap - 1)
      printf("\n");
  }
}

void PrintWeight(Weight* weight) { printf("S(%d, %d);\n", (int)round(weight->mg.value), (int)round(weight->eg.value)); }

#endif