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

#include "tb.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "pyrrhic/tbprobe.h"

#define ByteSwap(bb) __builtin_bswap64((bb))

void TBRootMoves(SimpleMoveList* moves, Board* board) {
  moves->count = 0;

  unsigned results[MAX_MOVES];
  unsigned result = TBRootProbe(board, results);

  if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE)
    return;

  const unsigned wdl = TB_GET_WDL(result);

  for (int i = 0; i < MAX_MOVES && results[i] != TB_RESULT_FAILED; i++)
    if (wdl == TB_GET_WDL(results[i]))
      moves->moves[moves->count++] = TBMoveFromResult(results[i], board);
}

unsigned TBRootProbe(Board* board, unsigned* results) {
  if (board->castling || BitCount(OccBB(BOTH)) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_root(ByteSwap(OccBB(WHITE)),
                       ByteSwap(OccBB(BLACK)),
                       ByteSwap(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                       ByteSwap(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                       ByteSwap(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                       ByteSwap(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                       ByteSwap(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                       ByteSwap(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                       board->fmr,
                       board->epSquare ? (board->epSquare ^ 56) : 0,
                       board->stm == WHITE ? 1 : 0,
                       results);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->fmr || BitCount(OccBB(BOTH)) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_wdl(ByteSwap(OccBB(WHITE)),
                      ByteSwap(OccBB(BLACK)),
                      ByteSwap(PieceBB(KING, WHITE) | PieceBB(KING, BLACK)),
                      ByteSwap(PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)),
                      ByteSwap(PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK)),
                      ByteSwap(PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)),
                      ByteSwap(PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK)),
                      ByteSwap(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK)),
                      board->epSquare ? (board->epSquare ^ 56) : 0,
                      board->stm == WHITE ? 1 : 0);
}