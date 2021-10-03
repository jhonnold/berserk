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
#include <time.h>
#include <unistd.h>

#include "../bits.h"
#include "../board.h"
#include "../eval.h"
#include "../pawns.h"
#include "../search.h"
#include "../thread.h"
#include "../util.h"
#include "../weights.h"
#include "tune.h"
#include "util.h"

void Tune() {
  Weights weights = {0};

  KPNetwork* network = KP_NET; // already initialized
  ResetKPNetworkGradients(network);

  InitMaterialWeights(&weights);
  InitPsqtWeights(&weights);
  InitPostPsqtWeights(&weights);
  InitMobilityWeights(&weights);
  InitThreatWeights(&weights);
  InitPieceBonusWeights(&weights);
  InitPawnBonusWeights(&weights);
  InitPasserBonusWeights(&weights);
  InitPawnShelterWeights(&weights);
  InitSpaceWeights(&weights);
  InitTempoWeight(&weights);
  InitComplexityWeights(&weights);
  InitKingSafetyWeights(&weights);

  PrintWeights(&weights, 0, 0);

  int n = 0;
  Position* positions = LoadPositions(&n, &weights, network);

  float error = TotalStaticError(n, positions);
  printf("Starting Error: %1.8f\n", error);

  long start = GetTimeMS();

  for (int epoch = 1; epoch <= 100000; epoch++) {
    for (int i = 0; i < n / BATCH_SIZE; i++)
      UpdateAndTrain(i, positions, &weights, network);

    long now = GetTimeMS();

    float newError = TotalTunedError(n, positions, &weights, network) / n;
    printf("\rEpoch: [#%4d], Error: [%1.8f], Delta: [%+1.8f], Speed: [%9.0f pos/s]", epoch, newError, error - newError,
           1000.0 * n * epoch / (now - start));
    error = newError;

    PrintWeights(&weights, epoch, error);
    SaveKPNetwork("kpnet.dat", network);

    ShufflePositions(n, positions);
  }

  free(positions);
}

float TotalStaticError(int n, Position* positions) {
  float e = 0;

#pragma omp parallel for schedule(static) num_threads(THREADS) reduction(+ : e)
  for (int i = 0; i < n; i++) {

    float sigmoid = Sigmoid(positions[i].staticEval, K);
    float difference = sigmoid - positions[i].result;
    e += powf(difference, 2);
  }

  return e / n;
}

void ResetKPNetworkGradients(KPNetwork* network) {
  for (int i = 0; i < N_KP_FEATURES * N_KP_HIDDEN; i++) {
    network->gWeights0[i].g = 0;
    network->gWeights0[i].V = 0;
    network->gWeights0[i].M = 0;
  }

  for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++) {
    network->gWeights1[i].g = 0;
    network->gWeights1[i].V = 0;
    network->gWeights1[i].M = 0;
  }

  for (int i = 0; i < N_KP_HIDDEN; i++) {
    network->gBiases0[i].g = 0;
    network->gBiases0[i].V = 0;
    network->gBiases0[i].M = 0;
  }

  for (int i = 0; i < N_KP_OUTPUT; i++) {
    network->gBiases1[i].g = 0;
    network->gBiases1[i].V = 0;
    network->gBiases1[i].M = 0;
  }
}

void UpdateAndApplyGradient(float* v, Gradient* grad) {
  if (!grad->g)
    return;

  grad->M = BETA1 * grad->M + (1.0 - BETA1) * grad->g;
  grad->V = BETA2 * grad->V + (1.0 - BETA2) * grad->g * grad->g;

  float delta = ALPHA * grad->M / (sqrtf(grad->V) + EPSILON);

  *v -= delta;

  grad->g = 0;
}

void UpdateParam(Param* p) { UpdateAndApplyGradient(&p->value, &p->grad); }

void UpdateWeight(Weight* w) {
  UpdateParam(&w->mg);
  UpdateParam(&w->eg);
}

void UpdateNetwork(KPNetwork* network) {
  for (int i = 0; i < N_KP_FEATURES * N_KP_HIDDEN; i++)
    UpdateAndApplyGradient(&network->weights0[i], &network->gWeights0[i]);

  for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++)
    UpdateAndApplyGradient(&network->weights1[i], &network->gWeights1[i]);

  for (int i = 0; i < N_KP_HIDDEN; i++)
    UpdateAndApplyGradient(&network->biases0[i], &network->gBiases0[i]);

  for (int i = 0; i < N_KP_OUTPUT; i++)
    UpdateAndApplyGradient(&network->biases1[i], &network->gBiases1[i]);
}

void UpdateWeights(Weights* weights) {
  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++)
    for (int sq = 0; sq < 32; sq++)
      for (int i = 0; i < 2; i++)
        UpdateWeight(&weights->psqt[pc][i][sq]);

  for (int sq = 0; sq < 12; sq++) {
    UpdateWeight(&weights->knightPostPsqt[sq]);
    UpdateWeight(&weights->bishopPostPsqt[sq]);
  }

  for (int c = 0; c < 9; c++)
    UpdateWeight(&weights->knightMobilities[c]);
  for (int c = 0; c < 14; c++)
    UpdateWeight(&weights->bishopMobilities[c]);
  for (int c = 0; c < 15; c++)
    UpdateWeight(&weights->rookMobilities[c]);
  for (int c = 0; c < 28; c++)
    UpdateWeight(&weights->queenMobilities[c]);
  for (int c = 0; c < 9; c++)
    UpdateWeight(&weights->kingMobilities[c]);

  for (int pc = 0; pc < 6; pc++) {
    UpdateWeight(&weights->knightThreats[pc]);
    UpdateWeight(&weights->bishopThreats[pc]);
    UpdateWeight(&weights->rookThreats[pc]);
  }
  UpdateWeight(&weights->kingThreat);
  UpdateWeight(&weights->pawnThreat);
  UpdateWeight(&weights->pawnPushThreat);
  UpdateWeight(&weights->pawnPushThreatPinned);
  UpdateWeight(&weights->hangingThreat);
  UpdateWeight(&weights->knightCheckQueen);
  UpdateWeight(&weights->bishopCheckQueen);
  UpdateWeight(&weights->rookCheckQueen);

  UpdateWeight(&weights->bishopPair);
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      UpdateWeight(&weights->imbalance[i][j]);

  UpdateWeight(&weights->minorBehindPawn);
  UpdateWeight(&weights->knightPostReachable);
  UpdateWeight(&weights->bishopPostReachable);
  UpdateWeight(&weights->bishopTrapped);
  UpdateWeight(&weights->rookTrapped);
  UpdateWeight(&weights->badBishopPawns);
  UpdateWeight(&weights->dragonBishop);
  UpdateWeight(&weights->rookOpenFileOffset);
  UpdateWeight(&weights->rookOpenFile);
  UpdateWeight(&weights->rookSemiOpen);
  UpdateWeight(&weights->rookToOpen);
  UpdateWeight(&weights->queenOppositeRook);
  UpdateWeight(&weights->queenRookBattery);

  UpdateWeight(&weights->defendedPawns);
  UpdateWeight(&weights->doubledPawns);
  UpdateWeight(&weights->candidateEdgeDistance);
  for (int f = 0; f < 4; f++)
    UpdateWeight(&weights->isolatedPawns[f]);
  UpdateWeight(&weights->openIsolatedPawns);
  UpdateWeight(&weights->backwardsPawns);
  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++)
      UpdateWeight(&weights->connectedPawn[f][r]);
    UpdateWeight(&weights->candidatePasser[r]);
  }

  for (int r = 0; r < 8; r++)
    UpdateWeight(&weights->passedPawn[r]);
  UpdateWeight(&weights->passedPawnEdgeDistance);
  UpdateWeight(&weights->passedPawnKingProximity);
  for (int r = 0; r < 5; r++)
    UpdateWeight(&weights->passedPawnAdvance[r]);
  UpdateWeight(&weights->passedPawnEnemySliderBehind);
  UpdateWeight(&weights->passedPawnSqRule);
  UpdateWeight(&weights->passedPawnUnsupported);
  UpdateWeight(&weights->passedPawnOutsideVKnight);

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      UpdateWeight(&weights->pawnShelter[f][r]);
      UpdateWeight(&weights->pawnStorm[f][r]);
    }
  }
  for (int r = 0; r < 8; r++)
    UpdateWeight(&weights->blockedPawnStorm[r]);
  UpdateWeight(&weights->castlingRights);

  UpdateParam(&weights->space.mg);

  UpdateParam(&weights->tempo.mg);

  UpdateParam(&weights->complexPawns.mg);
  UpdateParam(&weights->complexOffset.mg);
  UpdateParam(&weights->complexPawnsBothSides.mg);

  if (TUNE_KS) {
    for (int i = 0; i < 5; i++)
      UpdateParam(&weights->ksAttackerWeight[i].mg);
    UpdateParam(&weights->ksWeakSqs.mg);
    UpdateParam(&weights->ksPinned.mg);
    UpdateParam(&weights->ksKnightCheck.mg);
    UpdateParam(&weights->ksBishopCheck.mg);
    UpdateParam(&weights->ksRookCheck.mg);
    UpdateParam(&weights->ksQueenCheck.mg);
    UpdateParam(&weights->ksUnsafeCheck.mg);
    UpdateParam(&weights->ksEnemyQueen.mg);
    UpdateParam(&weights->ksKnightDefense.mg);
  }
}

