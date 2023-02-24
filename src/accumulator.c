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
#include <string.h>

#include "bits.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

extern int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
extern int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

INLINE void ApplyDelta(acc_t* dest, acc_t* src, Delta* delta) {
  regi_t regs[NUM_REGS];

  for (size_t c = 0; c < N_HIDDEN / UNROLL; ++c) {
    const size_t unrollOffset = c * UNROLL;

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_load(&inputs[i]);

    for (size_t r = 0; r < delta->r; r++) {
      const size_t offset   = delta->rem[r] * N_HIDDEN + unrollOffset;
      const regi_t* weights = (regi_t*) &INPUT_WEIGHTS[offset];
      for (size_t i = 0; i < NUM_REGS; i++)
        regs[i] = regi_sub(regs[i], weights[i]);
    }

    for (size_t a = 0; a < delta->a; a++) {
      const size_t offset   = delta->add[a] * N_HIDDEN + unrollOffset;
      const regi_t* weights = (regi_t*) &INPUT_WEIGHTS[offset];
      for (size_t i = 0; i < NUM_REGS; i++)
        regs[i] = regi_add(regs[i], weights[i]);
    }

    for (size_t i = 0; i < NUM_REGS; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplySub(acc_t* dest, acc_t* src, int f1) {
  regi_t regs[NUM_REGS];

  for (size_t c = 0; c < N_HIDDEN / UNROLL; ++c) {
    const size_t unrollOffset = c * UNROLL;

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * N_HIDDEN + unrollOffset;
    const regi_t* w1 = (regi_t*) &INPUT_WEIGHTS[o1];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    for (size_t i = 0; i < NUM_REGS; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplySubAdd(acc_t* dest, acc_t* src, int f1, int f2) {
  regi_t regs[NUM_REGS];

  for (size_t c = 0; c < N_HIDDEN / UNROLL; ++c) {
    const size_t unrollOffset = c * UNROLL;

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * N_HIDDEN + unrollOffset;
    const regi_t* w1 = (regi_t*) &INPUT_WEIGHTS[o1];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    const size_t o2  = f2 * N_HIDDEN + unrollOffset;
    const regi_t* w2 = (regi_t*) &INPUT_WEIGHTS[o2];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_add(regs[i], w2[i]);

    for (size_t i = 0; i < NUM_REGS; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplySubSubAdd(acc_t* dest, acc_t* src, int f1, int f2, int f3) {
  regi_t regs[NUM_REGS];

  for (size_t c = 0; c < N_HIDDEN / UNROLL; ++c) {
    const size_t unrollOffset = c * UNROLL;

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * N_HIDDEN + unrollOffset;
    const regi_t* w1 = (regi_t*) &INPUT_WEIGHTS[o1];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    const size_t o2  = f2 * N_HIDDEN + unrollOffset;
    const regi_t* w2 = (regi_t*) &INPUT_WEIGHTS[o2];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_sub(regs[i], w2[i]);

    const size_t o3  = f3 * N_HIDDEN + unrollOffset;
    const regi_t* w3 = (regi_t*) &INPUT_WEIGHTS[o3];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_add(regs[i], w3[i]);

    for (size_t i = 0; i < NUM_REGS; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

void ResetRefreshTable(AccumulatorKingState* refreshTable) {
  for (size_t b = 0; b < 2 * 2 * N_KING_BUCKETS; b++) {
    AccumulatorKingState* state = refreshTable + b;

    memcpy(state->values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
    memset(state->pcs, 0, sizeof(BitBoard) * 10);
  }
}

// Refreshes an accumulator using a diff from the last known board state
// with proper king bucketing
void RefreshAccumulator(Accumulator* dest, Board* board, const int perspective) {
  Delta delta[1];
  delta->r = delta->a = 0;

  int kingSq     = LSB(PieceBB(KING, perspective));
  int pBucket    = perspective == WHITE ? 0 : 2 * N_KING_BUCKETS;
  int kingBucket = sq64_to_sq32(kingSq ^ (56 * !perspective)) + N_KING_BUCKETS * (File(kingSq) > 3);

  AccumulatorKingState* state = &board->refreshTable[pBucket + kingBucket];

  for (int pc = WHITE_PAWN; pc <= BLACK_QUEEN; pc++) {
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
}

// Resets an accumulator from pieces on the board
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective) {
  Delta delta[1];
  delta->r = delta->a = 0;

  int kingSq = LSB(PieceBB(KING, perspective));

  BitBoard occ = OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ PieceBB(KING, BLACK);
  while (occ) {
    int sq                 = PopLSB(&occ);
    int pc                 = board->squares[sq];
    delta->add[delta->a++] = FeatureIdx(pc, sq, kingSq, perspective);
  }

  acc_t* values = dest->values[perspective];
  memcpy(values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);
  ApplyDelta(values, values, delta);
}

void ApplyUpdates(Board* board, const Move move, const int captured, const int view) {
  acc_t* output = board->accumulators->values[view];
  acc_t* prev   = (board->accumulators - 1)->values[view];

  const int king       = LSB(PieceBB(KING, view));
  const int movingSide = Moving(move) & 1;
  const int kingMove   = PieceType(Moving(move)) == KING;

  int from = FeatureIdx(Moving(move), From(move), king, view);
  int to   = FeatureIdx(Promo(move) ?: Moving(move), To(move), king, view);

  if (IsCas(move)) {
    int rookFrom = FeatureIdx(Piece(ROOK, movingSide), board->cr[CASTLING_ROOK[To(move)]], king, view);
    int rookTo   = FeatureIdx(Piece(ROOK, movingSide), CASTLE_ROOK_DEST[To(move)], king, view);

    ApplySubAdd(output, prev, rookFrom, rookTo);
  } else if (IsCap(move)) {
    int capSq      = IsEP(move) ? To(move) - PawnDir(movingSide) : To(move);
    int capturedTo = FeatureIdx(captured, capSq, king, view);

    if (!kingMove)
      ApplySubSubAdd(output, prev, from, capturedTo, to);
    else
      ApplySub(output, prev, capturedTo);
  } else if (!kingMove) {
    ApplySubAdd(output, prev, from, to);
  } else {
    memcpy(output, prev, sizeof(acc_t) * N_HIDDEN);
  }
}
