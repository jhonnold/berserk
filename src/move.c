// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#include "move.h"

#include <stdio.h>
#include <string.h>

#include "movegen.h"
#include "movepick.h"
#include "types.h"
#include "uci.h"

const char* PIECE_TO_CHAR = "PpNnBbRrQqKk";

const char* PROMOTION_TO_CHAR = "--nnbbrrqq--";

const int CHAR_TO_PIECE[] = {
  ['P'] = WHITE_PAWN,   //
  ['N'] = WHITE_KNIGHT, //
  ['B'] = WHITE_BISHOP, //
  ['R'] = WHITE_ROOK,   //
  ['Q'] = WHITE_QUEEN,  //
  ['K'] = WHITE_KING,   //
  ['p'] = BLACK_PAWN,   //
  ['n'] = BLACK_KNIGHT, //
  ['b'] = BLACK_BISHOP, //
  ['r'] = BLACK_ROOK,   //
  ['q'] = BLACK_QUEEN,  //
  ['k'] = BLACK_KING,   //
};

const char* SQ_TO_COORD[64] = {
  "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", //
  "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", //
  "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", //
  "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", //
  "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", //
  "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", //
  "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", //
  "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", //
};

const int CASTLING_ROOK[64] = {
  [G1] = 0,
  [C1] = 1,
  [G8] = 2,
  [C8] = 3,
};

const int CASTLE_ROOK_DEST[64] = {
  [G1] = F1,
  [C1] = D1,
  [G8] = F8,
  [C8] = D8,
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

  int from = From(move);
  int to   = To(move);

  if (CHESS_960 && IsCas(move))
    to = board->cr[CASTLING_ROOK[to]];

  if (Promo(move)) {
    sprintf(buffer, "%s%s%c", SQ_TO_COORD[from], SQ_TO_COORD[to], PROMOTION_TO_CHAR[Promo(move)]);
  } else {
    sprintf(buffer, "%s%s", SQ_TO_COORD[from], SQ_TO_COORD[to]);
  }

  return buffer;
}

inline int IsRecapture(SearchStack* ss, Move move) {
  return IsCap(move) &&                                                //
         ((IsCap((ss - 1)->move) && To((ss - 1)->move) == To(move)) || //
          (IsCap((ss - 3)->move) && To((ss - 3)->move) == To(move)));
}
