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

void InitAllMoves(MoveList* moves, Move hashMove, ThreadData* thread, SearchStack* ss, BitBoard threats) {
  moves->type         = ALL_MOVES;
  moves->phase        = HASH_MOVE;
  moves->nTactical    = 0;
  moves->nQuiets      = 0;
  moves->nBadTactical = 0;
  moves->seeCutoff    = 0;
  moves->threats      = threats;

  moves->hashMove = hashMove;
  moves->killer1  = ss->killers[0];
  moves->killer2  = ss->killers[1];

  Move parent      = (ss - 1)->move;
  int parentTo     = To(parent);
  int parentMoving = Promo(parent) ? Piece(PAWN, thread->board.xstm) : thread->board.squares[parentTo];
  moves->counter   = parent ? thread->counters[parentMoving][parentTo] : NULL_MOVE;

  moves->thread = thread;
  moves->ss     = ss;
}

void InitTacticalMoves(MoveList* moves, ThreadData* thread, int cutoff) {
  moves->type         = TACTICAL_MOVES;
  moves->phase        = GEN_TACTICAL_MOVES;
  moves->nTactical    = 0;
  moves->nQuiets      = 0;
  moves->nBadTactical = 0;
  moves->seeCutoff    = cutoff;
  moves->threats      = 0;

  moves->hashMove = NULL_MOVE;
  moves->killer1  = NULL_MOVE;
  moves->killer2  = NULL_MOVE;
  moves->counter  = NULL_MOVE;

  moves->thread = thread;
}

void InitPerftMoves(MoveList* moves, Board* board) {
  moves->type      = ALL_MOVES;
  moves->phase     = PERFT_MOVES;
  moves->nTactical = 0;
  moves->nQuiets   = 0;

  GenerateAllMoves(moves, board);
}

int GetTopIdx(int* arr, int n) {
  int m = 0;
  for (int i = m + 1; i < n; i++)
    if (arr[i] > arr[m]) m = i;

  return m;
}

int GetTopReverseIdx(int* arr, int n) {
  int m = MAX_MOVES - 1;
  for (int i = m - 1; i > MAX_MOVES - n - 1; i--)
    if (arr[i] > arr[m]) m = i;

  return m;
}

inline void ShiftToBadCaptures(MoveList* moves, int idx) {
  // Put the bad capture starting at the end
  moves->tactical[MAX_MOVES - 1 - moves->nBadTactical]  = moves->tactical[idx];
  moves->sTactical[MAX_MOVES - 1 - moves->nBadTactical] = moves->sTactical[idx];
  moves->nBadTactical++;

  // put the last good capture here instead
  moves->nTactical--;
  moves->tactical[idx]  = moves->tactical[moves->nTactical];
  moves->sTactical[idx] = moves->tactical[moves->nTactical];
}

inline Move PopGoodCapture(MoveList* moves, int idx) {
  Move temp = moves->tactical[idx];

  moves->nTactical--;
  moves->tactical[idx]  = moves->tactical[moves->nTactical];
  moves->sTactical[idx] = moves->sTactical[moves->nTactical];

  return temp;
}

inline Move PopQuiet(MoveList* moves, int idx) {
  Move temp = moves->quiet[idx];

  moves->nQuiets--;
  moves->quiet[idx]  = moves->quiet[moves->nQuiets];
  moves->sQuiet[idx] = moves->sQuiet[moves->nQuiets];

  return temp;
}

inline Move PopBadCapture(MoveList* moves) {
  Move temp = moves->tactical[MAX_MOVES - 1];

  moves->nBadTactical--;
  moves->tactical[MAX_MOVES - 1]  = moves->tactical[MAX_MOVES - 1 - moves->nBadTactical];
  moves->sTactical[MAX_MOVES - 1] = moves->tactical[MAX_MOVES - 1 - moves->nBadTactical];

  return temp;
}

void ScoreTacticalMoves(MoveList* moves, Board* board) {
  for (int i = 0; i < moves->nTactical; i++) {
    Move m = moves->tactical[i];

    int captured        = PieceType(board->squares[To(m)]);
    moves->sTactical[i] = GetTacticalHistory(moves->thread, board, m) + MATERIAL_VALUES[captured] * 32;
  }
}

void ScoreQuietMoves(MoveList* moves, Board* board, ThreadData* thread) {
  for (int i = 0; i < moves->nQuiets; i++) {
    Move m = moves->quiet[i];

    moves->sQuiet[i] = GetQuietHistory(board, moves->ss, thread, m, board->stm, moves->threats);
  }
}

