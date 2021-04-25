#ifdef TUNE

#ifndef TUNE_H
#define TUNE_H

#include "types.h"

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\texel-set-clean.epd"
#define THREADS 32

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
  Weight material[5];
  Weight bishopPair;

  Weight psqt[6][32];
  Weight knightPostPsqt[32];
  Weight bishopPostPsqt[32];

  Weight knightMobilities[9];
  Weight bishopMobilities[14];
  Weight rookMobilities[15];
  Weight queenMobilities[28];

  Weight knightPostReachable;
  Weight bishopPostReachable;
  Weight bishopTrapped;
  Weight rookTrapped;
  Weight rookOpenFile;
  Weight rookSemiOpen;
  Weight rookOppositeKing;
  Weight rookAdjacentKing;

  Weight doubledPawns;
  Weight opposedIsolatedPawns;
  Weight openIsolatedPawns;
  Weight backwardsPawns;
  Weight connectedPawn[8];

  Weight passedPawn[8];
  Weight passedPawnEdgeDistance;
  Weight passedPawnKingProximity;
  Weight passedPawnAdvance;

  Weight knightThreats[6];
  Weight bishopThreats[6];
  Weight rookThreats[6];
  Weight kingThreats[6];

  Weight ksAttackerWeight[5];
  Weight ksSafeCheck;
  Weight ksUnsafeCheck;
  Weight ksSqAttack;
  Weight ksWeak;
  Weight ksEnemyQueen;
  Weight ksKnightDefender;

  Weight ksPawnShelter[4][8];
  Weight ksPawnStorm[4][8];
  Weight ksBlocked[8];
  Weight ksFile[4];
} Weights;

typedef struct {
  int8_t pieces[2][5];
  int8_t psqt[2][6][32];
  int8_t bishopPair[2];
  int8_t bishopTrapped[2];
  int8_t knightPostPsqt[2][32];
  int8_t bishopPostPsqt[2][32];
  int8_t knightPostReachable[2];
  int8_t bishopPostReachable[2];
  int8_t knightMobilities[2][9];
  int8_t bishopMobilities[2][14];
  int8_t rookMobilities[2][15];
  int8_t queenMobilities[2][28];
  int8_t doubledPawns[2];
  int8_t opposedIsolatedPawns[2];
  int8_t openIsolatedPawns[2];
  int8_t backwardsPawns[2];
  int8_t connectedPawn[2][8];
  int8_t passedPawn[2][8];
  int8_t passedPawnAdvance[2];
  int8_t passedPawnEdgeDistance[2];
  int8_t passedPawnKingProximity[2];
  int8_t rookOpenFile[2];
  int8_t rookSemiOpen[2];
  int8_t rookOppositeKing[2];
  int8_t rookAdjacentKing[2];
  int8_t rookTrapped[2];
  int8_t knightThreats[2][6];
  int8_t bishopThreats[2][6];
  int8_t rookThreats[2][6];
  int8_t kingThreats[2][6];
  int ks[2];

  int8_t ss;
} EvalCoeffs;

typedef struct {
  int8_t phase;
  int8_t stm;
  float result;
  float scale;
  float phaseMg;
  float phaseEg;
  Score staticEval;
  EvalCoeffs coeffs;
  char fen[128];
} Position;

typedef struct {
  float error;
  int n;
  Position* positions;
  Weights* weights;
} GradientUpdate;

void Tune();

void ValidateEval(int n, Position* positions, Weights* weights);
void DetermineK(int n, Position* positions, Weights* weights);

void UpdateParam(Param* p);
void UpdateWeights(Weights* weights);
void UpdateAndTrain(int epoch, int n, Position* positions, Weights* weights);

void* UpdateGradients(void* arg);
void UpdateMaterialGradients(Position* position, double loss, Weights* weights);
void UpdatePsqtGradients(Position* position, double loss, Weights* weights);
void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights);
void UpdateMobilityGradients(Position* position, double loss, Weights* weights);
void UpdateThreatGradients(Position* position, double loss, Weights* weights);
void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights);
void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights);
void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights);

double EvaluateCoeffs(Position* position, Weights* weights);
void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights);

void InitMaterialWeights(Weights* weights);
void InitPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitMobilityWeights(Weights* weights);
void InitThreatWeights(Weights* weights);
void InitPieceBonusWeights(Weights* weights);
void InitPawnBonusWeights(Weights* weights);
void InitPasserBonusWeights(Weights* weights);

void LoadPosition(Board* board, Position* position);
void ResetCoeffs();
void CopyCoeffsInto(EvalCoeffs* c);
Position* LoadPositions(int* n);

double Sigmoid(double s);

void PrintWeights(Weights* weights);
void PrintWeightArray(Weight* weights, int n, int wrap);
void PrintWeight(Weight* weight);

#endif
#endif