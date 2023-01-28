// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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
#include "util.h"

#define NO_ENTRY    0ULL
#define MEGABYTE    (1024ull * 1024ull)
#define BUCKET_SIZE 5

typedef struct {
  uint16_t hash;
  uint8_t depth, flags;
  Move move;
  int16_t score, eval;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
  uint32_t padding;
} TTBucket;

typedef struct {
  void* mem;
  TTBucket* buckets;
  uint64_t count;
  uint8_t age;
} TTTable;

enum {
  TT_UNKNOWN = 0,
  TT_LOWER   = 1,
  TT_UPPER   = 2,
  TT_EXACT   = 4,
  TT_PV      = 8
};

extern TTTable TT;

size_t TTInit(int mb);
void TTFree();
void TTClearPart(int idx);
void TTClear();
void TTUpdate();
void TTPrefetch(uint64_t hash);
TTEntry* TTProbe(uint64_t hash, int* hit);
void TTPut(TTEntry* tt,
           uint64_t hash,
           int depth,
           int16_t score,
           uint8_t flag,
           Move move,
           int ply,
           int16_t eval,
           int pv);
int TTFull();

#define HASH_MAX ((int) (pow(2, 32) * sizeof(TTBucket) / MEGABYTE))

INLINE int TTScore(TTEntry* e, int ply) {
  if (e->score == UNKNOWN)
    return UNKNOWN;

  return e->score > MATE_BOUND ? e->score - ply : e->score < -MATE_BOUND ? e->score + ply : e->score;
}

extern const int DEPTH_OFFSET;

INLINE int TTDepth(TTEntry* e) {
  return e->depth + DEPTH_OFFSET;
}

#endif