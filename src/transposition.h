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

#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include "types.h"

enum { TT_LOWER, TT_UPPER, TT_EXACT };

extern const TTValue NO_ENTRY;
extern TTValue* TRANSPOSITION_ENTRIES;
extern int POWER;

#define BUCKET_SIZE 2

#define TTIdx(hash) ((int)((hash) >> (64 - POWER)) << 1)
#define TTEntry(score, flag, depth, move, eval)                                                                        \
  ((TTValue)((uint16_t)(eval)) << 48) | ((TTValue)((uint16_t)(score)) << 32) | ((TTValue)(flag) << 30) |               \
      ((TTValue)(depth) << 24) | ((TTValue)(move))
#define TTFlag(value) ((int)((unsigned)((value)&0xC0000000) >> 30))
#define TTMove(value) ((Move)((value)&0xFFFFFF))
#define TTDepth(value) ((int)((value)&0x3F000000) >> 24)
#define TTEval(value) ((int)((int16_t)((value) >> 48)))

void TTInit(int mb);
void TTFree();
void TTClear();
TTValue TTProbe(uint64_t hash);
int TTScore(TTValue value, int ply);
TTValue TTPut(uint64_t hash, int depth, int score, int flag, Move move, int ply, int eval);

#endif