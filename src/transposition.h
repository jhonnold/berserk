#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include <inttypes.h>

#include "types.h"

enum { TT_LOWER, TT_UPPER, TT_EXACT };

extern const TTValue NO_ENTRY;
extern TTValue* TRANSPOSITION_ENTRIES;
extern int POWER;

#define BUCKET_SIZE 2

#define ttIdx(hash) ((int)((hash) >> (64 - POWER)) << 1)
#define ttEntry(score, flag, depth, move)                                                                              \
  ((TTValue)(score) << 32) | ((TTValue)(flag) << 30) | ((TTValue)(depth) << 24) | ((TTValue)(move))
#define ttFlag(value) ((int)((value)&0xC0000000) >> 30)
#define ttMove(value) ((Move)((value)&0xFFFFFF))
#define ttDepth(value) ((int)((value)&0x3F000000) >> 24)

void ttInit(int mb);
void ttFree();
void ttClear();
TTValue ttProbe(uint64_t hash);
int ttScore(TTValue value, int ply);
void ttPut(uint64_t hash, int depth, int score, int flag, Move move, int ply);

#endif