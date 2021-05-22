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

double K = 3.575325000;

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
  InitPawnShelterWeights(&weights);

  int n = 0;
  Position* positions = LoadPositions(&n);
  ALPHA *= sqrt(n);

  ValidateEval(n, positions, &weights);
  // DetermineK(n, positions);

  for (int epoch = 1; epoch < 10000; epoch++) {
    double error = UpdateAndTrain(epoch, n, positions, &weights);

    PrintWeights(&weights, epoch, error);
  }

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

  for (int sq = 0; sq < 12; sq++) {
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

  UpdateParam(&weights->pawnThreat.mg);
  UpdateParam(&weights->pawnThreat.eg);

  UpdateParam(&weights->pawnPushThreat.mg);
  UpdateParam(&weights->pawnPushThreat.eg);

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

  UpdateParam(&weights->badBishopPawns.mg);
  UpdateParam(&weights->badBishopPawns.eg);

  UpdateParam(&weights->dragonBishop.mg);
  UpdateParam(&weights->dragonBishop.eg);

  UpdateParam(&weights->rookOpenFile.mg);
  UpdateParam(&weights->rookOpenFile.eg);

  UpdateParam(&weights->rookSemiOpen.mg);
  UpdateParam(&weights->rookSemiOpen.eg);

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

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      UpdateParam(&weights->ksPawnShelter[f][r].mg);
      UpdateParam(&weights->ksPawnShelter[f][r].eg);

      UpdateParam(&weights->ksPawnStorm[f][r].mg);
      UpdateParam(&weights->ksPawnStorm[f][r].eg);
    }
  }

  for (int r = 0; r < 8; r++) {
    UpdateParam(&weights->ksBlocked[r].mg);
    UpdateParam(&weights->ksBlocked[r].eg);
  }

  for (int f = 0; f < 4; f++) {
    UpdateParam(&weights->ksFile[f].mg);
    UpdateParam(&weights->ksFile[f].eg);
  }
}

double UpdateAndTrain(int epoch, int n, Position* positions, Weights* weights) {
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

    for (int sq = 0; sq < 12; sq++) {
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

    weights->pawnThreat.mg.g += w->pawnThreat.mg.g;
    weights->pawnThreat.eg.g += w->pawnThreat.eg.g;

    weights->pawnPushThreat.mg.g += w->pawnPushThreat.mg.g;
    weights->pawnPushThreat.eg.g += w->pawnPushThreat.eg.g;

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

    weights->badBishopPawns.mg.g += w->badBishopPawns.mg.g;
    weights->badBishopPawns.eg.g += w->badBishopPawns.eg.g;

    weights->dragonBishop.mg.g += w->dragonBishop.mg.g;
    weights->dragonBishop.eg.g += w->dragonBishop.eg.g;

    weights->rookOpenFile.mg.g += w->rookOpenFile.mg.g;
    weights->rookOpenFile.eg.g += w->rookOpenFile.eg.g;

    weights->rookSemiOpen.mg.g += w->rookSemiOpen.mg.g;
    weights->rookSemiOpen.eg.g += w->rookSemiOpen.eg.g;

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

    for (int f = 0; f < 4; f++) {
      for (int r = 0; r < 8; r++) {
        weights->ksPawnShelter[f][r].mg.g += w->ksPawnShelter[f][r].mg.g;
        weights->ksPawnShelter[f][r].eg.g += w->ksPawnShelter[f][r].eg.g;

        weights->ksPawnStorm[f][r].mg.g += w->ksPawnStorm[f][r].mg.g;
        weights->ksPawnStorm[f][r].eg.g += w->ksPawnStorm[f][r].eg.g;
      }
    }

    for (int r = 0; r < 8; r++) {
      weights->ksBlocked[r].mg.g += w->ksBlocked[r].mg.g;
      weights->ksBlocked[r].eg.g += w->ksBlocked[r].eg.g;
    }

    for (int r = 0; r < 4; r++) {
      weights->ksFile[r].mg.g += w->ksFile[r].mg.g;
      weights->ksFile[r].eg.g += w->ksFile[r].eg.g;
    }
  }

  printf("Epoch: %5d, Error: %9.8f\n", epoch, error / n);

  UpdateWeights(weights);

  return error;
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
    UpdatePawnShelterGradients(&positions[i], loss, weights);

    job->error += pow(positions[i].result - sigmoid, 2);
  }

  return NULL;
}

void UpdateMaterialGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->material[pc].mg.g += position->coeffs.pieces[pc] * mgBase;
    weights->material[pc].eg.g += position->coeffs.pieces[pc] * egBase;
  }
}

void UpdatePsqtGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      weights->psqt[pc][sq].mg.g += position->coeffs.psqt[pc][sq] * mgBase;
      weights->psqt[pc][sq].eg.g += position->coeffs.psqt[pc][sq] * egBase;
    }
  }
}

void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int sq = 0; sq < 12; sq++) {
    weights->knightPostPsqt[sq].mg.g += position->coeffs.knightPostPsqt[sq] * mgBase;
    weights->knightPostPsqt[sq].eg.g += position->coeffs.knightPostPsqt[sq] * egBase;

    weights->bishopPostPsqt[sq].mg.g += position->coeffs.bishopPostPsqt[sq] * mgBase;
    weights->bishopPostPsqt[sq].eg.g += position->coeffs.bishopPostPsqt[sq] * egBase;
  }
}

void UpdateMobilityGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.g += position->coeffs.knightMobilities[c] * mgBase;
    weights->knightMobilities[c].eg.g += position->coeffs.knightMobilities[c] * egBase;
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.g += position->coeffs.bishopMobilities[c] * mgBase;
    weights->bishopMobilities[c].eg.g += position->coeffs.bishopMobilities[c] * egBase;
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.g += position->coeffs.rookMobilities[c] * mgBase;
    weights->rookMobilities[c].eg.g += position->coeffs.rookMobilities[c] * egBase;
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.g += position->coeffs.queenMobilities[c] * mgBase;
    weights->queenMobilities[c].eg.g += position->coeffs.queenMobilities[c] * egBase;
  }
}

void UpdateThreatGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.g += position->coeffs.knightThreats[pc] * mgBase;
    weights->knightThreats[pc].eg.g += position->coeffs.knightThreats[pc] * egBase;

    weights->bishopThreats[pc].mg.g += position->coeffs.bishopThreats[pc] * mgBase;
    weights->bishopThreats[pc].eg.g += position->coeffs.bishopThreats[pc] * egBase;

    weights->rookThreats[pc].mg.g += position->coeffs.rookThreats[pc] * mgBase;
    weights->rookThreats[pc].eg.g += position->coeffs.rookThreats[pc] * egBase;

    weights->kingThreats[pc].mg.g += position->coeffs.kingThreats[pc] * mgBase;
    weights->kingThreats[pc].eg.g += position->coeffs.kingThreats[pc] * egBase;
  }

  weights->pawnThreat.mg.g += position->coeffs.pawnThreat * mgBase;
  weights->pawnThreat.eg.g += position->coeffs.pawnThreat * egBase;

  weights->pawnPushThreat.mg.g += position->coeffs.pawnPushThreat * mgBase;
  weights->pawnPushThreat.eg.g += position->coeffs.pawnPushThreat * egBase;

  weights->hangingThreat.mg.g += position->coeffs.hangingThreat * mgBase;
  weights->hangingThreat.eg.g += position->coeffs.hangingThreat * egBase;
}

void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  weights->bishopPair.mg.g += position->coeffs.bishopPair * mgBase;
  weights->bishopPair.eg.g += position->coeffs.bishopPair * egBase;

  weights->knightPostReachable.mg.g += position->coeffs.knightPostReachable * mgBase;
  weights->knightPostReachable.eg.g += position->coeffs.knightPostReachable * egBase;

  weights->bishopPostReachable.mg.g += position->coeffs.bishopPostReachable * mgBase;
  weights->bishopPostReachable.eg.g += position->coeffs.bishopPostReachable * egBase;

  weights->bishopTrapped.mg.g += position->coeffs.bishopTrapped * mgBase;
  weights->bishopTrapped.eg.g += position->coeffs.bishopTrapped * egBase;

  weights->rookTrapped.mg.g += position->coeffs.rookTrapped * mgBase;
  weights->rookTrapped.eg.g += position->coeffs.rookTrapped * egBase;

  weights->badBishopPawns.mg.g += position->coeffs.badBishopPawns * mgBase;
  weights->badBishopPawns.eg.g += position->coeffs.badBishopPawns * egBase;

  weights->dragonBishop.mg.g += position->coeffs.dragonBishop * mgBase;
  weights->dragonBishop.eg.g += position->coeffs.dragonBishop * egBase;

  weights->rookOpenFile.mg.g += position->coeffs.rookOpenFile * mgBase;
  weights->rookOpenFile.eg.g += position->coeffs.rookOpenFile * egBase;

  weights->rookSemiOpen.mg.g += position->coeffs.rookSemiOpen * mgBase;
  weights->rookSemiOpen.eg.g += position->coeffs.rookSemiOpen * egBase;
}

