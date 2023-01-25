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

void SetContempt(int* dest, int stm) {
  int contempt = CONTEMPT;

  dest[stm]     = contempt;
  dest[stm ^ 1] = -contempt;
}

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
    BitBoard atx = GetKnightAttacks(PopLSB(&knights));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  BitBoard bishops = PieceBB(BISHOP, stm);
  while (bishops) {
    BitBoard atx = GetBishopAttacks(PopLSB(&bishops), OccBB(BOTH));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  // remove rooks
  opponentPieces ^= PieceBB(ROOK, xstm);

  BitBoard rooks = PieceBB(ROOK, stm);
  while (rooks) {
    BitBoard atx = GetRookAttacks(PopLSB(&rooks), OccBB(BOTH));

    threats->sqs |= atx;
    threats->pcs |= opponentPieces & atx;
  }

  BitBoard queens = PieceBB(QUEEN, stm);
  while (queens)
    threats->sqs |= GetQueenAttacks(PopLSB(&queens), OccBB(BOTH));

  threats->sqs |= GetKingAttacks(LSB(PieceBB(KING, stm)));
}

// Main evalution method
Score Evaluate(Board* board, ThreadData* thread) {
  Score knownEval = EvaluateKnownPositions(board);
  if (knownEval != UNKNOWN)
    return knownEval;

  int score = board->stm == WHITE ?
    Propagate(board->accumulators, WHITE) :
    Propagate(board->accumulators, BLACK);

  // static contempt
  score += thread->contempt[board->stm];

  // scaled based on phase [1, 1.5]
  score = (128 + board->phase) * score / 128;

  return Min(TB_WIN_BOUND - 1, Max(-TB_WIN_BOUND + 1, score));
}