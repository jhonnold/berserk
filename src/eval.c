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

#include <stdio.h>

#include "eval.h"
#include "board.h"
#include "nn.h"
#include "util.h"

const int PHASE_VALUES[6] = { 0, 2, 2, 4, 8, 0 };
const int MAX_PHASE = 48;

// Main evalution method
Score Evaluate(Board* board) {
  if (IsMaterialDraw(board))
    return 0;

  int output = ApplySecondLayer(board->accumulators[board->side][board->ply], board->accumulators[board->xside][board->ply]);
  return (96 + min(MAX_PHASE, board->phase)) * output / 96;
}