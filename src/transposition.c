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

#include "bits.h"
#include "search.h"
#include "transposition.h"
#include "types.h"

// Berserk's transposition table is just a giant array of longs
// Even indicies store the key, odd indicies story the entry
// For collisions, conflicting hashes have buckets of size 2

// Global TT
TTValue* TRANSPOSITION_ENTRIES = NULL;
size_t SIZE = 0;
int POWER = 0;

const TTValue NO_ENTRY = 0ULL;

size_t TTInit(int mb) {
  POWER = (int)log2(MEGABYTE / sizeof(TTValue)) + (int)log2(mb) - (int)log2(BUCKET_SIZE);

  if (TRANSPOSITION_ENTRIES != NULL)
    free(TRANSPOSITION_ENTRIES);

  SIZE = (1ULL << POWER) * sizeof(TTValue) * BUCKET_SIZE;
  TRANSPOSITION_ENTRIES = (TTValue*) malloc(SIZE);

  TTClear();

  return SIZE;
}

void TTFree() { free(TRANSPOSITION_ENTRIES); }

inline void TTClear() { memset(TRANSPOSITION_ENTRIES, 0, SIZE); }

inline int TTScore(TTValue value, int ply) {
  int score = (int)((int16_t)((value & 0x0000FFFF00000000) >> 32));

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline void TTPrefetch(uint64_t hash) { __builtin_prefetch(&TRANSPOSITION_ENTRIES[TTIdx(hash)]); }

inline TTValue TTProbe(uint64_t hash) {
#ifdef TUNE
  return NO_ENTRY;
#else
  uint64_t idx = TTIdx(hash);

  for (uint64_t i = idx; i < idx + BUCKET_SIZE * 2; i += 2)
    if (TRANSPOSITION_ENTRIES[i] == hash)
      return TRANSPOSITION_ENTRIES[i + 1];

  return NO_ENTRY;
#endif
}

inline TTValue TTPut(uint64_t hash, int depth, int score, int flag, Move move, int ply, int eval) {
#ifdef TUNE
  return NO_ENTRY;
#else

  uint64_t idx = TTIdx(hash);
  int replacementDepth = INT32_MAX;
  uint64_t replacementIdx = idx;

  for (uint64_t i = idx; i < idx + BUCKET_SIZE * 2; i += 2) {
    uint64_t currHash = TRANSPOSITION_ENTRIES[i];
    if (currHash == 0) {
      replacementIdx = i;
      break;
    }

    int currDepth = TTDepth(TRANSPOSITION_ENTRIES[i + 1]);
    if (TRANSPOSITION_ENTRIES[i] == hash) {
      if (currDepth > depth && flag != TT_EXACT)
        return TRANSPOSITION_ENTRIES[i + 1];

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

  assert(adjustedScore <= INT16_MAX);
  assert(adjustedScore >= INT16_MIN);
  assert(eval <= INT16_MAX);
  assert(eval >= INT16_MIN);

  TRANSPOSITION_ENTRIES[replacementIdx] = hash;
  TTValue tt = TRANSPOSITION_ENTRIES[replacementIdx + 1] = TTEntry(adjustedScore, flag, depth, move, eval);

  assert(depth == TTDepth(tt));
  assert(score == TTScore(tt, ply));
  assert(flag == TTFlag(tt));
  assert(move == TTMove(tt));
  assert(eval == TTEval(tt));

  return tt;
#endif
}
