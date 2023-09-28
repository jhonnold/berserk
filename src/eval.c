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

#include "eval.h"

#include <stdio.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "nn.h"
#include "search.h"
#include "uci.h"
#include "util.h"

const int PHASE_VALUES[6] = {0, 3, 3, 5, 10, 0};
const int MAX_PHASE       = 64;

// "Threats" logic to be utilized in search
// idea originating in Koivisto
void Threats(Threat* threats, Board* board, int stm) {
  int xstm     = stm ^ 1;
  threats->pcs = threats->sqs = 0;

  BitBoard opponentPieces = OccBB(xstm) ^ PieceBB(PAWN, xstm);

  BitBoard pawnAttacks = stm == WHITE ? ShiftNW(PieceBB(PAWN, WHITE)) | ShiftNE(PieceBB(PAWN, WHITE)) :
                                        ShiftSW(PieceBB(PAWN, BLACK)) | ShiftSE(PieceBB(PAWN, BLACK));
  threats->sqs |= pawnAttacks;
  threats->pcs |= pawnAttacks & opponentPieces;

  // remove minors
  opponentPieces ^= PieceBB(KNIGHT, xstm) | PieceBB(BISHOP, xstm);

  BitBoard knights = PieceBB(KNIGHT, stm);
  while (knights) {
    BitBoard atx = GetKnightAttacks(popAndGetLsb(&knights));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  BitBoard bishops = PieceBB(BISHOP, stm);
  while (bishops) {
    BitBoard atx = GetBishopAttacks(popAndGetLsb(&bishops), OccBB(BOTH));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  // remove rooks
  opponentPieces ^= PieceBB(ROOK, xstm);

  BitBoard rooks = PieceBB(ROOK, stm);
  while (rooks) {
    BitBoard atx = GetRookAttacks(popAndGetLsb(&rooks), OccBB(BOTH));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  BitBoard queens = PieceBB(QUEEN, stm);
  while (queens) threats->sqs |= GetQueenAttacks(popAndGetLsb(&queens), OccBB(BOTH));

  threats->sqs |= GetKingAttacks(lsb(PieceBB(KING, stm)));
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
  Score knownEval = EvaluateKnownPositions(board);
  if (knownEval != UNKNOWN) return knownEval;

  int16_t* stm     = board->accumulators[board->stm][board->acc];
  int16_t* xstm    = board->accumulators[board->xstm][board->acc];

  int score = OutputLayer(stm, xstm);

  // sf cornered bishop in FRC
  if (CHESS_960) score += FRCCorneredBishop(board);

  // scaled based on phase [1, 1.5]
  score = (128 + board->phase) * score / 128;

  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}