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

#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "see.h"

void InitAllMoves(MoveList* moves, Move hashMove, SearchData* data) {
  moves->type = ALL_MOVES;
  moves->phase = HASH_MOVE;
  moves->nTactical = 0;
  moves->nQuiets = 0;
  moves->nBadTactical = 0;

  moves->hashMove = hashMove;
  moves->killer1 = data->killers[data->ply][0];
  moves->killer2 = data->killers[data->ply][1];
  moves->counter = data->ply ? data->counters[MoveStartEnd(data->moves[data->ply - 1])] : NULL_MOVE;

  moves->data = data;
}

void InitTacticalMoves(MoveList* moves, SearchData* data) {
  moves->type = TACTICAL_MOVES;
  moves->phase = GEN_TACTICAL_MOVES;
  moves->nTactical = 0;
  moves->nQuiets = 0;
  moves->nBadTactical = 0;

  moves->hashMove = NULL_MOVE;
  moves->killer1 = NULL_MOVE;
  moves->killer2 = NULL_MOVE;
  moves->counter = NULL_MOVE;

  moves->data = data;
}

int GetTopIdx(int* arr, int n) {
  int m = 0;
  for (int i = m + 1; i < n; i++)
    if (arr[i] > arr[m])
      m = i;

  return m;
}

int GetTopReverseIdx(int* arr, int n) {
  int m = MAX_MOVES - 1;
  for (int i = m - 1; i > MAX_MOVES - n - 1; i--)
    if (arr[i] > arr[m])
      m = i;

  return m;
}

inline void ShiftToBadCaptures(MoveList* moves, int idx) {
  // Put the bad capture starting at the end
  moves->tactical[MAX_MOVES - 1 - moves->nBadTactical] = moves->tactical[idx];
  moves->sTactical[MAX_MOVES - 1 - moves->nBadTactical] = moves->sTactical[idx];
  moves->nBadTactical++;

  // put the last good capture here instead
  moves->nTactical--;
  moves->tactical[idx] = moves->tactical[moves->nTactical];
  moves->sTactical[idx] = moves->tactical[moves->nTactical];
}

inline Move PopGoodCapture(MoveList* moves, int idx) {
  Move temp = moves->tactical[idx];

  moves->nTactical--;
  moves->tactical[idx] = moves->tactical[moves->nTactical];
  moves->sTactical[idx] = moves->sTactical[moves->nTactical];

  return temp;
}

inline Move PopQuiet(MoveList* moves, int idx) {
  Move temp = moves->quiet[idx];

  moves->nQuiets--;
  moves->quiet[idx] = moves->quiet[moves->nQuiets];
  moves->sQuiet[idx] = moves->sQuiet[moves->nQuiets];

  return temp;
}

inline Move PopBadCapture(MoveList* moves, int idx) {
  Move temp = moves->tactical[idx];

  moves->nBadTactical--;
  moves->tactical[idx] = moves->tactical[MAX_MOVES - 1 - moves->nBadTactical];
  moves->sTactical[idx] = moves->tactical[MAX_MOVES - 1 - moves->nBadTactical];

  return temp;
}

void ScoreTacticalMoves(MoveList* moves, Board* board) {
  for (int i = 0; i < moves->nTactical; i++) {
    Move m = moves->tactical[i];
    int attacker = MovePiece(m);
    int victim = MoveEP(m) ? PAWN_WHITE : MovePromo(m) ? MovePromo(m) : board->squares[MoveEnd(m)];

    moves->sTactical[i] = MVV_LVA[attacker][victim];
  }
}

void ScoreQuietMoves(MoveList* moves, Board* board, SearchData* data) {
  for (int i = 0; i < moves->nQuiets; i++) {
    Move m = moves->quiet[i];

    moves->sQuiet[i] = data->hh[board->side][MoveStartEnd(m)];
  }
}

void ScoreTacticalMovesWithSEE(MoveList* moves, Board* board) {
  for (int i = 0; i < moves->nTactical; i++) {
    Move m = moves->tactical[i];
    int see = SEE(board, m);
    if (MoveEP(m))
      see += STATIC_MATERIAL_VALUE[PAWN_TYPE];

    moves->sTactical[i] = see;
  }
}

Move NextMove(MoveList* moves, Board* board, int skipQuiets) {
  switch (moves->phase) {
  case HASH_MOVE:
    moves->phase = GEN_TACTICAL_MOVES;
    if (MoveIsLegal(moves->hashMove, board))
      return moves->hashMove;
    // fallthrough
  case GEN_TACTICAL_MOVES:
    GenerateTacticalMoves(moves, board);
    ScoreTacticalMoves(moves, board);
    moves->phase = PLAY_GOOD_TACTICAL;
    // fallthrough
  case PLAY_GOOD_TACTICAL:
    if (moves->nTactical > 0) {
      int idx = GetTopIdx(moves->sTactical, moves->nTactical);
      Move m = moves->tactical[idx];

      if (m == moves->hashMove) {
        PopGoodCapture(moves, idx);
        return NextMove(moves, board, skipQuiets);
      }

      int see = 0;
      if (MoveCapture(m) && !MoveEP(m) && PIECE_TYPE[board->squares[MoveEnd(m)]] < PIECE_TYPE[MovePiece(m)])
        see = SEE(board, m);

      if (see < 0) {
        moves->sTactical[idx] = see;
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
    if (!skipQuiets && moves->killer1 != moves->hashMove && MoveIsLegal(moves->killer1, board))
      return moves->killer1;
    // fallthrough
  case PLAY_KILLER_2:
    moves->phase = PLAY_COUNTER;
    if (!skipQuiets && moves->killer2 != moves->hashMove && MoveIsLegal(moves->killer2, board))
      return moves->killer2;
    // fallthrough
  case PLAY_COUNTER:
    moves->phase = GEN_QUIET_MOVES;
    if (!skipQuiets && moves->counter != moves->hashMove && moves->counter != moves->killer1 && moves->counter != moves->killer2 &&
        MoveIsLegal(moves->counter, board))
      return moves->counter;
    // fallthrough
  case GEN_QUIET_MOVES:
    if (!skipQuiets) {
      GenerateQuietMoves(moves, board);
      ScoreQuietMoves(moves, board, moves->data);
    }

    moves->phase = PLAY_QUIETS;
    // fallthrough
  case PLAY_QUIETS:
    if (moves->nQuiets > 0 && !skipQuiets) {
      int idx = GetTopIdx(moves->sQuiet, moves->nQuiets);
      Move m = PopQuiet(moves, idx);

      return m != moves->hashMove ? m : NextMove(moves, board, skipQuiets);
    }

    moves->phase = PLAY_BAD_TACTICAL;
    // fallthrough
  case PLAY_BAD_TACTICAL:
    if (moves->nBadTactical > 0) {
      int idx = GetTopReverseIdx(moves->sTactical, moves->nBadTactical);
      Move m = PopBadCapture(moves, idx);

      return m != moves->hashMove ? m : NextMove(moves, board, skipQuiets);
    }

    moves->phase = NO_MORE_MOVES;
    // fallthrough
  case NO_MORE_MOVES:
    return NULL_MOVE;
  }

  return NULL_MOVE;
}