void MergeGradient(Weight* dest, Weight* src) {
  dest->mg.grad.g += src->mg.grad.g;
  dest->eg.grad.g += src->eg.grad.g;
}

float TotalTunedError(int n, Position* positions, Weights* weights, KPNetwork* network) {
  pthread_t threads[THREADS];
  GradientUpdate jobs[THREADS];
  Weights local[THREADS];
  KPNetwork nets[THREADS];

  int chunk = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    memcpy(&local[t], weights, sizeof(Weights));
    memcpy(&nets[t], network, sizeof(KPNetwork));

    jobs[t].error = 0.0;
    jobs[t].n = t < THREADS - 1 ? chunk : (n - ((THREADS - 1) * chunk));
    jobs[t].positions = &positions[t * chunk];
    jobs[t].weights = &local[t];
    jobs[t].network = &nets[t];

    pthread_create(&threads[t], NULL, &UpdateGradients, &jobs[t]);
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  float error = 0;
  for (int t = 0; t < THREADS; t++)
    error += jobs[t].error;

  return error;
}

float UpdateAndTrain(int n, Position* positions, Weights* weights, KPNetwork* network) {
  pthread_t threads[THREADS];
  GradientUpdate jobs[THREADS];
  Weights local[THREADS];
  KPNetwork nets[THREADS];

  int chunk = (BATCH_SIZE / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    memcpy(&local[t], weights, sizeof(Weights));
    memcpy(&nets[t], network, sizeof(KPNetwork));

    jobs[t].error = 0.0;
    jobs[t].n = t < THREADS - 1 ? chunk : (n - ((THREADS - 1) * chunk));
    jobs[t].positions = &positions[t * chunk + n * BATCH_SIZE];
    jobs[t].weights = &local[t];
    jobs[t].network = &nets[t];

    pthread_create(&threads[t], NULL, &UpdateGradients, &jobs[t]);
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  float error = 0;
  for (int t = 0; t < THREADS; t++) {
    error += jobs[t].error;
    Weights* w = jobs[t].weights;
    KPNetwork* net = jobs[t].network;

    for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++)
      MergeGradient(&weights->pieces[pc], &w->pieces[pc]);
    MergeGradient(&weights->bishopPair, &w->bishopPair);

    for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++)
      for (int sq = 0; sq < 32; sq++)
        for (int i = 0; i < 2; i++)
          MergeGradient(&weights->psqt[pc][i][sq], &w->psqt[pc][i][sq]);

    for (int sq = 0; sq < 12; sq++) {
      MergeGradient(&weights->knightPostPsqt[sq], &w->knightPostPsqt[sq]);
      MergeGradient(&weights->bishopPostPsqt[sq], &w->bishopPostPsqt[sq]);
    }

    for (int c = 0; c < 9; c++)
      MergeGradient(&weights->knightMobilities[c], &w->knightMobilities[c]);
    for (int c = 0; c < 14; c++)
      MergeGradient(&weights->bishopMobilities[c], &w->bishopMobilities[c]);
    for (int c = 0; c < 15; c++)
      MergeGradient(&weights->rookMobilities[c], &w->rookMobilities[c]);
    for (int c = 0; c < 28; c++)
      MergeGradient(&weights->queenMobilities[c], &w->queenMobilities[c]);
    for (int c = 0; c < 9; c++)
      MergeGradient(&weights->kingMobilities[c], &w->kingMobilities[c]);

    for (int pc = 0; pc < 6; pc++) {
      MergeGradient(&weights->knightThreats[pc], &w->knightThreats[pc]);
      MergeGradient(&weights->bishopThreats[pc], &w->bishopThreats[pc]);
      MergeGradient(&weights->rookThreats[pc], &w->rookThreats[pc]);
    }
    MergeGradient(&weights->kingThreat, &w->kingThreat);
    MergeGradient(&weights->pawnThreat, &w->pawnThreat);
    MergeGradient(&weights->pawnPushThreat, &w->pawnPushThreat);
    MergeGradient(&weights->pawnPushThreatPinned, &w->pawnPushThreatPinned);
    MergeGradient(&weights->hangingThreat, &w->hangingThreat);
    MergeGradient(&weights->knightCheckQueen, &w->knightCheckQueen);
    MergeGradient(&weights->bishopCheckQueen, &w->bishopCheckQueen);
    MergeGradient(&weights->rookCheckQueen, &w->rookCheckQueen);

    for (int i = 0; i < 5; i++)
      for (int j = 0; j < 5; j++)
        MergeGradient(&weights->imbalance[i][j], &w->imbalance[i][j]);

    MergeGradient(&weights->minorBehindPawn, &w->minorBehindPawn);
    MergeGradient(&weights->knightPostReachable, &w->knightPostReachable);
    MergeGradient(&weights->bishopPostReachable, &w->bishopPostReachable);
    MergeGradient(&weights->bishopTrapped, &w->bishopTrapped);
    MergeGradient(&weights->rookTrapped, &w->rookTrapped);
    MergeGradient(&weights->badBishopPawns, &w->badBishopPawns);
    MergeGradient(&weights->dragonBishop, &w->dragonBishop);
    MergeGradient(&weights->rookOpenFileOffset, &w->rookOpenFileOffset);
    MergeGradient(&weights->rookOpenFile, &w->rookOpenFile);
    MergeGradient(&weights->rookSemiOpen, &w->rookSemiOpen);
    MergeGradient(&weights->rookToOpen, &w->rookToOpen);
    MergeGradient(&weights->queenOppositeRook, &w->queenOppositeRook);
    MergeGradient(&weights->queenRookBattery, &w->queenRookBattery);

    MergeGradient(&weights->defendedPawns, &w->defendedPawns);
    MergeGradient(&weights->doubledPawns, &w->doubledPawns);
    for (int f = 0; f < 4; f++)
      MergeGradient(&weights->isolatedPawns[f], &w->isolatedPawns[f]);
    MergeGradient(&weights->openIsolatedPawns, &w->openIsolatedPawns);
    MergeGradient(&weights->backwardsPawns, &w->backwardsPawns);
    MergeGradient(&weights->candidateEdgeDistance, &w->candidateEdgeDistance);
    for (int r = 0; r < 8; r++) {
      for (int f = 0; f < 4; f++)
        MergeGradient(&weights->connectedPawn[f][r], &w->connectedPawn[f][r]);
      MergeGradient(&weights->candidatePasser[r], &w->candidatePasser[r]);
    }

    for (int r = 0; r < 8; r++)
      MergeGradient(&weights->passedPawn[r], &w->passedPawn[r]);
    MergeGradient(&weights->passedPawnEdgeDistance, &w->passedPawnEdgeDistance);
    MergeGradient(&weights->passedPawnKingProximity, &w->passedPawnKingProximity);
    for (int r = 0; r < 5; r++)
      MergeGradient(&weights->passedPawnAdvance[r], &w->passedPawnAdvance[r]);
    MergeGradient(&weights->passedPawnEnemySliderBehind, &w->passedPawnEnemySliderBehind);
    MergeGradient(&weights->passedPawnSqRule, &w->passedPawnSqRule);
    MergeGradient(&weights->passedPawnUnsupported, &w->passedPawnUnsupported);
    MergeGradient(&weights->passedPawnOutsideVKnight, &w->passedPawnOutsideVKnight);

    for (int f = 0; f < 4; f++) {
      for (int r = 0; r < 8; r++) {
        MergeGradient(&weights->pawnShelter[f][r], &w->pawnShelter[f][r]);
        MergeGradient(&weights->pawnStorm[f][r], &w->pawnStorm[f][r]);
      }
    }
    for (int r = 0; r < 8; r++)
      MergeGradient(&weights->blockedPawnStorm[r], &w->blockedPawnStorm[r]);
    MergeGradient(&weights->castlingRights, &w->castlingRights);

    MergeGradient(&weights->space, &w->space);
    MergeGradient(&weights->tempo, &w->tempo);

    MergeGradient(&weights->complexPawns, &w->complexPawns);
    MergeGradient(&weights->complexPawnsBothSides, &w->complexPawnsBothSides);
    MergeGradient(&weights->complexOffset, &w->complexOffset);

    if (TUNE_KS) {
      for (int i = 0; i < 5; i++)
        MergeGradient(&weights->ksAttackerWeight[i], &w->ksAttackerWeight[i]);

      MergeGradient(&weights->ksWeakSqs, &w->ksWeakSqs);
      MergeGradient(&weights->ksPinned, &w->ksPinned);
      MergeGradient(&weights->ksKnightCheck, &w->ksKnightCheck);
      MergeGradient(&weights->ksBishopCheck, &w->ksBishopCheck);
      MergeGradient(&weights->ksRookCheck, &w->ksRookCheck);
      MergeGradient(&weights->ksQueenCheck, &w->ksQueenCheck);
      MergeGradient(&weights->ksUnsafeCheck, &w->ksUnsafeCheck);
      MergeGradient(&weights->ksEnemyQueen, &w->ksEnemyQueen);
      MergeGradient(&weights->ksKnightDefense, &w->ksKnightDefense);
    }

    for (int i = 0; i < N_KP_FEATURES * N_KP_HIDDEN; i++)
      network->gWeights0[i].g += net->gWeights0[i].g;
    for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++)
      network->gWeights1[i].g += net->gWeights1[i].g;

    for (int i = 0; i < N_KP_HIDDEN; i++)
      network->gBiases0[i].g += net->gBiases0[i].g;
    for (int i = 0; i < N_KP_OUTPUT; i++)
      network->gBiases1[i].g += net->gBiases1[i].g;
  }

  UpdateWeights(weights);
  UpdateNetwork(network);

  return error;
}

