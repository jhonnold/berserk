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


#ifndef NN_DEFS_H
#define NN_DEFS_H

#include "../types.h"

#define N_KING_BUCKETS 16

#define N_FEATURES (N_KING_BUCKETS * 12 * 64)
#define N_PIECE_THREATS 59808
#define N_PAIRS         4560 // 96 pawn ids, one feature per unordered pair
#define N_THREATS       (N_PIECE_THREATS + N_PAIRS)
#define N_HIDDEN   512
// Pairwise multiplication halves each perspective to N_HIDDEN / 2 units, so the
// two of them together are N_HIDDEN wide. See nn/evaluate.c, FT_SHIFT.
#define N_L1       N_HIDDEN
#define N_L2       16
#define N_L3       32
#define N_OUTPUT_BUCKETS 8

enum {
  SUB = 0,
  ADD = 1
};

typedef int16_t acc_t;

#define MAX_DIRTY_THREATS 96
// One step's pair changes per side: at most two pawns leave, each with 17 partners
#define MAX_PAIR_DELTA 48

struct Accumulator {
  uint8_t correct[2];
  uint16_t captured;
  uint16_t nDirty;
  Move move;
  uint32_t dirty[MAX_DIRTY_THREATS];
  BitBoard pawns[2][2]; // [before, after][colour], for the pawn pairs
  acc_t values[2][N_HIDDEN] ALIGN;
};

struct AccumulatorKingState {
  acc_t values[N_HIDDEN] ALIGN;
  BitBoard pcs[12];
};

#endif