void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  weights->doubledPawns.mg.g += position->coeffs.doubledPawns * mgBase;
  weights->doubledPawns.eg.g += position->coeffs.doubledPawns * egBase;

  weights->opposedIsolatedPawns.mg.g += position->coeffs.opposedIsolatedPawns * mgBase;
  weights->opposedIsolatedPawns.eg.g += position->coeffs.opposedIsolatedPawns * egBase;

  weights->openIsolatedPawns.mg.g += position->coeffs.openIsolatedPawns * mgBase;
  weights->openIsolatedPawns.eg.g += position->coeffs.openIsolatedPawns * egBase;

  weights->backwardsPawns.mg.g += position->coeffs.backwardsPawns * mgBase;
  weights->backwardsPawns.eg.g += position->coeffs.backwardsPawns * egBase;

  for (int r = 0; r < 8; r++) {
    weights->connectedPawn[r].mg.g += position->coeffs.connectedPawn[r] * mgBase;
    weights->connectedPawn[r].eg.g += position->coeffs.connectedPawn[r] * egBase;

    weights->candidatePasser[r].mg.g += position->coeffs.candidatePasser[r] * mgBase;
    weights->candidatePasser[r].eg.g += position->coeffs.candidatePasser[r] * egBase;
  }
}

void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.g += position->coeffs.passedPawn[r] * mgBase;
    weights->passedPawn[r].eg.g += position->coeffs.passedPawn[r] * egBase;
  }

  weights->passedPawnEdgeDistance.mg.g += position->coeffs.passedPawnEdgeDistance * mgBase;
  weights->passedPawnEdgeDistance.eg.g += position->coeffs.passedPawnEdgeDistance * egBase;

  weights->passedPawnKingProximity.mg.g += position->coeffs.passedPawnKingProximity * mgBase;
  weights->passedPawnKingProximity.eg.g += position->coeffs.passedPawnKingProximity * egBase;

  weights->passedPawnAdvance.mg.g += position->coeffs.passedPawnAdvance * mgBase;
  weights->passedPawnAdvance.eg.g += position->coeffs.passedPawnAdvance * egBase;
}

void UpdatePawnShelterGradients(Position* position, double loss, Weights* weights) {
  double mgBase = position->phaseMg * position->scale * loss;
  double egBase = position->phaseEg * position->scale * loss;

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->ksPawnShelter[f][r].mg.g += position->coeffs.pawnShelter[f][r] * mgBase;
      weights->ksPawnShelter[f][r].eg.g += position->coeffs.pawnShelter[f][r] * egBase;

      weights->ksPawnStorm[f][r].mg.g += position->coeffs.pawnStorm[f][r] * mgBase;
      weights->ksPawnStorm[f][r].eg.g += position->coeffs.pawnStorm[f][r] * egBase;
    }
  }

  for (int f = 0; f < 8; f++) {
    weights->ksBlocked[f].mg.g += position->coeffs.blockedPawnStorm[f] * mgBase;
    weights->ksBlocked[f].eg.g += position->coeffs.blockedPawnStorm[f] * egBase;
  }

  for (int f = 0; f < 4; f++) {
    weights->ksFile[f].mg.g += position->coeffs.kingFile[f] * mgBase;
    weights->ksFile[f].eg.g += position->coeffs.kingFile[f] * egBase;
  }
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
  EvaluatePawnShelterValues(&mg, &eg, position, weights);

  mg += scoreMG(position->coeffs.ks);
  eg += scoreEG(position->coeffs.ks);

  int result = (mg * position->phase + eg * (128 - position->phase)) / 128;
  result = (result * position->scale + MAX_SCALE / 2) / MAX_SCALE;
  return result + (position->stm == WHITE ? TEMPO : -TEMPO);
}