Move NextMove(MoveList* moves, Board* board, int skipQuiets) {
  switch (moves->phase) {
    case HASH_MOVE:
      moves->phase = GEN_TACTICAL_MOVES;
      if (IsPseudoLegal(moves->hashMove, board)) return moves->hashMove;
      // fallthrough
    case GEN_TACTICAL_MOVES:
      GenerateTacticalMoves(moves, board);
      ScoreTacticalMoves(moves, board);
      moves->phase = PLAY_GOOD_TACTICAL;
      // fallthrough
    case PLAY_GOOD_TACTICAL:
      if (moves->nTactical > 0) {
        int idx = GetTopIdx(moves->sTactical, moves->nTactical);
        Move m  = moves->tactical[idx];

        if (m == moves->hashMove) {
          PopGoodCapture(moves, idx);
          return NextMove(moves, board, skipQuiets);
        }

        if (!SEE(board, m, moves->seeCutoff)) {
          ShiftToBadCaptures(moves, idx);
          return NextMove(moves, board, skipQuiets);
        }

        return PopGoodCapture(moves, idx);
      }

      if (skipQuiets) {
        moves->phase = PLAY_BAD_TACTICAL;
        return NextMove(moves, board, skipQuiets);
      }

      moves->phase = PLAY_KILLER_1;
      // fallthrough
    case PLAY_KILLER_1:
      moves->phase = PLAY_KILLER_2;
      if (!skipQuiets && moves->killer1 != moves->hashMove && !IsCapture(moves->killer1, board) &&
          IsPseudoLegal(moves->killer1, board))
        return moves->killer1;
      // fallthrough
    case PLAY_KILLER_2:
      moves->phase = PLAY_COUNTER;
      if (!skipQuiets && moves->killer2 != moves->hashMove && !IsCapture(moves->killer2, board) &&
          IsPseudoLegal(moves->killer2, board))
        return moves->killer2;
      // fallthrough
    case PLAY_COUNTER:
      moves->phase = GEN_QUIET_MOVES;
      if (!skipQuiets && moves->counter != moves->hashMove && moves->counter != moves->killer1 &&
          moves->counter != moves->killer2 && !IsCapture(moves->counter, board) && IsPseudoLegal(moves->counter, board))
        return moves->counter;
      // fallthrough
    case GEN_QUIET_MOVES:
      if (!skipQuiets) {
        GenerateQuietMoves(moves, board);
        ScoreQuietMoves(moves, board, moves->thread);
      }

      moves->phase = PLAY_QUIETS;
      // fallthrough
    case PLAY_QUIETS:
      if (moves->nQuiets > 0 && !skipQuiets) {
        int idx = GetTopIdx(moves->sQuiet, moves->nQuiets);
        Move m  = PopQuiet(moves, idx);

        if (m == moves->hashMove || m == moves->killer1 || m == moves->killer2 || m == moves->counter)
          return NextMove(moves, board, skipQuiets);

        return m;
      }

      moves->phase = PLAY_BAD_TACTICAL;
      // fallthrough
    case PLAY_BAD_TACTICAL:
      if (moves->nBadTactical > 0) {
        Move m = PopBadCapture(moves);

        return m != moves->hashMove ? m : NextMove(moves, board, skipQuiets);
      }

      moves->phase = NO_MORE_MOVES;
      return NULL_MOVE;
    case PERFT_MOVES:
      if (moves->nTactical > 0) {
        Move m = PopGoodCapture(moves, 0);

        return m;
      }

      if (moves->nQuiets > 0) {
        Move m = PopQuiet(moves, 0);

        return m;
      }

      moves->phase = NO_MORE_MOVES;
      return NULL_MOVE;
    case NO_MORE_MOVES: return NULL_MOVE;
  }

  return NULL_MOVE;
}

char* PhaseName(MoveList* list) {
  switch (list->phase) {
    case HASH_MOVE: return "HASH_MOVE";
    case PLAY_GOOD_TACTICAL: return "PLAY_GOOD_TACTICAL";
    case PLAY_KILLER_1: return "PLAY_KILLER_1";
    case PLAY_KILLER_2: return "PLAY_KILLER_2";
    case PLAY_COUNTER: return "PLAY_COUNTER";
    case PLAY_QUIETS: return "PLAY_QUIETS";
    case PLAY_BAD_TACTICAL: return "PLAY_BAD_TACTICAL";
    default: return "UNKNOWN";
  }
}
