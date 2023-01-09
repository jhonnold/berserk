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

const int DEPTH_OFFSET = -2;

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
  int count = Threads.count;

  uint64_t size   = TT.count * sizeof(TTBucket);
  uint64_t slice  = (size + count - 1) / count;
  uint64_t blocks = (slice + 2 * MEGABYTE - 1) / (2 * MEGABYTE);
  uint64_t begin  = min(size, idx * blocks * 2 * MEGABYTE);
  uint64_t end    = min(size, begin + blocks * 2 * MEGABYTE);

  memset(TT.buckets + begin / sizeof(TTBucket), 0, end - begin);
}

inline void TTClear() {
  for (int i = 0; i < Threads.count; i++) ThreadWake(Threads.threads[i], THREAD_TT_CLEAR);
  for (int i = 0; i < Threads.count; i++) ThreadWaitUntilSleep(Threads.threads[i]);
}

inline void TTUpdate() {
  TT.age += 0x10;
}

inline uint64_t TTIdx(uint64_t hash) {
  return ((unsigned __int128) hash * (unsigned __int128) TT.count) >> 64;
}

inline void TTPrefetch(uint64_t hash) {
  __builtin_prefetch(&TT.buckets[TTIdx(hash)]);
}

inline TTEntry* TTProbe(uint64_t hash) {
  TTEntry* bucket    = TT.buckets[TTIdx(hash)].entries;
  uint16_t shortHash = (uint16_t) hash;

  for (int i = 0; i < BUCKET_SIZE; i++)
    if (bucket[i].hash == shortHash) {
      bucket[i].flags = TT.age | (bucket[i].flags & 0x0F);
      return &bucket[i];
    }

  return NULL;
}

inline void TTPut(uint64_t hash, int depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval, int pv) {
  TTBucket* bucket   = &TT.buckets[TTIdx(hash)];
  uint16_t shortHash = (uint16_t) hash;
  TTEntry* toReplace = bucket->entries;

  if (score > MATE_BOUND)
    score += ply;
  else if (score < -MATE_BOUND)
    score -= ply;

  if (pv) flag |= TT_PV;

  for (TTEntry* entry = bucket->entries; entry < bucket->entries + BUCKET_SIZE; entry++) {
    if (!entry->depth) { // raw depth of 0 means empty
      toReplace = entry;
      break;
    }

    if (entry->hash == shortHash) {
      if (TTDepth(entry) > depth * 2 && !(flag & TT_EXACT)) return;

      toReplace = entry;
      break;
    }

    int existingReplaceFactor = entry->depth - ((271 + TT.age - entry->flags) & 0xF0) / 4;
    int currentReplaceFactor  = toReplace->depth - ((271 + TT.age - toReplace->flags) & 0xF0) / 4;

    if (existingReplaceFactor < currentReplaceFactor) toReplace = entry;
  }

  toReplace->hash  = shortHash;
  toReplace->depth = depth - DEPTH_OFFSET;
  toReplace->flags = TT.age | flag;
  toReplace->move  = move;
  toReplace->score = score;
  toReplace->eval  = eval;
}

inline int TTFull() {
  int c = 1000 / BUCKET_SIZE;
  int t = 0;

  for (int i = 0; i < c; i++) {
    TTBucket b = TT.buckets[i];
    for (int j = 0; j < BUCKET_SIZE; j++) {
      if (b.entries[j].depth && (b.entries[j].flags & 0xF0) == TT.age) t++;
    }
  }

  return t * 1000 / (c * BUCKET_SIZE);
}