#ifndef EVAL_H
#define EVAL_H

#include <inttypes.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

extern const int pawnValue;
extern const int knightValue;
extern const int bishopValue;
extern const int rookValue;
extern const int queenValue;
extern const int kingValue;
extern const int materialValues[];

extern int baseMaterialValues[12][64];

void initPositionValues();

int getPhase(Board* board);
int taper(int score, int phase);

int Evaluate(Board* board);
int TraceEvaluate(Board* board);

#endif