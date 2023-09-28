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

#include "eval.h"

#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "nn.h"
#include "search.h"
#include "uci.h"
#include "util.h"

const int PHASE_VALUES[6] = {0, 3, 3, 5, 10, 0};
const int MAX_PHASE = 64;

// "Threats" logic to be utilized in search
// idea originating in Koivisto
BitBoard Threats(Board* board, int stm) {
  int xstm = stm ^ 1;

  BitBoard threatened = 0;
  BitBoard opponentPieces = OccBB(xstm) & ~PieceBB(PAWN, xstm);

  BitBoard pawnAttacks = stm == WHITE ? ShiftNW(PieceBB(PAWN, WHITE)) | ShiftNE(PieceBB(PAWN, WHITE))
                                      : ShiftSW(PieceBB(PAWN, BLACK)) | ShiftSE(PieceBB(PAWN, BLACK));
  threatened |= pawnAttacks & opponentPieces;

  // remove minors
  opponentPieces &= ~(PieceBB(KNIGHT, xstm) | PieceBB(BISHOP, xstm));

  BitBoard knights = PieceBB(KNIGHT, stm);
  while (knights) threatened |= opponentPieces & GetKnightAttacks(popAndGetLsb(&knights));

  BitBoard bishops = PieceBB(BISHOP, stm);
  while (bishops) threatened |= opponentPieces & GetBishopAttacks(popAndGetLsb(&bishops), OccBB(BOTH));

  // remove rooks
  opponentPieces &= ~PieceBB(ROOK, xstm);

  BitBoard rooks = PieceBB(ROOK, stm);
  while (rooks) threatened |= opponentPieces & GetRookAttacks(popAndGetLsb(&rooks), OccBB(BOTH));

  return threatened;
}

// Idea from SF to correct positions not in classical chess
// https://tcec-chess.com/#div=frc4ld&game=27&season=21
Score FRCCorneredBishop(Board* board) {
  static const Score v = 25;

  Score fix = 0;

  if (getBit(PieceBB(BISHOP, WHITE), A1) && getBit(PieceBB(PAWN, WHITE), B2))
    fix -= (3 + !!getBit(OccBB(BOTH), B3)) * v;

  if (getBit(PieceBB(BISHOP, WHITE), H1) && getBit(PieceBB(PAWN, WHITE), G2))
    fix -= (3 + !!getBit(OccBB(BOTH), G3)) * v;

  if (getBit(PieceBB(BISHOP, BLACK), A8) && getBit(PieceBB(PAWN, BLACK), B7))
    fix += (3 + !!getBit(OccBB(BOTH), B6)) * v;

  if (getBit(PieceBB(BISHOP, BLACK), H8) && getBit(PieceBB(PAWN, BLACK), G7))
    fix += (3 + !!getBit(OccBB(BOTH), G6)) * v;

  return board->stm == WHITE ? fix : -fix;
}

// Main evalution method
Score Evaluate(Board* board, ThreadData* thread) {
  SearchData* data = &thread->data;

  if (IsMaterialDraw(board)) return 0;

  int score = OutputLayer(board->accumulators[board->stm][board->ply], board->accumulators[board->xstm][board->ply]);

  if (CHESS_960) score += FRCCorneredBishop(board);

  int scalar = 128 + board->phase;
  int scaled = scalar * score / 128;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, scaled));
}