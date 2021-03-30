#ifdef TUNE
#ifndef TEXEL_H
#define TEXEL_H

#include <stdio.h>
#include <stdlib.h>

#include "types.h"

typedef struct {
  double result;
  int phase;
  double factors[2];
  char fen[128];
} Position;

typedef struct {
  Position* positions;
  int n;
  int threadid;
  double error;
} BatchJob;

typedef struct {
  Score* param;
  Score min;
  Score max;
  char* name;
} TexelParam;

void Texel();
void SGD(TexelParam* params, int numParams, Position* positions, int numPositions);
void LocalSearch(TexelParam* params, int numParams, Position* positions, int numPositions);
void CalculateGradients(double* gradients, TexelParam* params, int numParams, Position* positions, int numPositions,
                        double base);
Position* loadPositions();

void PrintParams(TexelParam* params, int numParams, double best, double current, int epoch);

void determineK(Position* positions, int n);

double totalError(Position* positions, int n);
void* batchError(void* arg);
double error(Position* p);

double sigmoid(Score score);

void addParam(char* name, Score* p, TexelParam* params, int* n);
void addParams(TexelParam* params, int* numParams);

void shufflePositions(Position* positions, int n);
#endif
#endif