#ifdef TUNE

#ifndef TUNE_H
#define TUNE_H

#include <stdio.h>

#include "types.h"

#define EPD_FILE_PATH "/Users/jhonnold/Downloads/texel-set-clean.epd"
#define THREADS 1

typedef struct {
  int epoch;
  float value;
  float g;
  float M;
  float V;
} Param;

typedef struct {
  Param mg;
  Param eg;
} Weight;

typedef struct {
  Weight pieces[5];
  Weight psqt[6][32];
  Weight bishopPair;

  Weight knightPostPsqt[12];
  Weight bishopPostPsqt[12];

  Weight knightMobilities[9];
  Weight bishopMobilities[14];
  Weight rookMobilities[15];
  Weight queenMobilities[28];

  Weight knightPostReachable;
  Weight bishopPostReachable;
  Weight bishopTrapped;
  Weight rookTrapped;
  Weight badBishopPawns;
  Weight dragonBishop;
  Weight rookOpenFile;
  Weight rookSemiOpen;

  Weight doubledPawns;
  Weight opposedIsolatedPawns;
  Weight openIsolatedPawns;
  Weight backwardsPawns;
  Weight connectedPawn[8];
  Weight candidatePasser[8];

  Weight passedPawn[8];
  Weight passedPawnEdgeDistance;
  Weight passedPawnKingProximity;
  Weight passedPawnAdvance;

  Weight knightThreats[6];
  Weight bishopThreats[6];
  Weight rookThreats[6];
  Weight kingThreats[6];
  Weight pawnThreat;
  Weight pawnPushThreat;
  Weight hangingThreat;

  Weight pawnShelter[4][8];
  Weight pawnStorm[4][8];
  Weight blockedPawnStorm[8];
  Weight kingFile[4];
} Weights;

typedef struct {
  uint8_t phase;
  int8_t stm;
  float result;
  int scale;
  float phaseMg;
  float phaseEg;
  Score staticEval;
  EvalCoeffs coeffs;
} Position;

typedef struct {
  float error;
  int n;
  Position* positions;
  Weights* weights;
} GradientUpdate;

void Tune();

void ValidateEval(int n, Position* positions, Weights* weights);
void DetermineK(int n, Position* positions);

void UpdateParam(Param* p);
void UpdateWeight(Weight* w);
void UpdateWeights(Weights* weights);
void MergeWeightGradients(Weight* dest, Weight* src);
double UpdateAndTrain(int epoch, int n, Position* positions, Weights* weights);

void* UpdateGradients(void* arg);
void UpdateMaterialGradients(Position* position, double loss, Weights* weights);
void UpdatePsqtGradients(Position* position, double loss, Weights* weights);
void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights);
void UpdateMobilityGradients(Position* position, double loss, Weights* weights);
void UpdateThreatGradients(Position* position, double loss, Weights* weights);
void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights);
void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights);
void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights);
void UpdatePawnShelterGradients(Position* position, double loss, Weights* weights);

void ApplyCoeff(double* mg, double* eg, int coeff, Weight* w);
double EvaluateCoeffs(Position* position, Weights* weights);
void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePawnShelterValues(double* mg, double* eg, Position* position, Weights* weights);

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

void LoadPosition(Board* board, Position* position, ThreadData* thread);
Position* LoadPositions(int* n);

double Sigmoid(double s);

void PrintWeights(Weights* weights, int epoch, double error);
void PrintWeightArray(FILE* fp, Weight* weights, int n, int wrap);
void PrintWeight(FILE* fp, Weight* weight);

#endif
#endif