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

INLINE Move Best(ScoredMove* current, ScoredMove* end) {
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

INLINE void ScoreMoves(MovePicker* picker, Board* board, const int type) {
  ScoredMove* current = picker->current;
  ThreadData* thread  = picker->thread;
  SearchStack* ss     = picker->ss;

  while (current < picker->end) {
    const Move move    = current->move;
    const int to       = To(move);
    const int pc       = Moving(move);
    const int captured = IsEP(move) ? PAWN : PieceType(board->squares[to]);

    if (type == ST_QUIET || type == ST_EVASION_QT)
      current->score = (int) HH(board->stm, move, board->threatened) * 2 + //
                       (int) (*(ss - 1)->ch)[pc][to] * 2 +                 //
                       (int) (*(ss - 2)->ch)[pc][to] * 2 +                 //
                       (int) (*(ss - 4)->ch)[pc][to] +                     //
                       (int) (*(ss - 6)->ch)[pc][to];
    else if (type == ST_CAPTURE)
      current->score = GetCaptureHistory(picker->thread, move) / 16 + SEE_VALUE[PieceType(board->squares[To(move)])];

    else if (type == ST_EVASION_CAP)
      current->score = 1e7 + SEE_VALUE[captured];

    else if (type == ST_MVV)
      current->score = SEE_VALUE[captured] + 2000 * IsPromo(move);

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

      ScoreMoves(picker, board, ST_CAPTURE);

      picker->phase = PLAY_GOOD_NOISY;
      // fallthrough
    case PLAY_GOOD_NOISY:
      if (picker->current != picker->end) {
        Move move = Best(picker->current, picker->end);
        int score = picker->current->score;
        picker->current++;

        if (move == picker->hashMove) {
          return NextMove(picker, board, skipQuiets);
        } else if (!SEE(board, move, -score / 2)) {
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
        picker->end     = picker->avoidSEELosingQuiets ? AddMostlySafeQuietMoves(picker->current, board) :
                                                         AddQuietMoves(picker->current, board);

        ScoreMoves(picker, board, ST_QUIET);
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

    // Probcut MP Steps
    case PC_GEN_NOISY_MOVES:
      picker->current = picker->endBad = picker->moves;
      picker->end                      = AddNoisyMoves(picker->current, board);

      ScoreMoves(picker, board, ST_MVV);

      picker->phase = PC_PLAY_GOOD_NOISY;
      // fallthrough
    case PC_PLAY_GOOD_NOISY:
      if (picker->current != picker->end) {
        Move move = Best(picker->current++, picker->end);

        if (!SEE(board, move, picker->seeCutoff))
          return NextMove(picker, board, skipQuiets);

        return move;
      }

      picker->phase = -1;
      return NULL_MOVE;

    // QSearch MP Steps
    case QS_GEN_NOISY_MOVES:
      picker->current = picker->moves;
      picker->end     = AddNoisyMoves(picker->current, board);

      ScoreMoves(picker, board, ST_CAPTURE);

      picker->phase = QS_PLAY_NOISY_MOVES;
      // fallthrough
    case QS_PLAY_NOISY_MOVES:
      if (picker->current != picker->end)
        return Best(picker->current++, picker->end);

      if (!picker->genChecks) {
        picker->phase = -1;
        return NULL_MOVE;
      }

      picker->phase = QS_GEN_QUIET_CHECKS;

      // fallthrough
    case QS_GEN_QUIET_CHECKS:
      picker->current = picker->moves;
      picker->end     = AddQuietCheckMoves(picker->current, board);

      picker->phase = QS_PLAY_QUIET_CHECKS;

      // fallthrough
    case QS_PLAY_QUIET_CHECKS:
      if (picker->current != picker->end)
        return (picker->current++)->move;

      picker->phase = -1;
      return NULL_MOVE;

    // QSearch Evasion Steps
    case QS_EVASION_HASH_MOVE:
      picker->phase = QS_EVASION_GEN_NOISY;
      if (IsPseudoLegal(picker->hashMove, board))
        return picker->hashMove;
      // fallthrough
    case QS_EVASION_GEN_NOISY:
      picker->current = picker->moves;
      picker->end     = AddNoisyMoves(picker->current, board);

      ScoreMoves(picker, board, ST_EVASION_CAP);

      picker->phase = QS_EVASION_PLAY_NOISY;

      // fallthrough
    case QS_EVASION_PLAY_NOISY:
      if (picker->current != picker->end) {
        Move move = Best(picker->current++, picker->end);

        return move != picker->hashMove ? move : NextMove(picker, board, skipQuiets);
      }

      picker->phase = QS_EVASION_GEN_QUIET;

      // fallthrough
    case QS_EVASION_GEN_QUIET:
      if (!skipQuiets) {
        picker->current = picker->moves;
        picker->end     = AddQuietMoves(picker->current, board);

        ScoreMoves(picker, board, ST_EVASION_QT);
      }

      picker->phase = QS_EVASION_PLAY_QUIET;

      // fallthrough
    case QS_EVASION_PLAY_QUIET:
      if (picker->current != picker->end && !skipQuiets) {
        Move move = Best(picker->current++, picker->end);

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
