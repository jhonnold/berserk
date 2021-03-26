#ifdef TUNE
#ifndef TEXEL_H
#define TEXEL_H

#include <stdio.h>
#include <stdlib.h>

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\berserk-texel-quiets.epd"

typedef struct {
  double result;
  double error;
  char fen[100];
} Position;

typedef struct {
  double* errors;
  Position* positions;
  int n;
  int threadid;
} BatchJob;

typedef struct {
  Score* param;
  char* name;
} TexelParam;

void Texel();
void SGD(TexelParam* params, int numParams, Position* positions, int numPositions);
void CalculateGradients(double* gradients, TexelParam* params, int numParams, Position* positions, int numPositions,
                        double base);
Position* loadPositions();
void addParam(char* name, Score* p, TexelParam* params, int* n);
void determineK(Position* positions, int n);
double totalError(Position* positions, int n);
void* batchError(void* arg);
double error(Position* p);
double sigmoid(int score);
void PrintParams(TexelParam* params, int numParams);

#endif
#endif