void* CalculateError(void* arg) {
  GradientUpdate* job = (GradientUpdate*)arg;

  int n = job->n;
  Weights* weights = job->weights;
  Position* positions = job->positions;
  KPNetwork* network = job->network;
  job->error = 0.0f;

  for (int i = 0; i < n; i++) {
    EvalGradientData gd[1];
    float eval = EvaluateCoeffs(&positions[i], weights, gd, network);

    job->error += powf(positions[i].result - Sigmoid(eval, K), 2);
  }

  return NULL;
}

void* UpdateGradients(void* arg) {
  GradientUpdate* job = (GradientUpdate*)arg;

  int n = job->n;
  Weights* weights = job->weights;
  Position* positions = job->positions;
  KPNetwork* network = job->network;

  for (int i = 0; i < n; i++) {
    EvalGradientData gd[1];
    float actual = EvaluateCoeffs(&positions[i], weights, gd, network);

    float sigmoid = Sigmoid(actual, K);
    float loss = SigmoidPrime(sigmoid, K) * 2.0 * (sigmoid - positions[i].result);

    UpdateMaterialGradients(&positions[i], loss, weights, gd);
    UpdatePsqtGradients(&positions[i], loss, weights, gd);
    UpdatePostPsqtGradients(&positions[i], loss, weights, gd);
    UpdateMobilityGradients(&positions[i], loss, weights, gd);
    UpdateThreatGradients(&positions[i], loss, weights, gd);
    UpdatePieceBonusGradients(&positions[i], loss, weights, gd);
    UpdatePawnBonusGradients(&positions[i], loss, weights, gd);
    UpdatePasserBonusGradients(&positions[i], loss, weights, gd);
    UpdatePawnShelterGradients(&positions[i], loss, weights, gd);
    UpdateSpaceGradients(&positions[i], loss, weights, gd);
    UpdateTempoGradient(&positions[i], loss, weights);
    UpdateKPNetworkGradients(&positions[i], loss, network, gd);

    if (TUNE_KS)
      UpdateKingSafetyGradients(&positions[i], loss, weights, gd);

    UpdateComplexityGradients(&positions[i], loss, weights, gd);

    job->error += pow(positions[i].result - sigmoid, 2);
  }

  return NULL;
}

void UpdateMaterialGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->pieces[pc].mg.grad.g += position->coeffs.pieces[pc] * mgBase;
    weights->pieces[pc].eg.grad.g += position->coeffs.pieces[pc] * egBase;
  }
}

void UpdatePsqtGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      for (int i = 0; i < 2; i++) {
        weights->psqt[pc][i][sq].mg.grad.g += position->coeffs.psqt[pc][i][sq] * mgBase;
        weights->psqt[pc][i][sq].eg.grad.g += position->coeffs.psqt[pc][i][sq] * egBase;
      }
    }
  }
}

void UpdatePostPsqtGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int sq = 0; sq < 12; sq++) {
    weights->knightPostPsqt[sq].mg.grad.g += position->coeffs.knightPostPsqt[sq] * mgBase;
    weights->knightPostPsqt[sq].eg.grad.g += position->coeffs.knightPostPsqt[sq] * egBase;

    weights->bishopPostPsqt[sq].mg.grad.g += position->coeffs.bishopPostPsqt[sq] * mgBase;
    weights->bishopPostPsqt[sq].eg.grad.g += position->coeffs.bishopPostPsqt[sq] * egBase;
  }
}

void UpdateMobilityGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.grad.g += position->coeffs.knightMobilities[c] * mgBase;
    weights->knightMobilities[c].eg.grad.g += position->coeffs.knightMobilities[c] * egBase;
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.grad.g += position->coeffs.bishopMobilities[c] * mgBase;
    weights->bishopMobilities[c].eg.grad.g += position->coeffs.bishopMobilities[c] * egBase;
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.grad.g += position->coeffs.rookMobilities[c] * mgBase;
    weights->rookMobilities[c].eg.grad.g += position->coeffs.rookMobilities[c] * egBase;
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.grad.g += position->coeffs.queenMobilities[c] * mgBase;
    weights->queenMobilities[c].eg.grad.g += position->coeffs.queenMobilities[c] * egBase;
  }

  for (int c = 0; c < 9; c++) {
    weights->kingMobilities[c].mg.grad.g += position->coeffs.kingMobilities[c] * mgBase;
    weights->kingMobilities[c].eg.grad.g += position->coeffs.kingMobilities[c] * egBase;
  }
}

void UpdateThreatGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.grad.g += position->coeffs.knightThreats[pc] * mgBase;
    weights->knightThreats[pc].eg.grad.g += position->coeffs.knightThreats[pc] * egBase;

    weights->bishopThreats[pc].mg.grad.g += position->coeffs.bishopThreats[pc] * mgBase;
    weights->bishopThreats[pc].eg.grad.g += position->coeffs.bishopThreats[pc] * egBase;

    weights->rookThreats[pc].mg.grad.g += position->coeffs.rookThreats[pc] * mgBase;
    weights->rookThreats[pc].eg.grad.g += position->coeffs.rookThreats[pc] * egBase;
  }

  weights->kingThreat.mg.grad.g += position->coeffs.kingThreat * mgBase;
  weights->kingThreat.eg.grad.g += position->coeffs.kingThreat * egBase;

  weights->pawnThreat.mg.grad.g += position->coeffs.pawnThreat * mgBase;
  weights->pawnThreat.eg.grad.g += position->coeffs.pawnThreat * egBase;

  weights->pawnPushThreat.mg.grad.g += position->coeffs.pawnPushThreat * mgBase;
  weights->pawnPushThreat.eg.grad.g += position->coeffs.pawnPushThreat * egBase;

  weights->pawnPushThreatPinned.mg.grad.g += position->coeffs.pawnPushThreatPinned * mgBase;
  weights->pawnPushThreatPinned.eg.grad.g += position->coeffs.pawnPushThreatPinned * egBase;

  weights->hangingThreat.mg.grad.g += position->coeffs.hangingThreat * mgBase;
  weights->hangingThreat.eg.grad.g += position->coeffs.hangingThreat * egBase;

  weights->knightCheckQueen.mg.grad.g += position->coeffs.knightCheckQueen * mgBase;
  weights->knightCheckQueen.eg.grad.g += position->coeffs.knightCheckQueen * egBase;

  weights->bishopCheckQueen.mg.grad.g += position->coeffs.bishopCheckQueen * mgBase;
  weights->bishopCheckQueen.eg.grad.g += position->coeffs.bishopCheckQueen * egBase;

  weights->rookCheckQueen.mg.grad.g += position->coeffs.rookCheckQueen * mgBase;
  weights->rookCheckQueen.eg.grad.g += position->coeffs.rookCheckQueen * egBase;
}

void UpdatePieceBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  weights->bishopPair.mg.grad.g += position->coeffs.bishopPair * mgBase;
  weights->bishopPair.eg.grad.g += position->coeffs.bishopPair * egBase;

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      weights->imbalance[i][j].mg.grad.g += position->coeffs.imbalance[i][j] * mgBase;
      weights->imbalance[i][j].eg.grad.g += position->coeffs.imbalance[i][j] * egBase;
    }
  }

  weights->minorBehindPawn.mg.grad.g += position->coeffs.minorBehindPawn * mgBase;
  weights->minorBehindPawn.eg.grad.g += position->coeffs.minorBehindPawn * egBase;

  weights->knightPostReachable.mg.grad.g += position->coeffs.knightPostReachable * mgBase;
  weights->knightPostReachable.eg.grad.g += position->coeffs.knightPostReachable * egBase;

  weights->bishopPostReachable.mg.grad.g += position->coeffs.bishopPostReachable * mgBase;
  weights->bishopPostReachable.eg.grad.g += position->coeffs.bishopPostReachable * egBase;

  weights->bishopTrapped.mg.grad.g += position->coeffs.bishopTrapped * mgBase;
  weights->bishopTrapped.eg.grad.g += position->coeffs.bishopTrapped * egBase;

  weights->rookTrapped.mg.grad.g += position->coeffs.rookTrapped * mgBase;
  weights->rookTrapped.eg.grad.g += position->coeffs.rookTrapped * egBase;

  weights->badBishopPawns.mg.grad.g += position->coeffs.badBishopPawns * mgBase;
  weights->badBishopPawns.eg.grad.g += position->coeffs.badBishopPawns * egBase;

  weights->dragonBishop.mg.grad.g += position->coeffs.dragonBishop * mgBase;
  weights->dragonBishop.eg.grad.g += position->coeffs.dragonBishop * egBase;

  weights->rookOpenFileOffset.mg.grad.g += position->coeffs.rookOpenFileOffset * mgBase;
  weights->rookOpenFileOffset.eg.grad.g += position->coeffs.rookOpenFileOffset * egBase;

  weights->rookOpenFile.mg.grad.g += position->coeffs.rookOpenFile * mgBase;
  weights->rookOpenFile.eg.grad.g += position->coeffs.rookOpenFile * egBase;

  weights->rookSemiOpen.mg.grad.g += position->coeffs.rookSemiOpen * mgBase;
  weights->rookSemiOpen.eg.grad.g += position->coeffs.rookSemiOpen * egBase;

  weights->rookToOpen.mg.grad.g += position->coeffs.rookToOpen * mgBase;
  weights->rookToOpen.eg.grad.g += position->coeffs.rookToOpen * egBase;

  weights->queenOppositeRook.mg.grad.g += position->coeffs.queenOppositeRook * mgBase;
  weights->queenOppositeRook.eg.grad.g += position->coeffs.queenOppositeRook * egBase;

  weights->queenRookBattery.mg.grad.g += position->coeffs.queenRookBattery * mgBase;
  weights->queenRookBattery.eg.grad.g += position->coeffs.queenRookBattery * egBase;
}

void UpdatePawnBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  weights->defendedPawns.mg.grad.g += position->coeffs.defendedPawns * mgBase;
  weights->defendedPawns.eg.grad.g += position->coeffs.defendedPawns * egBase;

  weights->doubledPawns.mg.grad.g += position->coeffs.doubledPawns * mgBase;
  weights->doubledPawns.eg.grad.g += position->coeffs.doubledPawns * egBase;

  for (int f = 0; f < 4; f++) {
    weights->isolatedPawns[f].mg.grad.g += position->coeffs.isolatedPawns[f] * mgBase;
    weights->isolatedPawns[f].eg.grad.g += position->coeffs.isolatedPawns[f] * egBase;
  }

  weights->openIsolatedPawns.mg.grad.g += position->coeffs.openIsolatedPawns * mgBase;
  weights->openIsolatedPawns.eg.grad.g += position->coeffs.openIsolatedPawns * egBase;

  weights->backwardsPawns.mg.grad.g += position->coeffs.backwardsPawns * mgBase;
  weights->backwardsPawns.eg.grad.g += position->coeffs.backwardsPawns * egBase;

  weights->candidateEdgeDistance.mg.grad.g += position->coeffs.candidateEdgeDistance * mgBase;
  weights->candidateEdgeDistance.eg.grad.g += position->coeffs.candidateEdgeDistance * egBase;

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++) {
      weights->connectedPawn[f][r].mg.grad.g += position->coeffs.connectedPawn[f][r] * mgBase;
      weights->connectedPawn[f][r].eg.grad.g += position->coeffs.connectedPawn[f][r] * egBase;
    }

    weights->candidatePasser[r].mg.grad.g += position->coeffs.candidatePasser[r] * mgBase;
    weights->candidatePasser[r].eg.grad.g += position->coeffs.candidatePasser[r] * egBase;
  }
}

void UpdatePasserBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.grad.g += position->coeffs.passedPawn[r] * mgBase;
    weights->passedPawn[r].eg.grad.g += position->coeffs.passedPawn[r] * egBase;
  }

  weights->passedPawnEdgeDistance.mg.grad.g += position->coeffs.passedPawnEdgeDistance * mgBase;
  weights->passedPawnEdgeDistance.eg.grad.g += position->coeffs.passedPawnEdgeDistance * egBase;

  weights->passedPawnKingProximity.mg.grad.g += position->coeffs.passedPawnKingProximity * mgBase;
  weights->passedPawnKingProximity.eg.grad.g += position->coeffs.passedPawnKingProximity * egBase;

  for (int r = 0; r < 5; r++) {
    weights->passedPawnAdvance[r].mg.grad.g += position->coeffs.passedPawnAdvance[r] * mgBase;
    weights->passedPawnAdvance[r].eg.grad.g += position->coeffs.passedPawnAdvance[r] * egBase;
  }

  weights->passedPawnEnemySliderBehind.mg.grad.g += position->coeffs.passedPawnEnemySliderBehind * mgBase;
  weights->passedPawnEnemySliderBehind.eg.grad.g += position->coeffs.passedPawnEnemySliderBehind * egBase;

  weights->passedPawnSqRule.mg.grad.g += position->coeffs.passedPawnSqRule * mgBase;
  weights->passedPawnSqRule.eg.grad.g += position->coeffs.passedPawnSqRule * egBase;

  weights->passedPawnUnsupported.mg.grad.g += position->coeffs.passedPawnUnsupported * mgBase;
  weights->passedPawnUnsupported.eg.grad.g += position->coeffs.passedPawnUnsupported * egBase;

  weights->passedPawnOutsideVKnight.mg.grad.g += position->coeffs.passedPawnOutsideVKnight * mgBase;
  weights->passedPawnOutsideVKnight.eg.grad.g += position->coeffs.passedPawnOutsideVKnight * egBase;
}

void UpdatePawnShelterGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->pawnShelter[f][r].mg.grad.g += position->coeffs.pawnShelter[f][r] * mgBase;
      weights->pawnShelter[f][r].eg.grad.g += position->coeffs.pawnShelter[f][r] * egBase;

      weights->pawnStorm[f][r].mg.grad.g += position->coeffs.pawnStorm[f][r] * mgBase;
      weights->pawnStorm[f][r].eg.grad.g += position->coeffs.pawnStorm[f][r] * egBase;
    }
  }

  for (int f = 0; f < 8; f++) {
    weights->blockedPawnStorm[f].mg.grad.g += position->coeffs.blockedPawnStorm[f] * mgBase;
    weights->blockedPawnStorm[f].eg.grad.g += position->coeffs.blockedPawnStorm[f] * egBase;
  }

  weights->castlingRights.mg.grad.g += position->coeffs.castlingRights * mgBase;
  weights->castlingRights.eg.grad.g += position->coeffs.castlingRights * egBase;
}

void UpdateSpaceGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;

  weights->space.mg.grad.g += position->coeffs.space * mgBase / 1024.0;
}

void UpdateComplexityGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd) {
  int sign = (gd->eg > 0) - (gd->eg < 0);
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->complexity >= -fabs(gd->eg));

  weights->complexPawns.mg.grad.g += position->coeffs.complexPawns * egBase * sign;
  weights->complexPawnsBothSides.mg.grad.g += position->coeffs.complexPawnsBothSides * egBase * sign;
  weights->complexOffset.mg.grad.g += position->coeffs.complexOffset * egBase * sign;
}

void UpdateKingSafetyGradients(Position* position, float loss, Weights* weights, EvalGradientData* ks) {
  float mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  float egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (ks->eg == 0.0 || ks->complexity >= -fabs(ks->eg));

  for (int i = 1; i < 5; i++) {
    weights->ksAttackerWeight[i].mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) *
                                              position->coeffs.ksAttackerWeights[BLACK][i] *
                                              position->coeffs.ksAttackerCount[BLACK];
    weights->ksAttackerWeight[i].mg.grad.g += (egBase / 32) * (ks->bDanger > 0) *
                                              position->coeffs.ksAttackerWeights[BLACK][i] *
                                              position->coeffs.ksAttackerCount[BLACK];

    weights->ksAttackerWeight[i].mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) *
                                              position->coeffs.ksAttackerWeights[WHITE][i] *
                                              position->coeffs.ksAttackerCount[WHITE];
    weights->ksAttackerWeight[i].mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) *
                                              position->coeffs.ksAttackerWeights[WHITE][i] *
                                              position->coeffs.ksAttackerCount[WHITE];
  }

  weights->ksWeakSqs.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksWeakSqs[BLACK];
  weights->ksWeakSqs.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksWeakSqs[BLACK];
  weights->ksWeakSqs.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksWeakSqs[WHITE];
  weights->ksWeakSqs.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksWeakSqs[WHITE];

  weights->ksPinned.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksPinned[BLACK];
  weights->ksPinned.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksPinned[BLACK];
  weights->ksPinned.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksPinned[WHITE];
  weights->ksPinned.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksPinned[WHITE];

  weights->ksKnightCheck.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksKnightCheck[BLACK];
  weights->ksKnightCheck.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksKnightCheck[BLACK];
  weights->ksKnightCheck.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksKnightCheck[WHITE];
  weights->ksKnightCheck.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksKnightCheck[WHITE];

  weights->ksBishopCheck.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksBishopCheck[BLACK];
  weights->ksBishopCheck.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksBishopCheck[BLACK];
  weights->ksBishopCheck.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksBishopCheck[WHITE];
  weights->ksBishopCheck.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksBishopCheck[WHITE];

  weights->ksRookCheck.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksRookCheck[BLACK];
  weights->ksRookCheck.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksRookCheck[BLACK];
  weights->ksRookCheck.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksRookCheck[WHITE];
  weights->ksRookCheck.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksRookCheck[WHITE];

  weights->ksQueenCheck.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksQueenCheck[BLACK];
  weights->ksQueenCheck.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksQueenCheck[BLACK];
  weights->ksQueenCheck.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksQueenCheck[WHITE];
  weights->ksQueenCheck.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksQueenCheck[WHITE];

  weights->ksUnsafeCheck.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksUnsafeCheck[BLACK];
  weights->ksUnsafeCheck.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksUnsafeCheck[BLACK];
  weights->ksUnsafeCheck.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksUnsafeCheck[WHITE];
  weights->ksUnsafeCheck.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksUnsafeCheck[WHITE];

  weights->ksEnemyQueen.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksEnemyQueen[BLACK];
  weights->ksEnemyQueen.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksEnemyQueen[BLACK];
  weights->ksEnemyQueen.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksEnemyQueen[WHITE];
  weights->ksEnemyQueen.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksEnemyQueen[WHITE];

  weights->ksKnightDefense.mg.grad.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksKnightDefense[BLACK];
  weights->ksKnightDefense.mg.grad.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksKnightDefense[BLACK];
  weights->ksKnightDefense.mg.grad.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksKnightDefense[WHITE];
  weights->ksKnightDefense.mg.grad.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksKnightDefense[WHITE];
}

