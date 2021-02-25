#include <inttypes.h>
#include <string.h>

#include "search.h"
#include "transposition.h"
#include "types.h"

const TTValue NO_ENTRY = 0ULL;
TTValue transpositionEntries[(1 << POWER) * 2 * BUCKET_SIZE];

inline void ttClear() { memset(transpositionEntries, NO_ENTRY, sizeof(transpositionEntries)); }

inline int ttScore(TTValue value, int ply) {
  int score = (int)(value >> 32);

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline TTValue ttProbe(uint64_t hash) {
  int idx = ttIdx(hash);

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2)
    if (transpositionEntries[i] == hash)
      return transpositionEntries[i + 1];

  return NO_ENTRY;
}

inline void ttPut(uint64_t hash, int depth, int score, int flag, Move move, int ply) {
  int idx = ttIdx(hash);
  int replacementDepth = INT32_MAX;
  int replacementIdx = idx;

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2) {
    uint64_t currHash = transpositionEntries[i];
    if (currHash == 0) {
      replacementIdx = i;
      break;
    }

    int currDepth = ttDepth(transpositionEntries[i + 1]);
    if (transpositionEntries[i] == hash) {
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

  transpositionEntries[replacementIdx] = hash;
  transpositionEntries[replacementIdx + 1] = ttEntry(score, flag, depth, move);
}