void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    *mg += position->coeffs.pieces[pc] * weights->material[pc].mg.value;
    *eg += position->coeffs.pieces[pc] * weights->material[pc].eg.value;
  }
}

void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      *mg += position->coeffs.psqt[pc][sq] * weights->psqt[pc][sq].mg.value;
      *eg += position->coeffs.psqt[pc][sq] * weights->psqt[pc][sq].eg.value;
    }
  }
}

void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int sq = 0; sq < 12; sq++) {
    *mg += position->coeffs.knightPostPsqt[sq] * weights->knightPostPsqt[sq].mg.value;
    *eg += position->coeffs.knightPostPsqt[sq] * weights->knightPostPsqt[sq].eg.value;

    *mg += position->coeffs.bishopPostPsqt[sq] * weights->bishopPostPsqt[sq].mg.value;
    *eg += position->coeffs.bishopPostPsqt[sq] * weights->bishopPostPsqt[sq].eg.value;
  }
}

void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int c = 0; c < 9; c++) {
    *mg += position->coeffs.knightMobilities[c] * weights->knightMobilities[c].mg.value;
    *eg += position->coeffs.knightMobilities[c] * weights->knightMobilities[c].eg.value;
  }

  for (int c = 0; c < 14; c++) {
    *mg += position->coeffs.bishopMobilities[c] * weights->bishopMobilities[c].mg.value;
    *eg += position->coeffs.bishopMobilities[c] * weights->bishopMobilities[c].eg.value;
  }

  for (int c = 0; c < 15; c++) {
    *mg += position->coeffs.rookMobilities[c] * weights->rookMobilities[c].mg.value;
    *eg += position->coeffs.rookMobilities[c] * weights->rookMobilities[c].eg.value;
  }

  for (int c = 0; c < 28; c++) {
    *mg += position->coeffs.queenMobilities[c] * weights->queenMobilities[c].mg.value;
    *eg += position->coeffs.queenMobilities[c] * weights->queenMobilities[c].eg.value;
  }
}

void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    *mg += position->coeffs.knightThreats[pc] * weights->knightThreats[pc].mg.value;
    *eg += position->coeffs.knightThreats[pc] * weights->knightThreats[pc].eg.value;

    *mg += position->coeffs.bishopThreats[pc] * weights->bishopThreats[pc].mg.value;
    *eg += position->coeffs.bishopThreats[pc] * weights->bishopThreats[pc].eg.value;

    *mg += position->coeffs.rookThreats[pc] * weights->rookThreats[pc].mg.value;
    *eg += position->coeffs.rookThreats[pc] * weights->rookThreats[pc].eg.value;

    *mg += position->coeffs.kingThreats[pc] * weights->kingThreats[pc].mg.value;
    *eg += position->coeffs.kingThreats[pc] * weights->kingThreats[pc].eg.value;
  }

  *mg += position->coeffs.pawnThreat * weights->pawnThreat.mg.value;
  *eg += position->coeffs.pawnThreat * weights->pawnThreat.eg.value;

  *mg += position->coeffs.pawnPushThreat * weights->pawnPushThreat.mg.value;
  *eg += position->coeffs.pawnPushThreat * weights->pawnPushThreat.eg.value;

  *mg += position->coeffs.hangingThreat * weights->hangingThreat.mg.value;
  *eg += position->coeffs.hangingThreat * weights->hangingThreat.eg.value;
}

void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  *mg += position->coeffs.bishopPair * weights->bishopPair.mg.value;
  *eg += position->coeffs.bishopPair * weights->bishopPair.eg.value;

  *mg += position->coeffs.knightPostReachable * weights->knightPostReachable.mg.value;
  *eg += position->coeffs.knightPostReachable * weights->knightPostReachable.eg.value;

  *mg += position->coeffs.bishopPostReachable * weights->bishopPostReachable.mg.value;
  *eg += position->coeffs.bishopPostReachable * weights->bishopPostReachable.eg.value;

  *mg += position->coeffs.bishopTrapped * weights->bishopTrapped.mg.value;
  *eg += position->coeffs.bishopTrapped * weights->bishopTrapped.eg.value;

  *mg += position->coeffs.rookTrapped * weights->rookTrapped.mg.value;
  *eg += position->coeffs.rookTrapped * weights->rookTrapped.eg.value;

  *mg += position->coeffs.badBishopPawns * weights->badBishopPawns.mg.value;
  *eg += position->coeffs.badBishopPawns * weights->badBishopPawns.eg.value;

  *mg += position->coeffs.dragonBishop * weights->dragonBishop.mg.value;
  *eg += position->coeffs.dragonBishop * weights->dragonBishop.eg.value;

  *mg += position->coeffs.rookOpenFile * weights->rookOpenFile.mg.value;
  *eg += position->coeffs.rookOpenFile * weights->rookOpenFile.eg.value;

  *mg += position->coeffs.rookSemiOpen * weights->rookSemiOpen.mg.value;
  *eg += position->coeffs.rookSemiOpen * weights->rookSemiOpen.eg.value;
}