void UpdateTempoGradient(Position* position, float loss, Weights* weights) {
  weights->tempo.mg.grad.g += (position->stm == WHITE ? 1 : -1) * loss;
}

void UpdateKPNetworkGradients(Position* position, float loss, KPNetwork* network, EvalGradientData* gd) {
  float lossMg = loss * position->phaseMg * position->scale / MAX_SCALE;
  float lossEg = loss * position->phaseEg * position->scale / MAX_SCALE;
  lossEg *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  float err = lossMg + lossEg;

  // Gradients for last layer is just the err
  network->gBiases1[0].g += err;

  for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++)
    network->gWeights1[i].g += network->hiddenActivations[i] * err;

  // Hidden layer is a scalar of loss * weight leading into output
  for (int i = 0; i < N_KP_HIDDEN; i++) {
    float layerErr = err * network->weights1[i] * ReLuPrime(network->hiddenActivations[i]);

    network->gBiases0[i].g += layerErr;

    // update gradients for features that are enabled in this position
    BitBoard whitePawns = position->whitePawns;
    BitBoard blackPawns = position->blackPawns;

    while (whitePawns) {
      int idx = GetKPNetworkIdx(PAWN_TYPE, popAndGetLsb(&whitePawns), WHITE);
      network->gWeights0[i * N_KP_FEATURES + idx].g += layerErr;
    }

    while (blackPawns) {
      int idx = GetKPNetworkIdx(PAWN_TYPE, popAndGetLsb(&blackPawns), BLACK);
      network->gWeights0[i * N_KP_FEATURES + idx].g += layerErr;
    }

    int idx = GetKPNetworkIdx(KING_TYPE, position->wk, WHITE);
    network->gWeights0[i * N_KP_FEATURES + idx].g += layerErr;

    idx = GetKPNetworkIdx(KING_TYPE, position->bk, BLACK);
    network->gWeights0[i * N_KP_FEATURES + idx].g += layerErr;
  }
}

void ApplyCoeff(float* mg, float* eg, int coeff, Weight* w) {
  *mg += coeff * w->mg.value;
  *eg += coeff * w->eg.value;
}

float EvaluateCoeffs(Position* position, Weights* weights, EvalGradientData* gd, KPNetwork* network) {
  float mg = 0, eg = 0;

  EvaluateMaterialValues(&mg, &eg, position, weights);
  EvaluatePsqtValues(&mg, &eg, position, weights);
  EvaluatePostPsqtValues(&mg, &eg, position, weights);
  EvaluateMobilityValues(&mg, &eg, position, weights);
  EvaluateThreatValues(&mg, &eg, position, weights);
  EvaluatePieceBonusValues(&mg, &eg, position, weights);
  EvaluatePawnBonusValues(&mg, &eg, position, weights);
  EvaluatePasserBonusValues(&mg, &eg, position, weights);
  EvaluatePawnShelterValues(&mg, &eg, position, weights);
  EvaluateSpaceValues(&mg, &eg, position, weights);

  if (TUNE_KS)
    EvaluateKingSafetyValues(&mg, &eg, position, weights, gd);
  else {
    mg += scoreMG(position->coeffs.ks);
    eg += scoreEG(position->coeffs.ks);
  }

  float kpNetEval = KPNetworkPredict(position->whitePawns, position->blackPawns, position->wk, position->bk, network);
  mg += kpNetEval;
  eg += kpNetEval;

  EvaluateComplexityValues(&mg, &eg, position, weights, gd);

  float result = (mg * position->phase + eg * (128 - position->phase)) / 128;
  result = (result * position->scale) / MAX_SCALE;
  return result + (position->stm == WHITE ? weights->tempo.mg.value : -weights->tempo.mg.value);
}

void EvaluateMaterialValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++)
    ApplyCoeff(mg, eg, position->coeffs.pieces[pc], &weights->pieces[pc]);
}

void EvaluatePsqtValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++)
    for (int sq = 0; sq < 32; sq++)
      for (int i = 0; i < 2; i++)
        ApplyCoeff(mg, eg, position->coeffs.psqt[pc][i][sq], &weights->psqt[pc][i][sq]);
}

void EvaluatePostPsqtValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int sq = 0; sq < 12; sq++) {
    ApplyCoeff(mg, eg, position->coeffs.knightPostPsqt[sq], &weights->knightPostPsqt[sq]);

    ApplyCoeff(mg, eg, position->coeffs.bishopPostPsqt[sq], &weights->bishopPostPsqt[sq]);
  }
}

void EvaluateMobilityValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int c = 0; c < 9; c++)
    ApplyCoeff(mg, eg, position->coeffs.knightMobilities[c], &weights->knightMobilities[c]);

  for (int c = 0; c < 14; c++)
    ApplyCoeff(mg, eg, position->coeffs.bishopMobilities[c], &weights->bishopMobilities[c]);

  for (int c = 0; c < 15; c++)
    ApplyCoeff(mg, eg, position->coeffs.rookMobilities[c], &weights->rookMobilities[c]);

  for (int c = 0; c < 28; c++)
    ApplyCoeff(mg, eg, position->coeffs.queenMobilities[c], &weights->queenMobilities[c]);

  for (int c = 0; c < 9; c++)
    ApplyCoeff(mg, eg, position->coeffs.kingMobilities[c], &weights->kingMobilities[c]);
}

void EvaluateThreatValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    ApplyCoeff(mg, eg, position->coeffs.knightThreats[pc], &weights->knightThreats[pc]);
    ApplyCoeff(mg, eg, position->coeffs.bishopThreats[pc], &weights->bishopThreats[pc]);
    ApplyCoeff(mg, eg, position->coeffs.rookThreats[pc], &weights->rookThreats[pc]);
  }

  ApplyCoeff(mg, eg, position->coeffs.kingThreat, &weights->kingThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnThreat, &weights->pawnThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnPushThreat, &weights->pawnPushThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnPushThreatPinned, &weights->pawnPushThreatPinned);
  ApplyCoeff(mg, eg, position->coeffs.hangingThreat, &weights->hangingThreat);
  ApplyCoeff(mg, eg, position->coeffs.knightCheckQueen, &weights->knightCheckQueen);
  ApplyCoeff(mg, eg, position->coeffs.bishopCheckQueen, &weights->bishopCheckQueen);
  ApplyCoeff(mg, eg, position->coeffs.rookCheckQueen, &weights->rookCheckQueen);
}

void EvaluatePieceBonusValues(float* mg, float* eg, Position* position, Weights* weights) {
  ApplyCoeff(mg, eg, position->coeffs.bishopPair, &weights->bishopPair);

  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      ApplyCoeff(mg, eg, position->coeffs.imbalance[i][j], &weights->imbalance[i][j]);

  ApplyCoeff(mg, eg, position->coeffs.minorBehindPawn, &weights->minorBehindPawn);
  ApplyCoeff(mg, eg, position->coeffs.knightPostReachable, &weights->knightPostReachable);
  ApplyCoeff(mg, eg, position->coeffs.bishopPostReachable, &weights->bishopPostReachable);
  ApplyCoeff(mg, eg, position->coeffs.bishopTrapped, &weights->bishopTrapped);
  ApplyCoeff(mg, eg, position->coeffs.rookTrapped, &weights->rookTrapped);
  ApplyCoeff(mg, eg, position->coeffs.badBishopPawns, &weights->badBishopPawns);
  ApplyCoeff(mg, eg, position->coeffs.dragonBishop, &weights->dragonBishop);
  ApplyCoeff(mg, eg, position->coeffs.rookOpenFileOffset, &weights->rookOpenFileOffset);
  ApplyCoeff(mg, eg, position->coeffs.rookOpenFile, &weights->rookOpenFile);
  ApplyCoeff(mg, eg, position->coeffs.rookSemiOpen, &weights->rookSemiOpen);
  ApplyCoeff(mg, eg, position->coeffs.rookToOpen, &weights->rookToOpen);
  ApplyCoeff(mg, eg, position->coeffs.queenOppositeRook, &weights->queenOppositeRook);
  ApplyCoeff(mg, eg, position->coeffs.queenRookBattery, &weights->queenRookBattery);
}

void EvaluatePawnBonusValues(float* mg, float* eg, Position* position, Weights* weights) {
  ApplyCoeff(mg, eg, position->coeffs.defendedPawns, &weights->defendedPawns);
  ApplyCoeff(mg, eg, position->coeffs.doubledPawns, &weights->doubledPawns);
  ApplyCoeff(mg, eg, position->coeffs.openIsolatedPawns, &weights->openIsolatedPawns);
  ApplyCoeff(mg, eg, position->coeffs.backwardsPawns, &weights->backwardsPawns);
  ApplyCoeff(mg, eg, position->coeffs.candidateEdgeDistance, &weights->candidateEdgeDistance);

  for (int f = 0; f < 4; f++)
    ApplyCoeff(mg, eg, position->coeffs.isolatedPawns[f], &weights->isolatedPawns[f]);

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++)
      ApplyCoeff(mg, eg, position->coeffs.connectedPawn[f][r], &weights->connectedPawn[f][r]);
    ApplyCoeff(mg, eg, position->coeffs.candidatePasser[r], &weights->candidatePasser[r]);
  }
}

void EvaluatePasserBonusValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int r = 0; r < 8; r++)
    ApplyCoeff(mg, eg, position->coeffs.passedPawn[r], &weights->passedPawn[r]);

  ApplyCoeff(mg, eg, position->coeffs.passedPawnEdgeDistance, &weights->passedPawnEdgeDistance);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnKingProximity, &weights->passedPawnKingProximity);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnEnemySliderBehind, &weights->passedPawnEnemySliderBehind);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnSqRule, &weights->passedPawnSqRule);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnUnsupported, &weights->passedPawnUnsupported);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnOutsideVKnight, &weights->passedPawnOutsideVKnight);

  for (int r = 0; r < 5; r++)
    ApplyCoeff(mg, eg, position->coeffs.passedPawnAdvance[r], &weights->passedPawnAdvance[r]);
}

void EvaluatePawnShelterValues(float* mg, float* eg, Position* position, Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      ApplyCoeff(mg, eg, position->coeffs.pawnShelter[f][r], &weights->pawnShelter[f][r]);
      ApplyCoeff(mg, eg, position->coeffs.pawnStorm[f][r], &weights->pawnStorm[f][r]);
    }
  }

  for (int r = 0; r < 8; r++)
    ApplyCoeff(mg, eg, position->coeffs.blockedPawnStorm[r], &weights->blockedPawnStorm[r]);

  ApplyCoeff(mg, eg, position->coeffs.castlingRights, &weights->castlingRights);
}

void EvaluateSpaceValues(float* mg, float* eg, Position* position, Weights* weights) {
  *mg += position->coeffs.space * weights->space.mg.value / 1024.0;
}

void EvaluateComplexityValues(float* mg, float* eg, Position* position, Weights* weights, EvalGradientData* gd) {
  float complexity = 0.0;
  complexity += position->coeffs.complexPawns * weights->complexPawns.mg.value;
  complexity += position->coeffs.complexPawnsBothSides * weights->complexPawnsBothSides.mg.value;
  complexity += position->coeffs.complexOffset * weights->complexOffset.mg.value;

  gd->eg = *eg;
  gd->complexity = complexity;

  if (*eg > 0)
    *eg = fmax(0.0, *eg + complexity);
  else if (*eg < 0)
    *eg = fmin(0.0, *eg - complexity);
}

void EvaluateKingSafetyValues(float* mg, float* eg, Position* position, Weights* weights, EvalGradientData* ks) {
  float wDanger = 0.0;
  float bDanger = 0.0;

  for (int i = 1; i < 5; i++) {
    wDanger += position->coeffs.ksAttackerWeights[WHITE][i] * position->coeffs.ksAttackerCount[WHITE] *
               weights->ksAttackerWeight[i].mg.value;
    bDanger += position->coeffs.ksAttackerWeights[BLACK][i] * position->coeffs.ksAttackerCount[BLACK] *
               weights->ksAttackerWeight[i].mg.value;
  }

  wDanger += position->coeffs.ksWeakSqs[WHITE] * weights->ksWeakSqs.mg.value;
  wDanger += position->coeffs.ksPinned[WHITE] * weights->ksPinned.mg.value;
  wDanger += position->coeffs.ksKnightCheck[WHITE] * weights->ksKnightCheck.mg.value;
  wDanger += position->coeffs.ksBishopCheck[WHITE] * weights->ksBishopCheck.mg.value;
  wDanger += position->coeffs.ksRookCheck[WHITE] * weights->ksRookCheck.mg.value;
  wDanger += position->coeffs.ksQueenCheck[WHITE] * weights->ksQueenCheck.mg.value;
  wDanger += position->coeffs.ksUnsafeCheck[WHITE] * weights->ksUnsafeCheck.mg.value;
  wDanger += position->coeffs.ksEnemyQueen[WHITE] * weights->ksEnemyQueen.mg.value;
  wDanger += position->coeffs.ksKnightDefense[WHITE] * weights->ksKnightDefense.mg.value;

  bDanger += position->coeffs.ksWeakSqs[BLACK] * weights->ksWeakSqs.mg.value;
  bDanger += position->coeffs.ksPinned[BLACK] * weights->ksPinned.mg.value;
  bDanger += position->coeffs.ksKnightCheck[BLACK] * weights->ksKnightCheck.mg.value;
  bDanger += position->coeffs.ksBishopCheck[BLACK] * weights->ksBishopCheck.mg.value;
  bDanger += position->coeffs.ksRookCheck[BLACK] * weights->ksRookCheck.mg.value;
  bDanger += position->coeffs.ksQueenCheck[BLACK] * weights->ksQueenCheck.mg.value;
  bDanger += position->coeffs.ksUnsafeCheck[BLACK] * weights->ksUnsafeCheck.mg.value;
  bDanger += position->coeffs.ksEnemyQueen[BLACK] * weights->ksEnemyQueen.mg.value;
  bDanger += position->coeffs.ksKnightDefense[BLACK] * weights->ksKnightDefense.mg.value;

  ks->wDanger = wDanger;
  ks->bDanger = bDanger;

  *mg += -wDanger * fmax(wDanger, 0.0) / 1024;
  *eg += -fmax(wDanger, 0.0) / 32;

  *mg -= -bDanger * fmax(bDanger, 0.0) / 1024;
  *eg -= -fmax(bDanger, 0.0) / 32;
}

void LoadPosition(Board* board, Position* position, ThreadData* thread) {
  memset(&C, 0, sizeof(EvalCoeffs));

  int phase = GetPhase(board);

  position->phaseMg = (float)phase / 128;
  position->phaseEg = 1 - (float)phase / 128;
  position->phase = phase;

  position->whitePawns = board->pieces[PAWN_WHITE];
  position->blackPawns = board->pieces[PAWN_BLACK];
  position->wk = lsb(board->pieces[KING_WHITE]);
  position->bk = lsb(board->pieces[KING_BLACK]);

  position->stm = board->side;

  Score eval = Evaluate(board, thread);
  position->staticEval = board->side == WHITE ? eval : -eval;

  position->scale = Scale(board, C.ss);

  memcpy(&position->coeffs, &C, sizeof(EvalCoeffs));
}

Position* LoadPositions(int* n, Weights* weights, KPNetwork* network) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = calloc(MAX_POSITIONS, sizeof(Position));

  EvalGradientData ks;
  Board board;
  ThreadData* threads = CreatePool(1);

  char buffer[128];

  int p = 0;
  while (p < MAX_POSITIONS && fgets(buffer, 128, fp)) {
    if (strstr(buffer, "[1.0]"))
      positions[p].result = 1.0;
    else if (strstr(buffer, "[0.5]"))
      positions[p].result = 0.5;
    else if (strstr(buffer, "[0.0]"))
      positions[p].result = 0.0;
    else {
      printf("Cannot Parse %s\n", buffer);
      exit(EXIT_FAILURE);
    }

    ParseFen(buffer, &board);

    if (board.checkers)
      continue;

    if (!(board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    if (bits(board.occupancies[BOTH]) == 3 && (board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    LoadPosition(&board, &positions[p], threads);
    if (abs(positions[p].staticEval) > 6000)
      continue;

    float eval = EvaluateCoeffs(&positions[p], weights, &ks, network);
    if (fabs(eval) > 6000)
      continue;

    if (fabs(positions[p].staticEval - eval) > 6) {
      printf("The coefficient based evaluation does NOT match the eval!\n");
      printf("FEN: %s\n", buffer);
      printf("Static: %d, Coeffs: %f\n", positions[p].staticEval, eval);
      exit(1);
    }

    if (!(++p & 4095))
      printf("Loaded %d positions...\r", p);
  }

  printf("Successfully loaded %d positions.\n", p);

  *n = p;

  positions = realloc(positions, sizeof(Position) * p);
  return positions;
}

void ShufflePositions(int n, Position* positions) {
  srand(time(NULL));

  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);

    Position temp = positions[i];
    positions[i] = positions[j];
    positions[j] = temp;
  }
}

void InitMaterialWeights(Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->pieces[pc].mg.value = scoreMG(MATERIAL_VALUES[pc]);
    weights->pieces[pc].eg.value = scoreEG(MATERIAL_VALUES[pc]);
  }
}

