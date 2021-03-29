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

const TTValue NO_ENTRY = 0ULL;
TTValue* TRANSPOSITION_ENTRIES = NULL;
size_t SIZE = 0;
int POWER = 0;

void ttInit(int mb) {
  POWER = (int)log2(0x100000 / sizeof(TTValue)) + (int)log2(mb) - (int)log2(BUCKET_SIZE) - 1;

  if (TRANSPOSITION_ENTRIES != NULL)
    free(TRANSPOSITION_ENTRIES);

  SIZE = (1 << POWER) * sizeof(TTValue) * BUCKET_SIZE * 2;
  TRANSPOSITION_ENTRIES = malloc(SIZE);

  ttClear();
}

void ttFree() { free(TRANSPOSITION_ENTRIES); }

inline void ttClear() { memset(TRANSPOSITION_ENTRIES, NO_ENTRY, SIZE); }

inline int ttScore(TTValue value, int ply) {
  int score = (int)((int16_t)((value & 0x0000FFFF00000000) >> 32));

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline TTValue ttProbe(uint64_t hash) {
#ifdef TUNE
  return NO_ENTRY;
#else
  int idx = ttIdx(hash);

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2)
    if (TRANSPOSITION_ENTRIES[i] == hash)
      return TRANSPOSITION_ENTRIES[i + 1];

  return NO_ENTRY;
#endif
}

inline TTValue ttPut(uint64_t hash, int depth, int score, int flag, Move move, int ply, int eval) {
#ifdef TUNE
  return NO_ENTRY;
#else

  int idx = ttIdx(hash);
  int replacementDepth = INT32_MAX;
  int replacementIdx = idx;

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2) {
    uint64_t currHash = TRANSPOSITION_ENTRIES[i];
    if (currHash == 0) {
      replacementIdx = i;
      break;
    }

    int currDepth = ttDepth(TRANSPOSITION_ENTRIES[i + 1]);
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
  TTValue tt = TRANSPOSITION_ENTRIES[replacementIdx + 1] = ttEntry(adjustedScore, flag, depth, move, eval);

  assert(depth == ttDepth(tt));
  assert(score == ttScore(tt, ply));
  assert(flag == ttFlag(tt));
  assert(move == ttMove(tt));
  assert(eval == ttEval(tt));

  return tt;
#endif
}