void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  *mg += position->coeffs.doubledPawns * weights->doubledPawns.mg.value;
  *eg += position->coeffs.doubledPawns * weights->doubledPawns.eg.value;

  *mg += position->coeffs.opposedIsolatedPawns * weights->opposedIsolatedPawns.mg.value;
  *eg += position->coeffs.opposedIsolatedPawns * weights->opposedIsolatedPawns.eg.value;

  *mg += position->coeffs.openIsolatedPawns * weights->openIsolatedPawns.mg.value;
  *eg += position->coeffs.openIsolatedPawns * weights->openIsolatedPawns.eg.value;

  *mg += position->coeffs.backwardsPawns * weights->backwardsPawns.mg.value;
  *eg += position->coeffs.backwardsPawns * weights->backwardsPawns.eg.value;

  for (int r = 0; r < 8; r++) {
    *mg += position->coeffs.connectedPawn[r] * weights->connectedPawn[r].mg.value;
    *eg += position->coeffs.connectedPawn[r] * weights->connectedPawn[r].eg.value;

    *mg += position->coeffs.candidatePasser[r] * weights->candidatePasser[r].mg.value;
    *eg += position->coeffs.candidatePasser[r] * weights->candidatePasser[r].eg.value;
  }
}

void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int r = 0; r < 8; r++) {
    *mg += position->coeffs.passedPawn[r] * weights->passedPawn[r].mg.value;
    *eg += position->coeffs.passedPawn[r] * weights->passedPawn[r].eg.value;
  }

  *mg += position->coeffs.passedPawnEdgeDistance * weights->passedPawnEdgeDistance.mg.value;
  *eg += position->coeffs.passedPawnEdgeDistance * weights->passedPawnEdgeDistance.eg.value;

  *mg += position->coeffs.passedPawnKingProximity * weights->passedPawnKingProximity.mg.value;
  *eg += position->coeffs.passedPawnKingProximity * weights->passedPawnKingProximity.eg.value;

  *mg += position->coeffs.passedPawnAdvance * weights->passedPawnAdvance.mg.value;
  *eg += position->coeffs.passedPawnAdvance * weights->passedPawnAdvance.eg.value;
}

void EvaluatePawnShelterValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      *mg += position->coeffs.pawnShelter[f][r] * weights->ksPawnShelter[f][r].mg.value;
      *eg += position->coeffs.pawnShelter[f][r] * weights->ksPawnShelter[f][r].eg.value;

      *mg += position->coeffs.pawnStorm[f][r] * weights->ksPawnStorm[f][r].mg.value;
      *eg += position->coeffs.pawnStorm[f][r] * weights->ksPawnStorm[f][r].eg.value;
    }
  }

  for (int r = 0; r < 8; r++) {
    *mg += position->coeffs.blockedPawnStorm[r] * weights->ksBlocked[r].mg.value;
    *eg += position->coeffs.blockedPawnStorm[r] * weights->ksBlocked[r].eg.value;
  }

  for (int r = 0; r < 4; r++) {
    *mg += position->coeffs.kingFile[r] * weights->ksFile[r].mg.value;
    *eg += position->coeffs.kingFile[r] * weights->ksFile[r].eg.value;
  }
}

