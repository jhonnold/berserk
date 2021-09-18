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
#include <string.h>

#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "types.h"
#include "uci.h"

const int CHAR_TO_PIECE[] = {['P'] = 0, ['N'] = 2, ['B'] = 4, ['R'] = 6, ['Q'] = 8, ['K'] = 10,
                             ['p'] = 1, ['n'] = 3, ['b'] = 5, ['r'] = 7, ['q'] = 9, ['k'] = 11};
const char* PIECE_TO_CHAR = "PpNnBbRrQqKk";
const char* PROMOTION_TO_CHAR = "ppnnbbrrqqkk";
const char* SQ_TO_COORD[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

Move ParseMove(char* moveStr, Board* board) {
  SimpleMoveList rootMoves;
  RootMoves(&rootMoves, board);

  for (int i = 0; i < rootMoves.count; i++)
    if (!strcmp(MoveToStr(rootMoves.moves[i], board), moveStr))
      return rootMoves.moves[i];

  return NULL_MOVE;
}

char* MoveToStr(Move move, Board* board) {
  static char buffer[6];

  int start = MoveStart(move);
  int end = MoveEnd(move);

  if (CHESS_960 && MoveCastle(move)) {
    switch (end) {
    case G1:
      end = board->castleRooks[0];
      break;
    case C1:
      end = board->castleRooks[1];
      break;
    case G8:
      end = board->castleRooks[2];
      break;
    case C8:
      end = board->castleRooks[3];
      break;
    default:
      break;
    }
  }

  if (MovePromo(move)) {
    sprintf(buffer, "%s%s%c", SQ_TO_COORD[start], SQ_TO_COORD[end], PROMOTION_TO_CHAR[MovePromo(move)]);
  } else {
    sprintf(buffer, "%s%s", SQ_TO_COORD[start], SQ_TO_COORD[end]);
  }

  return buffer;
}

inline int IsRecapture(SearchData* data, Move move) {
  Move parent = data->moves[data->ply - 1];

  return !(MoveCapture(parent) ^ MoveCapture(move)) && MoveEnd(parent) == MoveEnd(move);
}