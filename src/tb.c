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
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "pyrrhic/tbprobe.h"
#include "tb.h"


#define vf(bb) __builtin_bswap64((bb))

Move TBRootProbe(Board* board) {
  if (board->castling || bits(board->occupancies[BOTH]) > TB_LARGEST)
    return NULL_MOVE;

  unsigned results[MAX_MOVES]; // used for native support
  unsigned res = tb_probe_root(vf(board->occupancies[WHITE]), vf(board->occupancies[BLACK]),
                               vf(board->pieces[KING_WHITE] | board->pieces[KING_BLACK]),
                               vf(board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK]),
                               vf(board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]),
                               vf(board->pieces[BISHOP_WHITE] | board->pieces[BISHOP_BLACK]),
                               vf(board->pieces[KNIGHT_WHITE] | board->pieces[KNIGHT_BLACK]),
                               vf(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]), board->halfMove,
                               board->epSquare ? (board->epSquare ^ 56) : 0, board->side == WHITE ? 1 : 0, results);

  if (res == TB_RESULT_FAILED || res == TB_RESULT_STALEMATE || res == TB_RESULT_CHECKMATE)
    return NULL_MOVE;

  unsigned start = TB_GET_FROM(res) ^ 56;
  unsigned end = TB_GET_TO(res) ^ 56;
  unsigned ep = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);
  int piece = board->squares[start];
  int capture = !!board->squares[end];

  int promoPieces[5] = {0, QUEEN[board->side], ROOK[board->side], BISHOP[board->side], KNIGHT[board->side]};

  return BuildMove(start, end, piece, promoPieces[promo], capture,
                   PieceType(piece) == PAWN_TYPE && abs((int)start - (int)end) == 2, ep, 0);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->halfMove || bits(board->occupancies[BOTH]) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_wdl(vf(board->occupancies[WHITE]), vf(board->occupancies[BLACK]),
                      vf(board->pieces[KING_WHITE] | board->pieces[KING_BLACK]),
                      vf(board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK]),
                      vf(board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]),
                      vf(board->pieces[BISHOP_WHITE] | board->pieces[BISHOP_BLACK]),
                      vf(board->pieces[KNIGHT_WHITE] | board->pieces[KNIGHT_BLACK]),
                      vf(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]),
                      board->epSquare ? (board->epSquare ^ 56) : 0, board->side == WHITE ? 1 : 0);
}