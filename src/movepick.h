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

#ifndef MOVEPICK_H
#define MOVEPICK_H

#include "move.h"
#include "movegen.h"
#include "search.h"
#include "thread.h"
#include "types.h"
#include "util.h"

enum {
  HASH_MOVE,
  GEN_NOISY_MOVES,
  PLAY_GOOD_NOISY,
  PLAY_KILLER_1,
  PLAY_KILLER_2,
  PLAY_COUNTER,
  GEN_QUIET_MOVES,
  PLAY_QUIETS,
  PLAY_BAD_NOISY,
  // ProbCut
  PC_GEN_NOISY_MOVES,
  PC_PLAY_GOOD_NOISY,
  // QSearch
  QS_GEN_NOISY_MOVES,
  QS_PLAY_NOISY_MOVES,
  QS_GEN_QUIET_CHECKS,
  QS_PLAY_QUIET_CHECKS,
  // QSearch Evasions
  QS_EVASION_HASH_MOVE,
  QS_EVASION_GEN_NOISY,
  QS_EVASION_PLAY_NOISY,
  QS_EVASION_GEN_QUIET,
  QS_EVASION_PLAY_QUIET,
  // ...
  NO_MORE_MOVES,
  PERFT_MOVES,
};

typedef struct {
  ThreadData* thread;
  SearchStack* ss;
  Move hashMove, killer1, killer2, counter;
  int seeCutoff, phase, genChecks;

  ScoredMove *current, *end, *endBad;
  ScoredMove moves[MAX_MOVES];
} MovePicker;

enum {
  ST_QUIET,
  ST_CAPTURE,
  ST_EVASION_CAP,
  ST_EVASION_QT,
  ST_MVV
};

INLINE void InitNormalMovePicker(MovePicker* picker, Move hashMove, ThreadData* thread, SearchStack* ss) {
  picker->phase = HASH_MOVE;

  picker->hashMove = hashMove;
  picker->killer1  = ss->killers[0];
  picker->killer2  = ss->killers[1];
  if ((ss - 1)->move)
    picker->counter = thread->counters[Moving((ss - 1)->move)][To((ss - 1)->move)];
  else
    picker->counter = NULL_MOVE;

  picker->thread = thread;
  picker->ss     = ss;
}

INLINE void InitPCMovePicker(MovePicker* picker, ThreadData* thread, int threshold) {
  picker->phase     = PC_GEN_NOISY_MOVES;
  picker->thread    = thread;
  picker->seeCutoff = threshold;
}

INLINE void InitQSMovePicker(MovePicker* picker, ThreadData* thread, int genChecks) {
  picker->phase     = QS_GEN_NOISY_MOVES;
  picker->thread    = thread;
  picker->genChecks = genChecks;
}

INLINE void InitQSEvasionsPicker(MovePicker* picker, Move hashMove, ThreadData* thread, SearchStack* ss) {
  picker->phase = QS_EVASION_HASH_MOVE;

  picker->hashMove = hashMove;

  picker->thread = thread;
  picker->ss     = ss;
}

INLINE void InitPerftMovePicker(MovePicker* picker, Board* board) {
  picker->phase   = PERFT_MOVES;
  picker->current = picker->moves;

  picker->end = AddPerftMoves(picker->moves, board);
}

Move NextMove(MovePicker* picker, Board* board, int skipQuiets);

#endif