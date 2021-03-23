#ifndef TEXEL_H
#define TEXEL_H

#include <stdio.h>
#include <stdlib.h>

#define EPD_FILE_PATH "quiet-labeled.epd"

typedef struct {
  double result;
  double error;
  char fen[100];
} Position;

Position* loadPositions();
void determineK(Position* positions, int n);
void texel();
double totalError(Position* positions, int n);
double error(Position* p);
double sigmoid(int score);

#endif