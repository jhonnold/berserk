// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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
#include "move.h"
#include "nn/accumulator.h"
#include "nn/evaluate.h"
#include "uci.h"
#include "util.h"

const int PC_VALUES[12]   = {100, -100, 318, -318, 348, -348, 554, -554, 1068, -1068, 0, 0};
const int PHASE_VALUES[6] = {0, 3, 3, 5, 10, 0};
const int MAX_PHASE       = 64;

void SetContempt(int* dest, int stm) {
  int contempt = CONTEMPT;

  dest[stm]     = contempt;
  dest[stm ^ 1] = -contempt;
}

// Main evalution method
Score Evaluate(Board* board, ThreadData* thread) {
  if (IsMaterialDraw(board))
    return 0;

  const int lazy = abs(board->matScore) >= EVAL_LAZY_MARGIN;

  Accumulator* acc = board->accumulators;
  for (int c = WHITE; c <= BLACK; c++) {
    int correct = lazy ? acc->lazyCorrect[c] : acc->correct[c];

    if (!correct) {
      if (CanEfficientlyUpdate(acc, c, lazy))
        BackfillUpdates(acc, board, c, lazy);
      else
        RefreshAccumulator(acc, board, c, lazy);
    }
  }

  int score = Propagate(acc, board->stm, lazy);
  // int score = Predict(board);

  // static contempt
  score += thread->contempt[board->stm];

  // scaled based on phase [1, 1.5]
  score = (128 + board->phase) * score / 128;

  return Min(EVAL_UNKNOWN - 1, Max(-EVAL_UNKNOWN + 1, score));
}

void EvaluateTrace(Board* board) {
  for (int lazy = 0; lazy <= 1; lazy++) {
    // The UCI board has no guarantee of accumulator allocation
    // so we have to set that up here.
    board->accumulators = AlignedMalloc(sizeof(Accumulator), 64);
    ResetAccumulator(board->accumulators, board, WHITE, lazy);
    ResetAccumulator(board->accumulators, board, BLACK, lazy);

    int base   = Propagate(board->accumulators, board->stm, lazy);
    base       = board->stm == WHITE ? base : -base;
    int scaled = (128 + board->phase) * base / 128;

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
          ResetAccumulator(board->accumulators, board, WHITE, lazy);
          ResetAccumulator(board->accumulators, board, BLACK, lazy);
          int new = Propagate(board->accumulators, board->stm, lazy);
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

    printf(" NNUE Score: %dcp (white)\n", (int) Normalize(base));
    printf("Final Score: %dcp (white)\n", (int) Normalize(scaled));

    AlignedFree(board->accumulators);
  }
}