void LoadPosition(Board* board, Position* position, ThreadData* thread) {
  memset(&C, 0, sizeof(EvalCoeffs));

  int phase = GetPhase(board);

  position->phaseMg = (double)phase / 128;
  position->phaseEg = 1 - (double)phase / 128;
  position->phase = phase;

  position->stm = board->side;

  Score eval = Evaluate(board, thread);
  position->staticEval = sideScalar[board->side] * eval;

  position->scale = Scale(board, C.ss);

  memcpy(&position->coeffs, &C, sizeof(EvalCoeffs));
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

    LoadPosition(&board, &positions[p], threads);
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
  for (int sq = 0; sq < 12; sq++) {
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

  weights->pawnThreat.mg.value = scoreMG(PAWN_THREAT);
  weights->pawnThreat.eg.value = scoreEG(PAWN_THREAT);

  weights->pawnPushThreat.mg.value = scoreMG(PAWN_PUSH_THREAT);
  weights->pawnPushThreat.eg.value = scoreEG(PAWN_PUSH_THREAT);

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

  weights->badBishopPawns.mg.value = scoreMG(BAD_BISHOP_PAWNS);
  weights->badBishopPawns.eg.value = scoreEG(BAD_BISHOP_PAWNS);

  weights->dragonBishop.mg.value = scoreMG(DRAGON_BISHOP);
  weights->dragonBishop.eg.value = scoreEG(DRAGON_BISHOP);

  weights->rookOpenFile.mg.value = scoreMG(ROOK_OPEN_FILE);
  weights->rookOpenFile.eg.value = scoreEG(ROOK_OPEN_FILE);

  weights->rookSemiOpen.mg.value = scoreMG(ROOK_SEMI_OPEN);
  weights->rookSemiOpen.eg.value = scoreEG(ROOK_SEMI_OPEN);
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

void InitPawnShelterWeights(Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->ksPawnShelter[f][r].mg.value = scoreMG(PAWN_SHELTER[f][r]);
      weights->ksPawnShelter[f][r].eg.value = scoreEG(PAWN_SHELTER[f][r]);

      weights->ksPawnStorm[f][r].mg.value = scoreMG(PAWN_STORM[f][r]);
      weights->ksPawnStorm[f][r].eg.value = scoreEG(PAWN_STORM[f][r]);
    }
  }

  for (int r = 0; r < 8; r++) {
    weights->ksBlocked[r].mg.value = scoreMG(BLOCKED_PAWN_STORM[r]);
    weights->ksBlocked[r].eg.value = scoreEG(BLOCKED_PAWN_STORM[r]);
  }

  for (int f = 0; f < 4; f++) {
    weights->ksFile[f].mg.value = scoreMG(KS_KING_FILE[f]);
    weights->ksFile[f].eg.value = scoreEG(KS_KING_FILE[f]);
  }
}

double Sigmoid(double s) { return 1.0 / (1.0 + exp(-K * s / 400.0)); }

