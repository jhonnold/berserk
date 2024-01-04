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

#ifndef EVAL_H
#define EVAL_H

#include <stdlib.h>

#include "types.h"
#include "util.h"

#define EVAL_UNKNOWN 2046

INLINE int ClampEval(int eval) {
  return Min(EVAL_UNKNOWN - 1, Max(-EVAL_UNKNOWN + 1, eval));
}

INLINE EvalCacheEntry* ProbeEvalCache(Board* board, ThreadData* thread) {
  EvalCacheEntry* entry = &thread->evalCache[board->zobrist & EVAL_CACHE_MASK];
  return entry->key == board->zobrist ? entry : NULL;
}

INLINE void SaveEvalCache(Board* board, ThreadData* thread, int eval, uint32_t featureKey) {
  EvalCacheEntry* entry = &thread->evalCache[board->zobrist & EVAL_CACHE_MASK];

  entry->key        = board->zobrist;
  entry->eval       = eval;
  entry->featureKey = featureKey;
}

extern const int PHASE_VALUES[6];
extern const int MAX_PHASE;

void SetContempt(int* dest, int stm);
Score Evaluate(Board* board, ThreadData* thread, uint32_t* featureKey);
void EvaluateTrace(Board* board);

#endif