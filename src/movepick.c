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

#include "movepick.h"

#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "see.h"
#include "transposition.h"
#include "types.h"

const int MATERIAL_VALUES[7] = {100, 325, 325, 550, 1100, 0, 0};

void InitAllMoves(MovePicker* picker, Move hashMove, ThreadData* thread, SearchStack* ss, BitBoard threats) {
  picker->type  = MP_ALL;
  picker->phase = HASH_MOVE;

  picker->hashMove = hashMove;
  picker->killer1  = ss->killers[0];
  picker->killer2  = ss->killers[1];
  picker->counter  = thread->counters[Moving((ss - 1)->move)][To((ss - 1)->move)];

  picker->threats = threats;
  picker->thread  = thread;
  picker->ss      = ss;
}

void InitNoisyMoves(MovePicker* picker, ThreadData* thread, int probcut) {
  picker->type  = probcut ? MP_PC : MP_QS;
  picker->phase = GEN_NOISY_MOVES;

  picker->hashMove = NULL_MOVE;
  picker->killer1  = NULL_MOVE;
  picker->killer2  = NULL_MOVE;
  picker->counter  = NULL_MOVE;

  picker->threats = 0;
  picker->thread  = thread;
}

void InitPerftMoves(MovePicker* picker, Board* board) {
  picker->type    = MP_PERFT;
  picker->phase   = PERFT_MOVES;
  picker->current = picker->moves;

  picker->end = AddPerftMoves(picker->moves, board);
}

Move Best(ScoredMove* current, ScoredMove* end) {
  ScoredMove* orig = current;
  ScoredMove* max  = current;

  while (++current < end)
    if (current->score > max->score)
      max = current;

  ScoredMove temp = *orig;
  *orig           = *max;
  *max            = temp;

  return orig->move;
}

void ScoreMoves(MovePicker* picker, Board* board, const int type) {
  ScoredMove* current = picker->current;
  while (current < picker->end) {
    Move move = current->move;

    if (type == QUIET)
      current->score = GetQuietHistory(picker->ss, picker->thread, move, board->stm, picker->threats);
    else
      current->score =
        GetCaptureHistory(picker->thread, board, move) + MATERIAL_VALUES[PieceType(board->squares[To(move)])] * 8;

    current++;
  }
}

Move NextMove(MovePicker* picker, Board* board, int skipQuiets) {
  switch (picker->phase) {
    case HASH_MOVE:
      picker->phase = GEN_NOISY_MOVES;
      if (IsPseudoLegal(picker->hashMove, board))
        return picker->hashMove;
      // fallthrough
    case GEN_NOISY_MOVES:
      picker->current = picker->endBad = picker->moves;
      picker->end                      = AddNoisyMoves(picker->current, board);

      ScoreMoves(picker, board, !QUIET);

      picker->phase = PLAY_GOOD_NOISY;
      // fallthrough
    case PLAY_GOOD_NOISY:
      if (picker->current != picker->end) {
        Move move = Best(picker->current++, picker->end);

        if (move == picker->hashMove) {
          return NextMove(picker, board, skipQuiets);
        } else if (picker->type != MP_QS && !SEE(board, move, 0)) {
          *picker->endBad++ = *(picker->current - 1);
          return NextMove(picker, board, skipQuiets);
        } else {
          return move;
        }
      }

      picker->phase = PLAY_KILLER_1;
      // fallthrough
    case PLAY_KILLER_1:
      picker->phase = PLAY_KILLER_2;
      if (!skipQuiets && picker->killer1 != picker->hashMove && IsPseudoLegal(picker->killer1, board))
        return picker->killer1;
      // fallthrough
    case PLAY_KILLER_2:
      picker->phase = PLAY_COUNTER;
      if (!skipQuiets && picker->killer2 != picker->hashMove && IsPseudoLegal(picker->killer2, board))
        return picker->killer2;
      // fallthrough
    case PLAY_COUNTER:
      picker->phase = GEN_QUIET_MOVES;
      if (!skipQuiets && picker->counter != picker->hashMove && picker->counter != picker->killer1 &&
          picker->counter != picker->killer2 && IsPseudoLegal(picker->counter, board))
        return picker->counter;
      // fallthrough
    case GEN_QUIET_MOVES:
      if (!skipQuiets) {
        picker->current = picker->endBad;
        picker->end     = AddQuietMoves(picker->current, board);

        ScoreMoves(picker, board, QUIET);
      }

      picker->phase = PLAY_QUIETS;
      // fallthrough
    case PLAY_QUIETS:
      if (picker->current != picker->end && !skipQuiets) {
        Move move = Best(picker->current++, picker->end);

        if (move == picker->hashMove || //
            move == picker->killer1 ||  //
            move == picker->killer2 ||  //
            move == picker->counter)
          return NextMove(picker, board, skipQuiets);
        else
          return move;
      }

      picker->current = picker->moves;
      picker->end     = picker->endBad;

      picker->phase = PLAY_BAD_NOISY;
      // fallthrough
    case PLAY_BAD_NOISY:
      if (picker->current != picker->end) {
        Move move = (picker->current++)->move;

        return move != picker->hashMove ? move : NextMove(picker, board, skipQuiets);
      }

      picker->phase = -1;
      return NULL_MOVE;
    case PERFT_MOVES:
      if (picker->current != picker->end)
        return (picker->current++)->move;

      picker->phase = -1;
      return NULL_MOVE;
    default: return NULL_MOVE;
  }

  return NULL_MOVE;
}

char* PhaseName(MovePicker* picker) {
  switch (picker->phase) {
    case HASH_MOVE: return "HASH_MOVE";
    case PLAY_GOOD_NOISY: return "PLAY_GOOD_NOISY";
    case PLAY_KILLER_1: return "PLAY_KILLER_1";
    case PLAY_KILLER_2: return "PLAY_KILLER_2";
    case PLAY_COUNTER: return "PLAY_COUNTER";
    case PLAY_QUIETS: return "PLAY_QUIETS";
    case PLAY_BAD_NOISY: return "PLAY_BAD_NOISY";
    default: return "UNKNOWN";
  }
}
