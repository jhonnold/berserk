// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#include "search.h"
#include "types.h"
#include "util.h"

#define MEGABYTE    (1024ull * 1024ull)
#define BUCKET_SIZE 3

#define BOUND_MASK (0x3)
#define PV_MASK    (0x4)
#define AGE_MASK   (0xF8)
#define AGE_INC    (0x8)
#define AGE_CYCLE  (255 + AGE_INC)

typedef struct __attribute__((packed)) {
  uint16_t hash;
  uint8_t depth;
  uint8_t agePvBound;
  uint32_t evalAndMove;
  int16_t score;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
  uint16_t padding;
} TTBucket;

typedef struct {
  void* mem;
  TTBucket* buckets;
  uint64_t count;
  uint8_t age;
} TTTable;

typedef int HashBound;

enum {
  BOUND_UNKNOWN = 0,
  BOUND_LOWER   = 1,
  BOUND_UPPER   = 2,
  BOUND_EXACT   = 3
};

extern TTTable TT;

// allocates the table uncleared; clear it with ThreadsClearTT
size_t TTInit(int mb);
void TTClearPart(int idx, int count);
void TTUpdate(void);
INLINE uint64_t TTIdx(Key hash) {
  return ((unsigned __int128) hash * (unsigned __int128) TT.count) >> 64;
}

INLINE void TTPrefetch(Key hash) {
  __builtin_prefetch(&TT.buckets[TTIdx(hash)]);
}
int TTFull(void);

#define HASH_MAX ((int) ((1ull << 40) * sizeof(TTBucket) / MEGABYTE))

INLINE int TTAge(TTEntry* e) {
  return ((AGE_CYCLE + TT.age - e->agePvBound) & AGE_MASK);
}

INLINE Move TTMove(TTEntry* e) {
  // Lower 20 bits for move
  return (e->evalAndMove & 0xfffff);
}

INLINE Score TTEval(TTEntry* e) {
  // Top 12 bits for eval offset by 2048
  return ((e->evalAndMove >> 20) & 0xfff) - 2048;
}

INLINE void TTStoreMove(TTEntry* e, Move move) {
  e->evalAndMove = (e->evalAndMove & 0xfff00000) | move;
}

INLINE void TTStoreEval(TTEntry* e, Score eval) {
  uint32_t ueval = eval + 2048;
  e->evalAndMove = (ueval << 20) | (e->evalAndMove & 0x000fffff);
}

INLINE Score TTScore(TTEntry* e, Ply ply) {
  if (e->score == UNKNOWN)
    return UNKNOWN;

  return e->score >= TB_WIN_BOUND ? e->score - ply : e->score <= -TB_WIN_BOUND ? e->score + ply : e->score;
}

#define DEPTH_OFFSET (-2)

INLINE Depth TTDepth(TTEntry* e) {
  return e->depth + DEPTH_OFFSET;
}

INLINE HashBound TTBound(TTEntry* e) {
  return e->agePvBound & BOUND_MASK; // 2 bottom bits
}

INLINE Bool TTPV(TTEntry* e) {
  return e->agePvBound & PV_MASK; // 3rd to bottom bit
}

INLINE TTEntry* TTProbe(Key hash,
                        Ply ply,
                        Bool* hit,
                        Move* hashMove,
                        Score* ttScore,
                        Score* ttEval,
                        Depth* ttDepth,
                        HashBound* ttBound,
                        Bool* pv) {
  TTEntry* const bucket    = TT.buckets[TTIdx(hash)].entries;
  const uint16_t shortHash = (uint16_t) hash;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    if (bucket[i].hash == shortHash || !bucket[i].depth) {
      *hit = !!bucket[i].depth;

      if (*hit) {
        *hashMove = TTMove(&bucket[i]);
        *ttEval   = TTEval(&bucket[i]);
        *ttScore  = TTScore(&bucket[i], ply);
        *ttDepth  = TTDepth(&bucket[i]);
        *ttBound  = TTBound(&bucket[i]);
        *pv       = *pv || TTPV(&bucket[i]);
      }

      return &bucket[i];
    }
  }

  *hit = 0;

  TTEntry* replace = bucket;
  for (int i = 1; i < BUCKET_SIZE; i++)
    if (replace->depth - TTAge(replace) / 2 > bucket[i].depth - TTAge(bucket + i) / 2)
      replace = &bucket[i];

  return replace;
}

INLINE void
TTPut(TTEntry* tt, Key hash, Depth depth, int16_t score, uint8_t bound, Move move, Ply ply, int16_t eval, Bool pv) {
  uint16_t shortHash = (uint16_t) hash;

  if (score >= TB_WIN_BOUND)
    score += ply;
  else if (score <= -TB_WIN_BOUND)
    score -= ply;

  if (move || shortHash != tt->hash)
    TTStoreMove(tt, move);

  if ((bound == BOUND_EXACT) || shortHash != tt->hash || depth + 4 > TTDepth(tt) || TTAge(tt)) {
    tt->hash       = shortHash;
    tt->score      = score;
    tt->depth      = (uint8_t) (depth - DEPTH_OFFSET);
    tt->agePvBound = (uint8_t) (TT.age | (pv << 2) | bound);
    TTStoreEval(tt, eval);
  }
}

#endif
