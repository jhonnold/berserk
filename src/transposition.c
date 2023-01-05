// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

// Global TT
TTTable TT = {0};

size_t TTInit(int mb) {
  if (TT.mem) TTFree();

  uint64_t size = (uint64_t) mb * MEGABYTE;

#if defined(__linux__)
  // On Linux systems we align on 2MB boundaries and request Huge Pages
  TT.mem     = aligned_alloc(2 * MEGABYTE, size);
  TT.buckets = (TTBucket*) TT.mem;
  madvise(TT.buckets, size, MADV_HUGEPAGE);
#else
  TT.mem     = AlignedMalloc(size);
  TT.buckets = (TTBucket*) TT.mem;
#endif

  TT.count = size / sizeof(TTBucket);

  TTClear();
  return size;
}

void TTFree() {
#if defined(__linux__)
  free(TT.mem);
#else
  AlignedFree(TT.mem);
#endif
}

void TTClearPart(int idx) {
  int count = threadPool.count;

  uint64_t size   = TT.count * sizeof(TTBucket);
  uint64_t slice  = (size + count - 1) / count;
  uint64_t blocks = (slice + 2 * MEGABYTE - 1) / (2 * MEGABYTE);
  uint64_t begin  = min(size, idx * blocks * 2 * MEGABYTE);
  uint64_t end    = min(size, begin + blocks * 2 * MEGABYTE);

  memset(TT.buckets + begin / sizeof(TTBucket), 0, end - begin);
}

inline void TTClear() {
  for (int i = 0; i < threadPool.count; i++) ThreadWake(threadPool.threads[i], THREAD_TT_CLEAR);
  for (int i = 0; i < threadPool.count; i++) ThreadWaitUntilSleep(threadPool.threads[i]);
}

inline void TTUpdate() {
  TT.age += 1;
}

inline int TTScore(TTEntry* e, int ply) {
  if (e->score == UNKNOWN) return UNKNOWN;

  return e->score > MATE_BOUND ? e->score - ply : e->score < -MATE_BOUND ? e->score + ply : e->score;
}

inline uint32_t TTIdx(uint64_t hash) {
  return ((uint32_t) hash * (uint64_t) TT.count) >> 32;
}

inline void TTPrefetch(uint64_t hash) {
  __builtin_prefetch(&TT.buckets[TTIdx(hash)]);
}

inline TTEntry* TTProbe(uint64_t hash) {
  TTEntry* bucket    = TT.buckets[TTIdx(hash)].entries;
  uint32_t shortHash = hash >> 32;

  for (int i = 0; i < BUCKET_SIZE; i++)
    if (bucket[i].hash == shortHash) {
      bucket[i].age = TT.age;
      return &bucket[i];
    }

  return NULL;
}

inline void TTPut(uint64_t hash, int8_t depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval, int pv) {
  TTBucket* bucket   = &TT.buckets[TTIdx(hash)];
  uint32_t shortHash = hash >> 32;
  TTEntry* toReplace = bucket->entries;

  if (score > MATE_BOUND)
    score += ply;
  else if (score < -MATE_BOUND)
    score -= ply;

  if (pv) flag |= TT_PV;

  for (TTEntry* entry = bucket->entries; entry < bucket->entries + BUCKET_SIZE; entry++) {
    if (!entry->hash) {
      toReplace = entry;
      break;
    }

    if (entry->hash == shortHash) {
      if (entry->depth > depth * 2 && !(flag & TT_EXACT)) return;

      toReplace = entry;
      break;
    }

    if (entry->depth - (256 + TT.age - entry->age) * 4 < toReplace->depth - (256 + TT.age - toReplace->age) * 4)
      toReplace = entry;
  }

  *toReplace = (TTEntry) {.flags = flag,
                          .depth = depth,
                          .eval  = eval,
                          .score = score,
                          .hash  = shortHash,
                          .move  = move,
                          .age   = TT.age};
}

inline int TTFull() {
  int c = 1000 / BUCKET_SIZE;
  int t = 0;

  for (int i = 0; i < c; i++) {
    TTBucket b = TT.buckets[i];
    for (int j = 0; j < BUCKET_SIZE; j++) {
      if (b.entries[j].hash && b.entries[j].age == TT.age) t++;
    }
  }

  return t * 1000 / (c * BUCKET_SIZE);
}