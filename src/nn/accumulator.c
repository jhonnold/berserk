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

#include "accumulator.h"

#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bits.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../util.h"

void ResetRefreshTable(AccumulatorKingState* refreshTable) {
  for (size_t b = 0; b < 2 * 2 * N_KING_BUCKETS; b++) {
    AccumulatorKingState* state = refreshTable + b;

    memcpy(state->values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
    memset(state->pcs, 0, sizeof(BitBoard) * 12);
  }
}

// Refreshes an accumulator using a diff from the last known board state
// with proper king bucketing
void RefreshAccumulator(Accumulator* dest, Board* board, const int perspective) {
  Delta delta[1];
  delta->r = delta->a = 0;

  int kingSq     = LSB(PieceBB(KING, perspective));
  int pBucket    = perspective == WHITE ? 0 : 2 * N_KING_BUCKETS;
  int kingBucket = KING_BUCKETS[kingSq ^ (56 * perspective)] + N_KING_BUCKETS * (File(kingSq) > 3);

  AccumulatorKingState* state = &board->refreshTable[pBucket + kingBucket];

  for (int pc = WHITE_PAWN; pc <= BLACK_KING; pc++) {
    BitBoard curr = board->pieces[pc];
    BitBoard prev = state->pcs[pc];

    BitBoard rem = prev & ~curr;
    BitBoard add = curr & ~prev;

    while (rem) {
      int sq                 = PopLSB(&rem);
      delta->rem[delta->r++] = FeatureIdx(pc, sq, kingSq, perspective);
    }

    while (add) {
      int sq                 = PopLSB(&add);
      delta->add[delta->a++] = FeatureIdx(pc, sq, kingSq, perspective);
    }

    state->pcs[pc] = curr;
  }

  ApplyDelta(state->values, state->values, delta);

  // Copy in state
  memcpy(dest->values[perspective], state->values, sizeof(acc_t) * N_HIDDEN);
  dest->correct[perspective] = 1;
}

// Resets an accumulator from pieces on the board
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective) {
  Delta delta[1];
  delta->r = delta->a = 0;

  int kingSq = LSB(PieceBB(KING, perspective));

  BitBoard occ = OccBB(BOTH);
  while (occ) {
    int sq                 = PopLSB(&occ);
    int pc                 = board->squares[sq];
    delta->add[delta->a++] = FeatureIdx(pc, sq, kingSq, perspective);
  }

  acc_t* values = dest->values[perspective];
  memcpy(values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
  ApplyDelta(values, values, delta);
  dest->correct[perspective] = 1;
}

void ApplyUpdates(acc_t* output, acc_t* prev, Board* board, const Move move, const int captured, const int view) {
  const int king       = LSB(PieceBB(KING, view));
  const int movingSide = Moving(move) & 1;

  int from = FeatureIdx(Moving(move), From(move), king, view);
  int to   = FeatureIdx(IsPromo(move) ? PromoPiece(move, movingSide) : Moving(move), To(move), king, view);

  if (IsCas(move)) {
    int rookFrom = FeatureIdx(Piece(ROOK, movingSide), board->cr[CASTLING_ROOK[To(move)]], king, view);
    int rookTo   = FeatureIdx(Piece(ROOK, movingSide), CASTLE_ROOK_DEST[To(move)], king, view);

    ApplySubSubAddAdd(output, prev, from, rookFrom, to, rookTo);
  } else if (IsCap(move)) {
    int capSq      = IsEP(move) ? To(move) - PawnDir(movingSide) : To(move);
    int capturedTo = FeatureIdx(captured, capSq, king, view);

    ApplySubSubAdd(output, prev, from, capturedTo, to);
  } else {
    ApplySubAdd(output, prev, from, to);
  }
}

void ApplyLazyUpdates(Accumulator* live, Board* board, const int view) {
  Accumulator* curr = live;
  while (!(--curr)->correct[view])
    ; // go back to the latest correct accumulator

  do {
    ApplyUpdates((curr + 1)->values[view], curr->values[view], board, curr->move, curr->captured, view);
    (curr + 1)->correct[view] = 1;
  } while (++curr != live);
}

int CanEfficientlyUpdate(Accumulator* live, const int view) {
  Accumulator* curr = live;

  while (1) {
    curr--;

    int from  = From(curr->move) ^ (56 * view); // invert for black
    int to    = To(curr->move) ^ (56 * view);   // invert for black
    int piece = Moving(curr->move);

    if ((piece & 1) == view && MoveRequiresRefresh(piece, from, to))
      return 0; // refresh only necessary for our view
    if (curr->correct[view])
      return 1;
  }
}