void InitPsqtWeights(Weights* weights) {
  for (int sq = 0; sq < 32; sq++) {
    for (int i = 0; i < 2; i++) {
      weights->psqt[PAWN_TYPE][i][sq].mg.value = scoreMG(PAWN_PSQT[i][sq]);
      weights->psqt[PAWN_TYPE][i][sq].eg.value = scoreEG(PAWN_PSQT[i][sq]);

      weights->psqt[KNIGHT_TYPE][i][sq].mg.value = scoreMG(KNIGHT_PSQT[i][sq]);
      weights->psqt[KNIGHT_TYPE][i][sq].eg.value = scoreEG(KNIGHT_PSQT[i][sq]);

      weights->psqt[BISHOP_TYPE][i][sq].mg.value = scoreMG(BISHOP_PSQT[i][sq]);
      weights->psqt[BISHOP_TYPE][i][sq].eg.value = scoreEG(BISHOP_PSQT[i][sq]);

      weights->psqt[ROOK_TYPE][i][sq].mg.value = scoreMG(ROOK_PSQT[i][sq]);
      weights->psqt[ROOK_TYPE][i][sq].eg.value = scoreEG(ROOK_PSQT[i][sq]);

      weights->psqt[QUEEN_TYPE][i][sq].mg.value = scoreMG(QUEEN_PSQT[i][sq]);
      weights->psqt[QUEEN_TYPE][i][sq].eg.value = scoreEG(QUEEN_PSQT[i][sq]);

      weights->psqt[KING_TYPE][i][sq].mg.value = scoreMG(KING_PSQT[i][sq]);
      weights->psqt[KING_TYPE][i][sq].eg.value = scoreEG(KING_PSQT[i][sq]);
    }
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

  for (int c = 0; c < 9; c++) {
    weights->kingMobilities[c].mg.value = scoreMG(KING_MOBILITIES[c]);
    weights->kingMobilities[c].eg.value = scoreEG(KING_MOBILITIES[c]);
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
  }

  weights->kingThreat.mg.value = scoreMG(KING_THREAT);
  weights->kingThreat.eg.value = scoreEG(KING_THREAT);

  weights->pawnThreat.mg.value = scoreMG(PAWN_THREAT);
  weights->pawnThreat.eg.value = scoreEG(PAWN_THREAT);

  weights->pawnPushThreat.mg.value = scoreMG(PAWN_PUSH_THREAT);
  weights->pawnPushThreat.eg.value = scoreEG(PAWN_PUSH_THREAT);

  weights->pawnPushThreatPinned.mg.value = scoreMG(PAWN_PUSH_THREAT_PINNED);
  weights->pawnPushThreatPinned.eg.value = scoreEG(PAWN_PUSH_THREAT_PINNED);

  weights->hangingThreat.mg.value = scoreMG(HANGING_THREAT);
  weights->hangingThreat.eg.value = scoreEG(HANGING_THREAT);

  weights->knightCheckQueen.mg.value = scoreMG(KNIGHT_CHECK_QUEEN);
  weights->knightCheckQueen.eg.value = scoreEG(KNIGHT_CHECK_QUEEN);

  weights->bishopCheckQueen.mg.value = scoreMG(BISHOP_CHECK_QUEEN);
  weights->bishopCheckQueen.eg.value = scoreEG(BISHOP_CHECK_QUEEN);

  weights->rookCheckQueen.mg.value = scoreMG(ROOK_CHECK_QUEEN);
  weights->rookCheckQueen.eg.value = scoreEG(ROOK_CHECK_QUEEN);
}

void InitPieceBonusWeights(Weights* weights) {
  weights->bishopPair.mg.value = scoreMG(BISHOP_PAIR);
  weights->bishopPair.eg.value = scoreEG(BISHOP_PAIR);

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      weights->imbalance[i][j].mg.value = scoreMG(IMBALANCE[i][j]);
      weights->imbalance[i][j].eg.value = scoreEG(IMBALANCE[i][j]);
    }
  }

  weights->minorBehindPawn.mg.value = scoreMG(MINOR_BEHIND_PAWN);
  weights->minorBehindPawn.eg.value = scoreEG(MINOR_BEHIND_PAWN);

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

  weights->rookOpenFileOffset.mg.value = scoreMG(ROOK_OPEN_FILE_OFFSET);
  weights->rookOpenFileOffset.eg.value = scoreEG(ROOK_OPEN_FILE_OFFSET);

  weights->rookOpenFile.mg.value = scoreMG(ROOK_OPEN_FILE);
  weights->rookOpenFile.eg.value = scoreEG(ROOK_OPEN_FILE);

  weights->rookSemiOpen.mg.value = scoreMG(ROOK_SEMI_OPEN);
  weights->rookSemiOpen.eg.value = scoreEG(ROOK_SEMI_OPEN);

  weights->rookToOpen.mg.value = scoreMG(ROOK_TO_OPEN);
  weights->rookToOpen.eg.value = scoreEG(ROOK_TO_OPEN);

  weights->queenOppositeRook.mg.value = scoreMG(QUEEN_OPPOSITE_ROOK);
  weights->queenOppositeRook.eg.value = scoreEG(QUEEN_OPPOSITE_ROOK);

  weights->queenRookBattery.mg.value = scoreMG(QUEEN_ROOK_BATTERY);
  weights->queenRookBattery.eg.value = scoreEG(QUEEN_ROOK_BATTERY);
}

void InitPawnBonusWeights(Weights* weights) {
  weights->defendedPawns.mg.value = scoreMG(DEFENDED_PAWN);
  weights->defendedPawns.eg.value = scoreEG(DEFENDED_PAWN);

  weights->doubledPawns.mg.value = scoreMG(DOUBLED_PAWN);
  weights->doubledPawns.eg.value = scoreEG(DOUBLED_PAWN);

  for (int f = 0; f < 4; f++) {
    weights->isolatedPawns[f].mg.value = scoreMG(ISOLATED_PAWN[f]);
    weights->isolatedPawns[f].eg.value = scoreEG(ISOLATED_PAWN[f]);
  }

  weights->openIsolatedPawns.mg.value = scoreMG(OPEN_ISOLATED_PAWN);
  weights->openIsolatedPawns.eg.value = scoreEG(OPEN_ISOLATED_PAWN);

  weights->backwardsPawns.mg.value = scoreMG(BACKWARDS_PAWN);
  weights->backwardsPawns.eg.value = scoreEG(BACKWARDS_PAWN);

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++) {
      weights->connectedPawn[f][r].mg.value = scoreMG(CONNECTED_PAWN[f][r]);
      weights->connectedPawn[f][r].eg.value = scoreEG(CONNECTED_PAWN[f][r]);
    }

    weights->candidatePasser[r].mg.value = scoreMG(CANDIDATE_PASSER[r]);
    weights->candidatePasser[r].eg.value = scoreEG(CANDIDATE_PASSER[r]);
  }

  weights->candidateEdgeDistance.mg.value = scoreMG(CANDIDATE_EDGE_DISTANCE);
  weights->candidateEdgeDistance.eg.value = scoreEG(CANDIDATE_EDGE_DISTANCE);
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

  for (int r = 0; r < 5; r++) {
    weights->passedPawnAdvance[r].mg.value = scoreMG(PASSED_PAWN_ADVANCE_DEFENDED[r]);
    weights->passedPawnAdvance[r].eg.value = scoreEG(PASSED_PAWN_ADVANCE_DEFENDED[r]);
  }

  weights->passedPawnEnemySliderBehind.mg.value = scoreMG(PASSED_PAWN_ENEMY_SLIDER_BEHIND);
  weights->passedPawnEnemySliderBehind.eg.value = scoreEG(PASSED_PAWN_ENEMY_SLIDER_BEHIND);

  weights->passedPawnSqRule.mg.value = scoreMG(PASSED_PAWN_SQ_RULE);
  weights->passedPawnSqRule.eg.value = scoreEG(PASSED_PAWN_SQ_RULE);

  weights->passedPawnUnsupported.mg.value = scoreMG(PASSED_PAWN_UNSUPPORTED);
  weights->passedPawnUnsupported.eg.value = scoreEG(PASSED_PAWN_UNSUPPORTED);

  weights->passedPawnOutsideVKnight.mg.value = scoreMG(PASSED_PAWN_OUTSIDE_V_KNIGHT);
  weights->passedPawnOutsideVKnight.eg.value = scoreEG(PASSED_PAWN_OUTSIDE_V_KNIGHT);
}

void InitPawnShelterWeights(Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->pawnShelter[f][r].mg.value = scoreMG(PAWN_SHELTER[f][r]);
      weights->pawnShelter[f][r].eg.value = scoreEG(PAWN_SHELTER[f][r]);

      weights->pawnStorm[f][r].mg.value = scoreMG(PAWN_STORM[f][r]);
      weights->pawnStorm[f][r].eg.value = scoreEG(PAWN_STORM[f][r]);
    }
  }

  for (int r = 0; r < 8; r++) {
    weights->blockedPawnStorm[r].mg.value = scoreMG(BLOCKED_PAWN_STORM[r]);
    weights->blockedPawnStorm[r].eg.value = scoreEG(BLOCKED_PAWN_STORM[r]);
  }

  weights->castlingRights.mg.value = scoreMG(CAN_CASTLE);
  weights->castlingRights.eg.value = scoreEG(CAN_CASTLE);
}

void InitSpaceWeights(Weights* weights) { weights->space.mg.value = SPACE; }

void InitComplexityWeights(Weights* weights) {
  weights->complexPawns.mg.value = COMPLEXITY_PAWNS;
  weights->complexPawnsBothSides.mg.value = COMPLEXITY_PAWNS_BOTH_SIDES;
  weights->complexOffset.mg.value = COMPLEXITY_OFFSET;
}

void InitKingSafetyWeights(Weights* weights) {
  for (int i = 1; i < 5; i++)
    weights->ksAttackerWeight[i].mg.value = KS_ATTACKER_WEIGHTS[i];

  weights->ksWeakSqs.mg.value = KS_WEAK_SQS;
  weights->ksPinned.mg.value = KS_PINNED;
  weights->ksKnightCheck.mg.value = KS_KNIGHT_CHECK;
  weights->ksBishopCheck.mg.value = KS_BISHOP_CHECK;
  weights->ksRookCheck.mg.value = KS_ROOK_CHECK;
  weights->ksQueenCheck.mg.value = KS_QUEEN_CHECK;
  weights->ksUnsafeCheck.mg.value = KS_UNSAFE_CHECK;
  weights->ksEnemyQueen.mg.value = KS_ENEMY_QUEEN;
  weights->ksKnightDefense.mg.value = KS_KNIGHT_DEFENSE;
}

void InitTempoWeight(Weights* weights) { weights->tempo.mg.value = TEMPO; }

