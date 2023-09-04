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
#include "move.h"
#include "nn/accumulator.h"
#include "nn/evaluate.h"
#include "search.h"
#include "uci.h"
#include "util.h"

const int PHASE_VALUES[6] = {3, 4, 4, 6, 12, 0};
const int MAX_PHASE       = 128;

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

  Accumulator* acc = board->accumulators;
  for (int c = WHITE; c <= BLACK; c++) {
    if (!acc->correct[c]) {
      if (CanEfficientlyUpdate(acc, c))
        ApplyLazyUpdates(acc, board, c);
      else
        RefreshAccumulator(acc, board, c);
    }
  }

  const int layer = (BitCount(OccBB(BOTH)) - 1) / 4;
  int score       = board->stm == WHITE ? Propagate(acc, WHITE, layer) : Propagate(acc, BLACK, layer);

  // static contempt
  score += thread->contempt[board->stm];

  // scaled based on phase [1, 1.5]
  score = (256 + board->phase) * score / 256;

  return Min(TB_WIN_BOUND - 1, Max(-TB_WIN_BOUND + 1, score));
}

void EvaluateTrace(Board* board) {
  // The UCI board has no guarantee of accumulator allocation
  // so we have to set that up here.
  board->accumulators = AlignedMalloc(sizeof(Accumulator), 64);
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  const int realLayer = (BitCount(OccBB(BOTH)) - 1) / 4;
  int base            = Propagate(board->accumulators, board->stm, realLayer);
  base                = board->stm == WHITE ? base : -base;
  int scaled          = (128 + board->phase) * base / 128;

  printf("\nNNUE derived piece values:\n");

  for (int r = 0; r < 8; r++) {
    printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
    printf("|");
    for (int f = 0; f < 16; f++) {
      if (f == 8)
        printf("\n|");

      int sq = r * 8 + (f > 7 ? f - 8 : f);
      int pc = board->squares[sq];

      if (pc == NO_PIECE) {
        printf("       |");
      } else if (f < 8) {
        printf("   %c   |", PIECE_TO_CHAR[pc]);
      } else if (PieceType(pc) == KING) {
        printf("       |");
      } else {
        // To calculate the piece value, we pop it
        // reset the accumulators and take a diff
        PopBit(OccBB(BOTH), sq);
        ResetAccumulator(board->accumulators, board, WHITE);
        ResetAccumulator(board->accumulators, board, BLACK);
        int new = Propagate(board->accumulators, board->stm, realLayer);
        new     = board->stm == WHITE ? new : -new;
        SetBit(OccBB(BOTH), sq);

        int diff       = base - new;
        int normalized = Normalize(diff);
        int v          = abs(normalized);

        char buffer[6];
        buffer[5] = '\0';
        buffer[0] = diff > 0 ? '+' : diff < 0 ? '-' : ' ';
        if (v >= 1000) {
          buffer[1] = '0' + v / 1000;
          v %= 1000;
          buffer[2] = '0' + v / 100;
          v %= 100;
          buffer[3] = '.';
          buffer[4] = '0' + v / 10;
        } else {
          buffer[1] = '0' + v / 100;
          v %= 100;
          buffer[2] = '.';
          buffer[3] = '0' + v / 10;
          v %= 10;
          buffer[4] = '0' + v;
        }
        printf(" %s |", buffer);
      }
    }

    printf("\n");
  }

  printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n\n");

  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  printf("+------------+------------+\n");
  printf("|   Bucket   | Positional |\n");
  printf("|            |  (Layers)  |\n");
  printf("+------------+------------+\n");

  for (int l = 0; l < N_LAYERS; l++) {
    printf("| %10d |", l);

    int v = Propagate(board->accumulators, board->stm, l);
    v     = Normalize(board->stm == WHITE ? v : -v);

    char buffer[11];
    for (int i = 0; i < 10; i++)
      buffer[i] = ' ';
    buffer[10] = '\0';
    buffer[0]  = v > 0 ? '+' : v < 0 ? '-' : ' ';
    if (v >= 1000) {
      buffer[6] = '0' + v / 1000;
      v %= 1000;
      buffer[7] = '0' + v / 100;
      v %= 100;
      buffer[8] = '.';
      buffer[9] = '0' + v / 10;
    } else {
      buffer[6] = '0' + v / 100;
      v %= 100;
      buffer[7] = '.';
      buffer[8] = '0' + v / 10;
      v %= 10;
      buffer[9] = '0' + v;
    }
    printf(" %s |", buffer);

    if (l == realLayer)
      printf(" <-- this bucket is used");
    printf("\n");
  }
  printf("+------------+------------+\n");

  printf("\n NNUE Score: %dcp (white)\n", (int) Normalize(base));
  printf("Final Score: %dcp (white)\n", (int) Normalize(scaled));

  AlignedFree(board->accumulators);
}