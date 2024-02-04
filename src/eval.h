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

#define EVAL_UNKNOWN     2046
#define EVAL_LAZY_MARGIN 450

INLINE int ClampEval(int eval) {
  return Min(EVAL_UNKNOWN - 1, Max(-EVAL_UNKNOWN + 1, eval));
}

extern const int PC_VALUES[12];
extern const int PHASE_VALUES[6];
extern const int MAX_PHASE;

void SetContempt(int* dest, int stm);
Score Evaluate(Board* board, ThreadData* thread);
void EvaluateTrace(Board* board);

#endif