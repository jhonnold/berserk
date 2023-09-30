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
#define BUCKET_SIZE 2

#define BOUND_MASK (0x3)
#define PV_MASK    (0x4)
#define AGE_MASK   (0xF8)
#define AGE_INC    (0x8)
#define AGE_CYCLE  (255 + AGE_INC)

typedef struct {
  uint32_t hash;
  uint8_t depth, agePvBound;
  Move move;
  int16_t eval, score;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
} TTBucket;

typedef struct {
  void* mem;
  TTBucket* buckets;
  uint64_t count;
  uint8_t age;
} TTTable;

enum {
  BOUND_UNKNOWN = 0,
  BOUND_LOWER   = 1,
  BOUND_UPPER   = 2,
  BOUND_EXACT   = 3
};

extern TTTable TT;

size_t TTInit(int mb);
void TTFree();
void TTClearPart(int idx);
void TTClear();
void TTUpdate();
void TTPrefetch(uint64_t hash);
TTEntry* TTProbe(uint64_t hash, int* hit);
int TTFull();

extern const int DEPTH_OFFSET;

#define HASH_MAX ((int) (pow(2, 32) * sizeof(TTBucket) / MEGABYTE))

INLINE int TTScore(TTEntry* e, int ply) {
  if (e->score == UNKNOWN)
    return UNKNOWN;

  return e->score >= TB_WIN_BOUND ? e->score - ply : e->score <= -TB_WIN_BOUND ? e->score + ply : e->score;
}

INLINE int TTDepth(TTEntry* e) {
  return e->depth + DEPTH_OFFSET;
}

INLINE int TTBound(TTEntry* e) {
  return e->agePvBound & BOUND_MASK; // 2 bottom bits
}

INLINE int TTPV(TTEntry* e) {
  return e->agePvBound & PV_MASK; // 3rd to bottom bit
}

INLINE void
TTPut(TTEntry* tt, uint64_t hash, int depth, int16_t score, uint8_t bound, Move move, int ply, int16_t eval, int pv) {
  uint32_t shortHash = (uint32_t) hash;

  if (score >= TB_WIN_BOUND)
    score += ply;
  else if (score <= -TB_WIN_BOUND)
    score -= ply;

  if (move || shortHash != tt->hash)
    tt->move = move;

  // 6 is really 4 (but accounting for 2 in DEPTH OFFSET)
  if ((bound == BOUND_EXACT) || shortHash != tt->hash || depth + 6 > tt->depth) {
    tt->hash       = shortHash;
    tt->eval       = eval;
    tt->score      = score;
    tt->depth      = (uint8_t) (depth - DEPTH_OFFSET);
    tt->agePvBound = (uint8_t) (TT.age | (pv << 2) | bound);
  }
}

#endif