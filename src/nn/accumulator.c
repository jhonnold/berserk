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

#include "accumulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bits.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../util.h"

void ResetRefreshTable(AccumulatorKingState* refreshTable) {
  for (size_t b = 0; b < 2 * 2 * N_KING_BUCKETS; b++) {
    AccumulatorKingState* state = refreshTable + b;

    memcpy(state->values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
    memset(state->pcs, 0, sizeof(BitBoard) * 12);
  }
}

// Refreshes an accumulator using a diff from the last known board state
// with proper king bucketing

// Resets an accumulator from pieces on the board
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective) {
  Delta delta[1];
  delta->r = delta->a = 0;

  int kingSq = LSB(PieceBB(KING, perspective));

  BitBoard occ = OccBB(BOTH);
  while (occ) {
    int sq                 = PopLSB(&occ);
    int pc                 = board->squares[sq];
    delta->add[delta->a++] = FeatureIdx(pc, sq, kingSq, perspective);
  }

  acc_t* values = dest->values[perspective];
  memcpy(values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
  ApplyDelta(values, values, delta);
  dest->correct[perspective] = 1;
}
