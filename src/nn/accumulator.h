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

#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <string.h>

#include "../bits.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../types.h"
#include "../util.h"
#include "simd.h"
#include "threats.h"

extern int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN];
extern int16_t INPUT_BIASES[N_HIDDEN];

typedef struct {
  uint8_t r, a;
  int rem[32];
  int add[32];
} Delta;

INLINE void ApplyDelta(acc_t* dest, acc_t* src, Delta* delta) {
  vi16_t regs[NUM_REGS];

  for (size_t c = 0; c < N_HIDDEN / UNROLL; ++c) {
    const size_t unrollOffset = c * UNROLL;

    const vi16_t* inputs = (vi16_t*) &src[unrollOffset];
    vi16_t* outputs      = (vi16_t*) &dest[unrollOffset];

    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = vi16_load(&inputs[i]);

    for (size_t r = 0; r < delta->r; r++) {
      const size_t offset   = delta->rem[r] * N_HIDDEN + unrollOffset;
      const vi16_t* weights = (vi16_t*) &INPUT_WEIGHTS[offset];
      for (size_t i = 0; i < NUM_REGS; i++)
        regs[i] = vi16_sub(regs[i], weights[i]);
    }

    for (size_t a = 0; a < delta->a; a++) {
      const size_t offset   = delta->add[a] * N_HIDDEN + unrollOffset;
      const vi16_t* weights = (vi16_t*) &INPUT_WEIGHTS[offset];
      for (size_t i = 0; i < NUM_REGS; i++)
        regs[i] = vi16_add(regs[i], weights[i]);
    }

    for (size_t i = 0; i < NUM_REGS; i++)
      vi16_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplyFusedDelta(acc_t* dest, const acc_t* src, const int* pRem, const int pr, const int* pAdd,
                            const int pa, const int* tRem, const int tr, const int* tAdd, const int ta) {
  const size_t LANES = sizeof(vi16_t) / sizeof(acc_t);

  for (size_t c = 0; c < N_HIDDEN / (LANES * THREAT_TILE); c++) {
    const size_t offset = c * LANES * THREAT_TILE;
    const vi16_t* in    = (const vi16_t*) &src[offset];
    vi16_t* out         = (vi16_t*) &dest[offset];

    vi16_t regs[THREAT_TILE];
    for (size_t i = 0; i < THREAT_TILE; i++)
      regs[i] = vi16_load(&in[i]);

    for (int k = 0; k < pr; k++) {
      const vi16_t* w = (const vi16_t*) &INPUT_WEIGHTS[(size_t) pRem[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = vi16_sub(regs[i], vi16_load(&w[i]));
    }

    for (int k = 0; k < pa; k++) {
      const vi16_t* w = (const vi16_t*) &INPUT_WEIGHTS[(size_t) pAdd[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = vi16_add(regs[i], vi16_load(&w[i]));
    }

    for (int k = 0; k < tr; k++) {
      const int8_t* w = &THREAT_WEIGHTS[(size_t) tRem[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = vi16_sub(regs[i], vi16_load8(&w[i * LANES]));
    }

    for (int k = 0; k < ta; k++) {
      const int8_t* w = &THREAT_WEIGHTS[(size_t) tAdd[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = vi16_add(regs[i], vi16_load8(&w[i * LANES]));
    }

    for (size_t i = 0; i < THREAT_TILE; i++)
      vi16_store(&out[i], regs[i]);
  }
}

INLINE void AddAllThreats(acc_t* dest, const acc_t* src, Board* board, const int view) {
  int add[256];
  const int a = ActiveThreats(board, view, add);

  ApplyFusedDelta(dest, src, NULL, 0, NULL, 0, NULL, 0, add, a);
}

INLINE void ThreatIndices(const Accumulator* step, const int view, const int flip, int* rem, int* r, int* add,
                          int* a) {
  for (int i = 0; i < step->nDirty; i++) {
    const uint32_t d = step->dirty[i];
    const int idx    = ThreatIdx(DirtyAttacker(d), DirtyFrom(d), DirtyVictim(d), DirtyTo(d), view, flip);
    if (idx < 0)
      continue;

    __builtin_prefetch(&THREAT_WEIGHTS[(size_t) idx * N_HIDDEN]);

    if (DirtyAdd(d))
      add[(*a)++] = idx;
    else
      rem[(*r)++] = idx;
  }
}

void ResetRefreshTable(AccumulatorKingState* refreshTable);
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective);

INLINE void RefreshAccumulator(Accumulator* dest, Board* board, const int perspective) {
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

  // ApplyDelta reads and writes the full 1024 element state even when nothing
  // changed in this bucket, so skip the pass outright when the diff is empty.
  if (delta->r || delta->a)
    ApplyDelta(state->values, state->values, delta);

  AddAllThreats(dest->values[perspective], state->values, board, perspective);
  dest->correct[perspective] = 1;
}

INLINE void PrefetchRow(int f) {
  __builtin_prefetch(&INPUT_WEIGHTS[(size_t) f * N_HIDDEN]);
}

INLINE void ApplySubAdd(acc_t* dest, acc_t* src, int f1, int f2, const int* tRem, int tr, const int* tAdd, int ta) {
  const int rem[1] = {f1}, add[1] = {f2};
  PrefetchRow(f1), PrefetchRow(f2);
  ApplyFusedDelta(dest, src, rem, 1, add, 1, tRem, tr, tAdd, ta);
}

INLINE void ApplySubSubAdd(acc_t* dest, acc_t* src, int f1, int f2, int f3, const int* tRem, int tr, const int* tAdd,
                           int ta) {
  const int rem[2] = {f1, f2}, add[1] = {f3};
  PrefetchRow(f1), PrefetchRow(f2), PrefetchRow(f3);
  ApplyFusedDelta(dest, src, rem, 2, add, 1, tRem, tr, tAdd, ta);
}

INLINE void ApplySubSubAddAdd(acc_t* dest, acc_t* src, int f1, int f2, int f3, int f4, const int* tRem, int tr,
                              const int* tAdd, int ta) {
  const int rem[2] = {f1, f2}, add[2] = {f3, f4};
  PrefetchRow(f1), PrefetchRow(f2), PrefetchRow(f3), PrefetchRow(f4);
  ApplyFusedDelta(dest, src, rem, 2, add, 2, tRem, tr, tAdd, ta);
}

INLINE void ApplyUpdates(acc_t* output, acc_t* prev, Board* board, const Accumulator* step, const int view,
                         const int flip) {
  const Move move      = step->move;
  const int captured   = step->captured;
  const int king       = LSB(PieceBB(KING, view));
  const int movingSide = Moving(move) & 1;

  int tRem[MAX_DIRTY_THREATS], tAdd[MAX_DIRTY_THREATS], tr = 0, ta = 0;
  ThreatIndices(step, view, flip, tRem, &tr, tAdd, &ta);

  int from = FeatureIdx(Moving(move), From(move), king, view);
  int to   = FeatureIdx(IsPromo(move) ? PromoPiece(move, movingSide) : Moving(move), To(move), king, view);

  if (IsCas(move)) {
    int rookFrom = FeatureIdx(Piece(ROOK, movingSide), board->cr[CASTLING_ROOK[To(move)]], king, view);
    int rookTo   = FeatureIdx(Piece(ROOK, movingSide), CASTLE_ROOK_DEST[To(move)], king, view);

    ApplySubSubAddAdd(output, prev, from, rookFrom, to, rookTo, tRem, tr, tAdd, ta);
  } else if (IsCap(move)) {
    int capSq      = IsEP(move) ? To(move) - PawnDir(movingSide) : To(move);
    int capturedTo = FeatureIdx(captured, capSq, king, view);

    ApplySubSubAdd(output, prev, from, capturedTo, to, tRem, tr, tAdd, ta);
  } else {
    ApplySubAdd(output, prev, from, to, tRem, tr, tAdd, ta);
  }
}

INLINE void ApplyLazyUpdates(Accumulator* live, Board* board, const int view) {
  Accumulator* curr = live;
  while (!(--curr)->correct[view])
    ; // go back to the latest correct accumulator

  const int flip = FeatureFlip(LSB(PieceBB(KING, view)), view);

  do {
    ApplyUpdates((curr + 1)->values[view], curr->values[view], board, curr, view, flip);
    (curr + 1)->correct[view] = 1;
  } while (++curr != live);
}

INLINE int CanEfficientlyUpdate(Accumulator* live, const int view) {
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

void LoadDefaultNN();
int LoadNetwork(char* path);

#endif