void PrintWeights(Weights* weights, int epoch, double error) {
  FILE* fp;
  fp = fopen("weights.out", "a");

  if (fp == NULL)
    return;

  fprintf(fp, "Epoch: %d, Error: %f\n", epoch, error);

  fprintf(fp, "\nconst Score MATERIAL_VALUES[7] = {");
  PrintWeightArray(fp, weights->material, 5, 0);
  fprintf(fp, " S(   0,   0), S(   0,   0) };\n");

  fprintf(fp, "\nconst Score BISHOP_PAIR = ");
  PrintWeight(fp, &weights->bishopPair);

  fprintf(fp, "\nconst Score PAWN_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[PAWN_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KNIGHT_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[KNIGHT_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[BISHOP_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[ROOK_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score QUEEN_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[QUEEN_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KING_PSQT[32] = {\n");
  PrintWeightArray(fp, weights->psqt[KING_TYPE], 32, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KNIGHT_POST_PSQT[12] = {\n");
  PrintWeightArray(fp, weights->knightPostPsqt, 12, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_POST_PSQT[12] = {\n");
  PrintWeightArray(fp, weights->bishopPostPsqt, 12, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KNIGHT_MOBILITIES[9] = {\n");
  PrintWeightArray(fp, weights->knightMobilities, 9, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_MOBILITIES[14] = {\n");
  PrintWeightArray(fp, weights->bishopMobilities, 14, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_MOBILITIES[15] = {\n");
  PrintWeightArray(fp, weights->rookMobilities, 15, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score QUEEN_MOBILITIES[28] = {\n");
  PrintWeightArray(fp, weights->queenMobilities, 28, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KNIGHT_OUTPOST_REACHABLE = ");
  PrintWeight(fp, &weights->knightPostReachable);

  fprintf(fp, "\nconst Score BISHOP_OUTPOST_REACHABLE = ");
  PrintWeight(fp, &weights->bishopPostReachable);

  fprintf(fp, "\nconst Score BISHOP_TRAPPED = ");
  PrintWeight(fp, &weights->bishopTrapped);

  fprintf(fp, "\nconst Score ROOK_TRAPPED = ");
  PrintWeight(fp, &weights->rookTrapped);

  fprintf(fp, "\nconst Score BAD_BISHOP_PAWNS = ");
  PrintWeight(fp, &weights->badBishopPawns);

  fprintf(fp, "\nconst Score DRAGON_BISHOP = ");
  PrintWeight(fp, &weights->dragonBishop);

  fprintf(fp, "\nconst Score ROOK_OPEN_FILE = ");
  PrintWeight(fp, &weights->rookOpenFile);

  fprintf(fp, "\nconst Score ROOK_SEMI_OPEN = ");
  PrintWeight(fp, &weights->rookSemiOpen);

  fprintf(fp, "\nconst Score DOUBLED_PAWN = ");
  PrintWeight(fp, &weights->doubledPawns);

  fprintf(fp, "\nconst Score OPPOSED_ISOLATED_PAWN = ");
  PrintWeight(fp, &weights->opposedIsolatedPawns);

  fprintf(fp, "\nconst Score OPEN_ISOLATED_PAWN = ");
  PrintWeight(fp, &weights->openIsolatedPawns);

  fprintf(fp, "\nconst Score BACKWARDS_PAWN = ");
  PrintWeight(fp, &weights->backwardsPawns);

  fprintf(fp, "\nconst Score CONNECTED_PAWN[8] = {\n");
  PrintWeightArray(fp, weights->connectedPawn, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score CANDIDATE_PASSER[8] = {\n");
  PrintWeightArray(fp, weights->candidatePasser, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN[8] = {\n");
  PrintWeightArray(fp, weights->passedPawn, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN_EDGE_DISTANCE = ");
  PrintWeight(fp, &weights->passedPawnEdgeDistance);

  fprintf(fp, "\nconst Score PASSED_PAWN_KING_PROXIMITY = ");
  PrintWeight(fp, &weights->passedPawnKingProximity);

  fprintf(fp, "\nconst Score PASSED_PAWN_ADVANCE_DEFENDED = ");
  PrintWeight(fp, &weights->passedPawnAdvance);

  fprintf(fp, "\nconst Score KNIGHT_THREATS[6] = {");
  PrintWeightArray(fp, weights->knightThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_THREATS[6] = {");
  PrintWeightArray(fp, weights->bishopThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_THREATS[6] = {");
  PrintWeightArray(fp, weights->rookThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KING_THREATS[6] = {");
  PrintWeightArray(fp, weights->kingThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_THREAT = ");
  PrintWeight(fp, &weights->pawnThreat);

  fprintf(fp, "\nconst Score PAWN_PUSH_THREAT = ");
  PrintWeight(fp, &weights->pawnPushThreat);

  fprintf(fp, "\nconst Score HANGING_THREAT = ");
  PrintWeight(fp, &weights->hangingThreat);

  fprintf(fp, "\nconst Score PAWN_SHELTER[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->ksPawnShelter[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnShelter[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnShelter[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnShelter[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_STORM[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->ksPawnStorm[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnStorm[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnStorm[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->ksPawnStorm[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BLOCKED_PAWN_STORM[8] = {\n");
  PrintWeightArray(fp, weights->ksBlocked, 8, 0);
  fprintf(fp, "\n};\n");

  fprintf(fp, "\nconst Score KS_KING_FILE[4] = {");
  PrintWeightArray(fp, weights->ksFile, 4, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\n");

  fclose(fp);
}

void PrintWeightArray(FILE* fp, Weight* weights, int n, int wrap) {
  for (int i = 0; i < n; i++) {
    if (wrap)
      fprintf(fp, " S(%4d,%4d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));
    else
      fprintf(fp, " S(%d, %d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));

    if (wrap && i % wrap == wrap - 1)
      fprintf(fp, "\n");
  }
}

void PrintWeight(FILE* fp, Weight* weight) {
  fprintf(fp, "S(%d, %d);\n", (int)round(weight->mg.value), (int)round(weight->eg.value));
}

#endif