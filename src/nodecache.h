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

#ifndef NODECACHE_H
#define NODECACHE_H

#include <string.h>

#include "types.h"
#include "util.h"

INLINE void ResetNodeCache(LowPlyNodeCache* cache) {
  memset(cache, 0, sizeof(LowPlyNodeCache));
}

INLINE void UpdateNodeCache(LowPlyNodeCache* cache) {
  cache->generation++;
}

INLINE LowPlyNodeCounter* ProbeNodeCache(LowPlyNodeCache* cache, uint64_t zobrist) {
  const uint32_t key = zobrist & (LOW_PLY_CACHE_ENTRIES - 1);

  // Hit
  if (cache->counts[key].zobrist == zobrist) {
    cache->counts[key].generation = cache->generation;
    return &cache->counts[key];
  }

  // Miss, but overwrite
  else if (cache->counts[key].generation < cache->generation) {
    memset(&cache->counts[key], 0, sizeof(LowPlyNodeCounter));
    cache->counts[key].zobrist    = zobrist;
    cache->counts[key].generation = cache->generation;
    return &cache->counts[key];
  }

  // Miss
  return NULL;
}

INLINE LowPlyMove* GetLowPlyMove(LowPlyNodeCounter* entry, Move move) {
  const int32_t startIdx = (move * 6364136223846793005ULL + 1442695040888963407ULL) & (MAX_MOVES - 1);

  for (int32_t idx = startIdx; idx <= startIdx + MAX_MOVES; idx++) {
    LowPlyMove* moveEntry = &entry->moves[idx & (MAX_MOVES - 1)];
    if (!moveEntry->move)
      break;
    if (moveEntry->move == move)
      return moveEntry;
  }

  return NULL;
}

INLINE void AddLowPlyMoveStats(LowPlyNodeCounter* entry, Move move, uint64_t nodes) {
  const int32_t startIdx = (move * 6364136223846793005ULL + 1442695040888963407ULL) & (MAX_MOVES - 1);

  for (int32_t idx = startIdx; idx <= startIdx + MAX_MOVES; idx++) {
    LowPlyMove* moveEntry = &entry->moves[idx & (MAX_MOVES - 1)];

    if (!moveEntry->move || moveEntry->move == move) {
      moveEntry->move  = move;
      moveEntry->nodes += nodes;
      entry->nodes += nodes;
      return;
    }
  }
}

#endif