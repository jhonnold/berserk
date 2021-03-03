#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "transposition.h"
#include "types.h"

const TTValue NO_ENTRY = 0ULL;
TTValue* TRANSPOSITION_ENTRIES = NULL;
size_t SIZE = 0;
int POWER = 0;

void ttInit(int mb) {
  POWER = (int)log2(0x100000 / sizeof(TTValue)) + (int)log2(mb) - (int)log2(BUCKET_SIZE);

  if (TRANSPOSITION_ENTRIES != NULL)
    free(TRANSPOSITION_ENTRIES);

  SIZE = (1 << POWER) * sizeof(TTValue) * BUCKET_SIZE;
  TRANSPOSITION_ENTRIES = malloc(SIZE);

  ttClear();
}

void ttFree() { free(TRANSPOSITION_ENTRIES); }

inline void ttClear() { memset(TRANSPOSITION_ENTRIES, NO_ENTRY, SIZE); }

inline int ttScore(TTValue value, int ply) {
  int score = (int)(value >> 32);

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline TTValue ttProbe(uint64_t hash) {
  int idx = ttIdx(hash);

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2)
    if (TRANSPOSITION_ENTRIES[i] == hash)
      return TRANSPOSITION_ENTRIES[i + 1];

  return NO_ENTRY;
}

inline void ttPut(uint64_t hash, int depth, int score, int flag, Move move, int ply) {
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
        return;

      replacementIdx = i;
      break;
    }

    if (currDepth < replacementDepth) {
      replacementIdx = i;
      replacementDepth = currDepth;
    }
  }

  if (score > MATE_BOUND)
    score += ply;
  else if (score < -MATE_BOUND)
    score -= ply;

  TRANSPOSITION_ENTRIES[replacementIdx] = hash;
  TRANSPOSITION_ENTRIES[replacementIdx + 1] = ttEntry(score, flag, depth, move);
}
