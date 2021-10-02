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

#ifndef TUNE_H
#define TUNE_H

#include <stdio.h>

#include "../types.h"

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\lichess-big3-resolved.book"
#define THREADS 30
#define TUNE_KS 0

typedef struct {
  float value;
  Gradient grad;
} Param;

typedef struct {
  Param mg;
  Param eg;
} Weight;

typedef struct {
  Weight pieces[5];
  Weight psqt[6][2][32];
  Weight bishopPair;

  Weight knightPostPsqt[12];
  Weight bishopPostPsqt[12];

  Weight knightMobilities[9];
  Weight bishopMobilities[14];
  Weight rookMobilities[15];
  Weight queenMobilities[28];
  Weight kingMobilities[9];

  Weight minorBehindPawn;
  Weight knightPostReachable;
  Weight bishopPostReachable;
  Weight bishopTrapped;
  Weight rookTrapped;
  Weight badBishopPawns;
  Weight dragonBishop;
  Weight rookOpenFileOffset;
  Weight rookOpenFile;
  Weight rookSemiOpen;
  Weight rookToOpen;
  Weight queenOppositeRook;
  Weight queenRookBattery;

  Weight defendedPawns;
  Weight doubledPawns;
  Weight isolatedPawns[4];
  Weight openIsolatedPawns;
  Weight backwardsPawns;
  Weight connectedPawn[4][8];
  Weight candidatePasser[8];
  Weight candidateEdgeDistance;

  Weight passedPawn[8];
  Weight passedPawnEdgeDistance;
  Weight passedPawnKingProximity;
  Weight passedPawnAdvance[5];
  Weight passedPawnEnemySliderBehind;
  Weight passedPawnSqRule;
  Weight passedPawnUnsupported;
  Weight passedPawnOutsideVKnight;

  Weight knightThreats[6];
  Weight bishopThreats[6];
  Weight rookThreats[6];
  Weight kingThreat;
  Weight pawnThreat;
  Weight pawnPushThreat;
  Weight pawnPushThreatPinned;
  Weight hangingThreat;
  Weight knightCheckQueen;
  Weight bishopCheckQueen;
  Weight rookCheckQueen;

  Weight space;

  Weight imbalance[5][5];

  Weight pawnShelter[4][8];
  Weight pawnStorm[4][8];
  Weight blockedPawnStorm[8];
  Weight castlingRights;

  Weight complexPawns;
  Weight complexPawnsBothSides;
  Weight complexOffset;

  Weight ksAttackerWeight[5];
  Weight ksWeakSqs;
  Weight ksPinned;
  Weight ksKnightCheck;
  Weight ksBishopCheck;
  Weight ksRookCheck;
  Weight ksQueenCheck;
  Weight ksUnsafeCheck;
  Weight ksEnemyQueen;
  Weight ksKnightDefense;

  Weight tempo;
} Weights;

typedef struct {
  uint8_t phase;
  int8_t stm;
  float result;
  int scale;
  float phaseMg;
  float phaseEg;
  BitBoard whitePawns;
  BitBoard blackPawns;
  Score staticEval;
  EvalCoeffs coeffs;
} Position;

typedef struct {
  float wDanger;
  float bDanger;
  float complexity;
  float eg;
} EvalGradientData;

typedef struct {
  float error;
  int n;
  Position* positions;
  Weights* weights;
  Network* network;
} GradientUpdate;

void Tune();

void ValidateEval(int n, Position* positions, Weights* weights);
float TotalStaticError(int n, Position* positions);

void UpdateAndApplyGradient(float* v, Gradient* grad);
void UpdateParam(Param* p);
void UpdateWeight(Weight* w);
void UpdateNetwork(Network* network);
void UpdateWeights(Weights* weights);
void MergeGradient(Weight* dest, Weight* src);
float UpdateAndTrain(int n, Position* positions, Weights* weights, Network* network);
void ResetNetworkGradients(Network* network);

void* UpdateGradients(void* arg);
void UpdateMaterialGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePsqtGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePostPsqtGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdateMobilityGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdateThreatGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePieceBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePawnBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePasserBonusGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdatePawnShelterGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdateSpaceGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdateComplexityGradients(Position* position, float loss, Weights* weights, EvalGradientData* gd);
void UpdateKingSafetyGradients(Position* position, float loss, Weights* weights, EvalGradientData* ks);
void UpdateTempoGradient(Position* position, float loss, Weights* weights);
void UpdateNetworkGradients(Position* position, float loss, Network* network, EvalGradientData* gd);

void ApplyCoeff(float* mg, float* eg, int coeff, Weight* w);
float EvaluateCoeffs(Position* position, Weights* weights, EvalGradientData* gd, Network* network);
void EvaluateMaterialValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePsqtValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePostPsqtValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluateMobilityValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluateThreatValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePieceBonusValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePawnBonusValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePasserBonusValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluatePawnShelterValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluateSpaceValues(float* mg, float* eg, Position* position, Weights* weights);
void EvaluateComplexityValues(float* mg, float* eg, Position* position, Weights* weights, EvalGradientData* gd);
void EvaluateKingSafetyValues(float* mg, float* eg, Position* position, Weights* weights, EvalGradientData* ks);

void InitMaterialWeights(Weights* weights);
void InitPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitMobilityWeights(Weights* weights);
void InitThreatWeights(Weights* weights);
void InitPieceBonusWeights(Weights* weights);
void InitPawnBonusWeights(Weights* weights);
void InitPasserBonusWeights(Weights* weights);
void InitPawnShelterWeights(Weights* weights);
void InitSpaceWeights(Weights* weights);
void InitComplexityWeights(Weights* weights);
void InitKingSafetyWeights(Weights* weights);
void InitTempoWeight(Weights* weights);

void LoadPosition(Board* board, Position* position, ThreadData* thread);
Position* LoadPositions(int* n, Weights* weights, Network* network);

void PrintWeights(Weights* weights, int epoch, float error);
void PrintWeightArray(FILE* fp, Weight* weights, int n, int wrap);
void PrintWeight(FILE* fp, Weight* weight);

#endif
#endif