// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

#include "movegen.h"

const int CASTLE_MAP[4][3] = {
  {WHITE_KS, G1, F1},
  {WHITE_QS, C1, D1},
  {BLACK_KS, G8, F8},
  {BLACK_QS, C8, D8},
};

ScoredMove* AddNoisyMoves(ScoredMove* moves, Board* board) {
  return board->stm == WHITE ? AddPseudoLegalMoves(moves, board, GT_CAPTURE, WHITE) : //
                               AddPseudoLegalMoves(moves, board, GT_CAPTURE, BLACK);
}

ScoredMove* AddQuietMoves(ScoredMove* moves, Board* board) {
  return board->stm == WHITE ? AddPseudoLegalMoves(moves, board, GT_QUIET, WHITE) : //
                               AddPseudoLegalMoves(moves, board, GT_QUIET, BLACK);
}

ScoredMove* AddPerftMoves(ScoredMove* moves, Board* board) {
  return board->stm == WHITE ? AddLegalMoves(moves, board, WHITE) : //
                               AddLegalMoves(moves, board, BLACK);
}
