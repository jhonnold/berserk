#ifdef TUNE

#ifndef TUNE_H
#define TUNE_H

#include "types.h"

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\quiet-set.epd"
#define THREADS 32

typedef struct {
  double value;
  double g;
  double M;
  double V;
  double epoch;
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
  int pieces[2][5];
  int psqt[2][6][32];
  int bishopPair[2];
  int bishopTrapped[2];
  int knightPostPsqt[2][32];
  int bishopPostPsqt[2][32];
  int knightPostReachable[2];
  int bishopPostReachable[2];
  int knightMobilities[2][9];
  int bishopMobilities[2][14];
  int rookMobilities[2][15];
  int queenMobilities[2][28];
  int doubledPawns[2];
  int opposedIsolatedPawns[2];
  int openIsolatedPawns[2];
  int backwardsPawns[2];
  int connectedPawn[2][8];
  int passedPawn[2][8];
  int passedPawnAdvance[2];
  int passedPawnEdgeDistance[2];
  int passedPawnKingProximity[2];
  int rookOpenFile[2];
  int rookSemiOpen[2];
  int rookOppositeKing[2];
  int rookAdjacentKing[2];
  int rookTrapped[2];
  int knightThreats[2][6];
  int bishopThreats[2][6];
  int rookThreats[2][6];
  int kingThreats[2][6];

  int ks[2];
} EvalCoeffs;

typedef struct {
  double result;
  double scale;
  double phaseMg;
  double phaseEg;
  int phase;
  int stm;
  Score staticEval;
  EvalCoeffs coeffs;
} Position;

typedef struct {
  double error;
  int n;
  Position* positions;
  Weights* weights;
} GradientUpdate;

void Tune();

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
Position* LoadPositions(int* n);

double Sigmoid(double s);

void PrintWeights(Weights* weights);
void PrintWeightArray(Weight* weights, int n, int wrap);
void PrintWeight(Weight* weight);

#endif
#endif