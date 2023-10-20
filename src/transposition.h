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

extern TTTable TT;

size_t TTInit(int mb);
void TTFree();
void TTClearPart(int idx);
void TTClear();
void TTUpdate();
void TTPrefetch(uint64_t hash);
TTEntry* TTProbe(uint64_t hash,
                 int ply,
                 int* hit,
                 Move* hashMove,
                 int* ttScore,
                 int* ttEval,
                 int* ttDepth,
                 int* ttBound,
                 int* pv);
void TTPut(TTEntry* tt,
           uint64_t hash,
           int depth,
           int16_t score,
           uint8_t bound,
           Move move,
           int ply,
           int16_t eval,
           int pv);
int TTFull();

#define HASH_MAX ((int) (pow(2, 32) * sizeof(TTBucket) / MEGABYTE))

INLINE Move TTMove(TTEntry* e) {
  // Lower 20 bits for move
  return (e->evalAndMove & 0xfffff);
}

INLINE int TTEval(TTEntry* e) {
  // Top 12 bits for eval offset by 2048
  return ((e->evalAndMove >> 20) & 0xfff) - 2048;
}

INLINE void TTStoreMove(TTEntry* e, Move move) {
  e->evalAndMove = (e->evalAndMove & 0xfff00000) | move;
}

INLINE void TTStoreEval(TTEntry* e, int eval) {
  uint32_t ueval = eval + 2048;
  e->evalAndMove = (ueval << 20) | (e->evalAndMove & 0x000fffff);
}

INLINE int TTScore(TTEntry* e, int ply) {
  if (e->score == UNKNOWN)
    return UNKNOWN;

  return e->score >= TB_WIN_BOUND ? e->score - ply : e->score <= -TB_WIN_BOUND ? e->score + ply : e->score;
}

extern const int DEPTH_OFFSET;

INLINE int TTDepth(TTEntry* e) {
  return e->depth + DEPTH_OFFSET;
}

INLINE int TTBound(TTEntry* e) {
  return e->agePvBound & BOUND_MASK; // 2 bottom bits
}

INLINE int TTPV(TTEntry* e) {
  return e->agePvBound & PV_MASK; // 3rd to bottom bit
}

#endif