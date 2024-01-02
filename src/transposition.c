// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#include "bits.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"

const int DEPTH_OFFSET = -2;

// Global TT
TTTable TT = {0};

size_t TTInit(int mb) {
  if (TT.mem)
    TTFree();

  uint64_t size = (uint64_t) mb * MEGABYTE;

#if defined(__linux__)
  const size_t alignment = 2 * MEGABYTE;
#else
  const size_t alignment = 4096;
#endif

  TT.mem = AlignedMalloc(size, alignment);

#if defined(MADV_HUGEPAGE)
  madvise(TT.mem, size, MADV_HUGEPAGE);
#endif

  TT.buckets = (TTBucket*) TT.mem;
  TT.count   = size / sizeof(TTBucket);

  TTClear();
  return size;
}

void TTFree() {
  AlignedFree(TT.mem);
}

void TTClearPart(int idx) {
  int count = Threads.count;

  const uint64_t size   = TT.count * sizeof(TTBucket);
  const uint64_t slice  = (size + count - 1) / count;
  const uint64_t blocks = (slice + 2 * MEGABYTE - 1) / (2 * MEGABYTE);
  const uint64_t begin  = Min(size, idx * blocks * 2 * MEGABYTE);
  const uint64_t end    = Min(size, begin + blocks * 2 * MEGABYTE);

  memset(TT.buckets + begin / sizeof(TTBucket), 0, end - begin);
}

inline void TTClear() {
  for (int i = 0; i < Threads.count; i++)
    ThreadWake(Threads.threads[i], THREAD_TT_CLEAR);
  for (int i = 0; i < Threads.count; i++)
    ThreadWaitUntilSleep(Threads.threads[i]);
}

inline void TTUpdate() {
  TT.age += AGE_INC;
}

inline uint64_t TTIdx(uint64_t hash) {
  return ((unsigned __int128) hash * (unsigned __int128) TT.count) >> 64;
}

inline void TTPrefetch(uint64_t hash) {
  __builtin_prefetch(&TT.buckets[TTIdx(hash)]);
}

inline TTEntry* TTProbe(uint64_t hash,
                        int ply,
                        int* hit,
                        Move* hashMove,
                        int* ttScore,
                        int* ttEval,
                        int* ttDepth,
                        int* ttBound,
                        int* pv) {
  TTEntry* const bucket    = TT.buckets[TTIdx(hash)].entries;
  const uint16_t shortHash = (uint16_t) hash;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    if (bucket[i].hash == shortHash || !bucket[i].depth) {
      bucket[i].agePvBound = (uint8_t) (TT.age | (bucket[i].agePvBound & (PV_MASK | BOUND_MASK)));
      *hit                 = !!bucket[i].depth;

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
    if (replace->depth - ((AGE_CYCLE + TT.age - replace->agePvBound) & AGE_MASK) / 2 >
        bucket[i].depth - ((AGE_CYCLE + TT.age - bucket[i].agePvBound) & AGE_MASK) / 2)
      replace = &bucket[i];

  return replace;
}

inline void
TTPut(TTEntry* tt, uint64_t hash, int depth, int16_t score, uint8_t bound, Move move, int ply, int16_t eval, int pv) {
  uint16_t shortHash = (uint16_t) hash;

  if (score >= TB_WIN_BOUND)
    score += ply;
  else if (score <= -TB_WIN_BOUND)
    score -= ply;

  if (move || shortHash != tt->hash)
    TTStoreMove(tt, move);

  if ((bound == BOUND_EXACT) || shortHash != tt->hash || depth + 4 > TTDepth(tt)) {
    tt->hash       = shortHash;
    tt->score      = score;
    tt->depth      = (uint8_t) (depth - DEPTH_OFFSET);
    tt->agePvBound = (uint8_t) (TT.age | (pv << 2) | bound);
    TTStoreEval(tt, eval);
  }
}

int TTFull() {
  int c = 0;

  for (int i = 0; i < 1000; i++)
    for (int j = 0; j < BUCKET_SIZE; j++)
      c += TT.buckets[i].entries[j].depth && (TT.buckets[i].entries[j].agePvBound & AGE_MASK) == TT.age;

  return c / BUCKET_SIZE;
}
