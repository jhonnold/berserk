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

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#include "bits.h"
#include "search.h"
#include "transposition.h"
#include "types.h"

// Global TT
TTTable TT = {0};

size_t TTInit(int mb) {
  if (TT.mask)
    TTFree();

  uint64_t keySize = (uint64_t)log2(mb) + (uint64_t)log2(MEGABYTE / sizeof(TTBucket));

#if defined(__linux__) && !defined(__ANDROID__)
  // On Linux systems we align on 2MB boundaries and request Huge Pages
  TT.buckets = aligned_alloc(2 * MEGABYTE, (1ULL << keySize) * sizeof(TTBucket));
  madvise(TT.buckets, (1ULL << keySize) * sizeof(TTBucket), MADV_HUGEPAGE);
#else
  TT.buckets = calloc((1ULL << keySize), sizeof(TTBucket));
#endif

  TT.mask = (1ULL << keySize) - 1ULL;

  TTClear();
  return (TT.mask + 1ULL) * sizeof(TTBucket);
}

void TTFree() { free(TT.buckets); }

inline void TTClear() { memset(TT.buckets, 0, (TT.mask + 1ULL) * sizeof(TTBucket)); }

inline int TTScore(TTEntry* e, int ply) {
  return e->score > MATE_BOUND ? e->score - ply : e->score < -MATE_BOUND ? e->score + ply : e->score;
}

inline void TTPrefetch(uint64_t hash) { __builtin_prefetch(&TT.buckets[TT.mask & hash]); }

inline TTEntry* TTProbe(int* hit, uint64_t hash) {
#ifndef TUNE
  TTBucket* bucket = &TT.buckets[TT.mask & hash];
  uint32_t shortHash = hash >> 32;

  for (int i = 0; i < BUCKET_SIZE; i++)
    if (bucket->entries[i].hash == shortHash) {
      *hit = 1;
      return &bucket->entries[i];
    }
#endif

  return 0;
}

inline void TTPut(uint64_t hash, uint8_t depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval) {
#ifdef TUNE
  return;
#else

  TTBucket* bucket = &TT.buckets[TT.mask & hash];
  uint32_t shortHash = hash >> 32;

  int replacementDepth = INT32_MAX;
  int replacementIdx = 0;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    TTEntry* entry = &bucket->entries[i];
    if (!entry->hash) {
      replacementIdx = i;
      break;
    }

    if (entry->hash == shortHash) {
      if (entry->depth >= depth * 2 && !(flag & TT_EXACT))
        return;

      replacementIdx = i;
      break;
    }

    if (entry->depth < replacementDepth) {
      replacementIdx = i;
      replacementDepth = entry->depth;
    }
  }

  int adjustedScore = score;
  if (score > MATE_BOUND)
    adjustedScore += ply;
  else if (score < -MATE_BOUND)
    adjustedScore -= ply;

  TTEntry* entry = &bucket->entries[replacementIdx];
  *entry = (TTEntry){.flags = flag, .depth = depth, .eval = eval, .score = score, .hash = shortHash, .move = move};
#endif
}

inline int TTFull() {
  int c = 1000 / BUCKET_SIZE;
  int t = 0;

  for (int i = 0; i < c; i++) {
    TTBucket b = TT.buckets[i];
    for (int j = 0; j < BUCKET_SIZE; j++) {
      if (b.entries[j].hash)
        t++;
    }
  }

  return t * 1000 / (c * BUCKET_SIZE);
}