void PrintWeights(Weights* weights, int epoch, float error) {
  FILE* fp;
  fp = fopen("weights.c", "w");

  if (fp == NULL)
    return;

  fprintf(fp, "// Berserk is a UCI compliant chess engine written in C\n");
  fprintf(fp, "// Copyright (C) 2021 Jay Honnold\n\n");
  fprintf(fp, "// This program is free software: you can redistribute it and/or modify\n");
  fprintf(fp, "// it under the terms of the GNU General Public License as published by\n");
  fprintf(fp, "// the Free Software Foundation, either version 3 of the License, or\n");
  fprintf(fp, "// (at your option) any later version.\n\n");
  fprintf(fp, "// This program is distributed in the hope that it will be useful,\n");
  fprintf(fp, "// but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
  fprintf(fp, "// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
  fprintf(fp, "// GNU General Public License for more details.\n\n");
  fprintf(fp, "// You should have received a copy of the GNU General Public License\n");
  fprintf(fp, "// along with this program.  If not, see <https://www.gnu.org/licenses/>.\n\n");

  fprintf(fp, "// This is an auto-generated file.\n// Generated: %ld, Epoch: %d, Error: %1.8f, Dataset: %s\n\n",
          GetTimeMS(), epoch, error, EPD_FILE_PATH);
  fprintf(fp, "#include \"weights.h\"\n\n// clang-format off");

  fprintf(fp, "\nconst Score MATERIAL_VALUES[7] = {");
  PrintWeightArray(fp, weights->pieces, 5, 0);
  fprintf(fp, " S(   0,   0), S(   0,   0) };\n");

  fprintf(fp, "\nconst Score BISHOP_PAIR = ");
  PrintWeight(fp, &weights->bishopPair);

  fprintf(fp, "\nconst Score PAWN_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[PAWN_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[PAWN_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score KNIGHT_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[KNIGHT_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[KNIGHT_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score BISHOP_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[BISHOP_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[BISHOP_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score ROOK_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[ROOK_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[ROOK_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score QUEEN_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[QUEEN_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[QUEEN_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score KING_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[KING_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[KING_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

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

  fprintf(fp, "\nconst Score KING_MOBILITIES[9] = {\n");
  PrintWeightArray(fp, weights->kingMobilities, 9, 3);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score MINOR_BEHIND_PAWN = ");
  PrintWeight(fp, &weights->minorBehindPawn);

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

  fprintf(fp, "\nconst Score ROOK_OPEN_FILE_OFFSET = ");
  PrintWeight(fp, &weights->rookOpenFileOffset);

  fprintf(fp, "\nconst Score ROOK_OPEN_FILE = ");
  PrintWeight(fp, &weights->rookOpenFile);

  fprintf(fp, "\nconst Score ROOK_SEMI_OPEN = ");
  PrintWeight(fp, &weights->rookSemiOpen);

  fprintf(fp, "\nconst Score ROOK_TO_OPEN = ");
  PrintWeight(fp, &weights->rookToOpen);

  fprintf(fp, "\nconst Score QUEEN_OPPOSITE_ROOK = ");
  PrintWeight(fp, &weights->queenOppositeRook);

  fprintf(fp, "\nconst Score QUEEN_ROOK_BATTERY = ");
  PrintWeight(fp, &weights->queenRookBattery);

  fprintf(fp, "\nconst Score DEFENDED_PAWN = ");
  PrintWeight(fp, &weights->defendedPawns);

  fprintf(fp, "\nconst Score DOUBLED_PAWN = ");
  PrintWeight(fp, &weights->doubledPawns);

  fprintf(fp, "\nconst Score ISOLATED_PAWN[4] = {\n");
  PrintWeightArray(fp, weights->isolatedPawns, 4, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score OPEN_ISOLATED_PAWN = ");
  PrintWeight(fp, &weights->openIsolatedPawns);

  fprintf(fp, "\nconst Score BACKWARDS_PAWN = ");
  PrintWeight(fp, &weights->backwardsPawns);

  fprintf(fp, "\nconst Score CONNECTED_PAWN[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->connectedPawn[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score CANDIDATE_PASSER[8] = {\n");
  PrintWeightArray(fp, weights->candidatePasser, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score CANDIDATE_EDGE_DISTANCE = ");
  PrintWeight(fp, &weights->candidateEdgeDistance);

  fprintf(fp, "\nconst Score PASSED_PAWN[8] = {\n");
  PrintWeightArray(fp, weights->passedPawn, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN_EDGE_DISTANCE = ");
  PrintWeight(fp, &weights->passedPawnEdgeDistance);

  fprintf(fp, "\nconst Score PASSED_PAWN_KING_PROXIMITY = ");
  PrintWeight(fp, &weights->passedPawnKingProximity);

  fprintf(fp, "\nconst Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {\n");
  PrintWeightArray(fp, weights->passedPawnAdvance, 5, 5);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = ");
  PrintWeight(fp, &weights->passedPawnEnemySliderBehind);

  fprintf(fp, "\nconst Score PASSED_PAWN_SQ_RULE = ");
  PrintWeight(fp, &weights->passedPawnSqRule);

  fprintf(fp, "\nconst Score PASSED_PAWN_UNSUPPORTED = ");
  PrintWeight(fp, &weights->passedPawnUnsupported);

  fprintf(fp, "\nconst Score PASSED_PAWN_OUTSIDE_V_KNIGHT = ");
  PrintWeight(fp, &weights->passedPawnOutsideVKnight);

  fprintf(fp, "\nconst Score KNIGHT_THREATS[6] = {");
  PrintWeightArray(fp, weights->knightThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_THREATS[6] = {");
  PrintWeightArray(fp, weights->bishopThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_THREATS[6] = {");
  PrintWeightArray(fp, weights->rookThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KING_THREAT = ");
  PrintWeight(fp, &weights->kingThreat);

  fprintf(fp, "\nconst Score PAWN_THREAT = ");
  PrintWeight(fp, &weights->pawnThreat);

  fprintf(fp, "\nconst Score PAWN_PUSH_THREAT = ");
  PrintWeight(fp, &weights->pawnPushThreat);

  fprintf(fp, "\nconst Score PAWN_PUSH_THREAT_PINNED = ");
  PrintWeight(fp, &weights->pawnPushThreatPinned);

  fprintf(fp, "\nconst Score HANGING_THREAT = ");
  PrintWeight(fp, &weights->hangingThreat);

  fprintf(fp, "\nconst Score KNIGHT_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->knightCheckQueen);

  fprintf(fp, "\nconst Score BISHOP_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->bishopCheckQueen);

  fprintf(fp, "\nconst Score ROOK_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->rookCheckQueen);

  fprintf(fp, "\nconst Score SPACE = %d;\n", (int)round(weights->space.mg.value));

  fprintf(fp, "\nconst Score IMBALANCE[5][5] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->imbalance[0], 1, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[1], 2, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[2], 3, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[3], 4, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[4], 5, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_SHELTER[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->pawnShelter[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_STORM[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->pawnStorm[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BLOCKED_PAWN_STORM[8] = {\n");
  PrintWeightArray(fp, weights->blockedPawnStorm, 8, 0);
  fprintf(fp, "\n};\n");

  fprintf(fp, "\nconst Score CAN_CASTLE = ");
  PrintWeight(fp, &weights->castlingRights);

  fprintf(fp, "\nconst Score COMPLEXITY_PAWNS = %d;\n", (int)round(weights->complexPawns.mg.value));

  fprintf(fp, "\nconst Score COMPLEXITY_PAWNS_BOTH_SIDES = %d;\n", (int)round(weights->complexPawnsBothSides.mg.value));

  fprintf(fp, "\nconst Score COMPLEXITY_OFFSET = %d;\n", (int)round(weights->complexOffset.mg.value));

  fprintf(fp, "\nconst Score KS_ATTACKER_WEIGHTS[5] = {\n");
  fprintf(fp, " 0, %d, %d, %d, %d", (int)round(weights->ksAttackerWeight[1].mg.value),
          (int)round(weights->ksAttackerWeight[2].mg.value), (int)round(weights->ksAttackerWeight[3].mg.value),
          (int)round(weights->ksAttackerWeight[4].mg.value));
  fprintf(fp, "\n};\n");

  fprintf(fp, "\nconst Score KS_WEAK_SQS = %d;\n", (int)round(weights->ksWeakSqs.mg.value));

  fprintf(fp, "\nconst Score KS_PINNED = %d;\n", (int)round(weights->ksPinned.mg.value));

  fprintf(fp, "\nconst Score KS_KNIGHT_CHECK = %d;\n", (int)round(weights->ksKnightCheck.mg.value));

  fprintf(fp, "\nconst Score KS_BISHOP_CHECK = %d;\n", (int)round(weights->ksBishopCheck.mg.value));

  fprintf(fp, "\nconst Score KS_ROOK_CHECK = %d;\n", (int)round(weights->ksRookCheck.mg.value));

  fprintf(fp, "\nconst Score KS_QUEEN_CHECK = %d;\n", (int)round(weights->ksQueenCheck.mg.value));

  fprintf(fp, "\nconst Score KS_UNSAFE_CHECK = %d;\n", (int)round(weights->ksUnsafeCheck.mg.value));

  fprintf(fp, "\nconst Score KS_ENEMY_QUEEN = %d;\n", (int)round(weights->ksEnemyQueen.mg.value));

  fprintf(fp, "\nconst Score KS_KNIGHT_DEFENSE = %d;\n", (int)round(weights->ksKnightDefense.mg.value));

  fprintf(fp, "\nconst Score TEMPO = %d;\n", (int)round(weights->tempo.mg.value));

  fprintf(fp, "// clang-format on\n");
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