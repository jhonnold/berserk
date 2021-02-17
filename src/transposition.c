#include <inttypes.h>
#include <string.h>

#include "search.h"
#include "transposition.h"
#include "types.h"

#define POWER 16
#define BUCKET_SIZE 2

const int TT_EXACT = 2;
const int TT_LOWER = 0;
const int TT_UPPER = 1;

TTValue transpositionEntries[(1 << POWER) * 2 * BUCKET_SIZE];

inline int ttIdx(uint64_t hash) { return (int)((hash) >> (64 - POWER)) << 1; }

inline TTValue createValue(int score, int flag, int depth, Move move) {
  return ((TTValue)score << 32) | ((TTValue)flag << 30) | ((TTValue)depth << 24) | ((TTValue)move);
}

inline int ttFlag(TTValue value) { return (int)((value & 0xC0000000) >> 30); }
inline Move ttMove(TTValue value) { return (Move)(value & 0xFFFFFF); }
inline int ttDepth(TTValue value) { return (int)((value & 0x3F000000) >> 24); }
inline int ttScore(TTValue value, int ply) {
  int score = (int)(value >> 32);

  return score > MATE_BOUND ? score - ply : score < -MATE_BOUND ? score + ply : score;
}

inline TTValue ttProbe(uint64_t hash) {
  int idx = ttIdx(hash);

  for (int i = idx; i < idx + BUCKET_SIZE * 2; i += 2) {
    uint64_t ttHash = transpositionEntries[i];
    if (ttHash == hash)
      return transpositionEntries[i + 1];
  }

  return 0;
}

inline void ttPut(uint64_t hash, int depth, int score, int flag, Move move) {
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

  transpositionEntries[replacementIdx] = hash;
  transpositionEntries[replacementIdx + 1] = createValue(score, flag, depth, move);
}

inline void ttClear() { memset(transpositionEntries, 0ULL, sizeof(transpositionEntries)); }
