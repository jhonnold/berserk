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

inline int TTScore(TTValue value, int ply) {
  int score = (int)((int16_t)((value & 0x0000FFFF00000000) >> 32));

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline void TTPrefetch(uint64_t hash) { __builtin_prefetch(&TT.buckets[TT.mask & hash]); }

inline TTValue TTProbe(uint64_t hash) {
#ifdef TUNE
  return NO_ENTRY;
#else
  TTBucket bucket = TT.buckets[TT.mask & hash];

  for (int i = 0; i < BUCKET_SIZE; i++)
    if (bucket.entries[i].hash == hash)
      return bucket.entries[i].value;

  return NO_ENTRY;
#endif
}

inline TTValue TTPut(uint64_t hash, int depth, int score, int flag, Move move, int ply, int eval) {
#ifdef TUNE
  return NO_ENTRY;
#else

  TTBucket* bucket = &TT.buckets[TT.mask & hash];
  int replacementDepth = INT32_MAX;
  int replacementIdx = 0;

  for (int i = 0; i < BUCKET_SIZE; i++) {
    TTEntry entry = bucket->entries[i];
    if (!entry.hash) {
      replacementIdx = i;
      break;
    }

    int currDepth = TTDepth(entry.value);
    if (entry.hash == hash) {
      if (currDepth > depth && flag != TT_EXACT)
        return entry.value;

      replacementIdx = i;
      break;
    }

    if (currDepth < replacementDepth) {
      replacementIdx = i;
      replacementDepth = currDepth;
    }
  }

  int adjustedScore = score;
  if (score > MATE_BOUND)
    adjustedScore += ply;
  else if (score < -MATE_BOUND)
    adjustedScore -= ply;

  bucket->entries[replacementIdx].hash = hash;
  TTValue tt = bucket->entries[replacementIdx].value = TTEntry(adjustedScore, flag, depth, move, eval);

  return tt;
#endif
}
