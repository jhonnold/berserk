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

#define NO_ENTRY 0ULL
#define MEGABYTE 0x100000ULL
#define BUCKET_SIZE 4

typedef struct {
  uint32_t hash, move;
  int16_t eval, score;
  uint8_t flags, depth;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
} TTBucket;

typedef struct {
  TTBucket* buckets;
  uint64_t mask;
  uint64_t size;
} TTTable;

enum { TT_LOWER = 1, TT_UPPER = 2, TT_EXACT = 4 };

extern TTTable TT;

size_t TTInit(int mb);
void TTFree();
void TTClear();
void TTPrefetch(uint64_t hash);
TTEntry* TTProbe(int* hit, uint64_t hash);
int TTScore(TTEntry* e, int ply);
void TTPut(uint64_t hash, uint8_t depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval);
int TTFull();

#endif