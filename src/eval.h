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

#ifndef EVAL_H
#define EVAL_H

#include <stdlib.h>

#include "types.h"

#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))
#define psqtIdx(sq) ((rank((sq)) << 2) + (file((sq)) > 3 ? file((sq)) ^ 7 : file((sq))))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))

extern EvalCoeffs C;

extern const int MAX_SCALE;
extern const int MAX_PHASE;
extern const Score PHASE_MULTIPLIERS[5];

extern const int STATIC_MATERIAL_VALUE[7];

extern Score PSQT[12][2][64];

void InitPSQT();

int Scale(Board* board, int ss);
Score GetPhase(Board* board);

Score MaterialValue(Board* board, int side);
Score Evaluate(Board* board, ThreadData* thread);
Score EvaluateScaled(Board* board, ThreadData* thread);

#endif