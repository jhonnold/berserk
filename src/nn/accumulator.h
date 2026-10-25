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
#include "threats.h"

#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
#define UNROLL     512
#define NUM_REGS   16
#define regi_t     __m512i
#define regi_load  _mm512_load_si512
#define regi_sub   _mm512_sub_epi16
#define regi_add   _mm512_add_epi16
#define regi_store _mm512_store_si512
#elif defined(__AVX2__)
#include <immintrin.h>
#define UNROLL     256
#define NUM_REGS   16
#define regi_t     __m256i
#define regi_load  _mm256_load_si256
#define regi_sub   _mm256_sub_epi16
#define regi_add   _mm256_add_epi16
#define regi_store _mm256_store_si256
#elif defined(__SSE4_1__)
#include <immintrin.h>
#define UNROLL     128
#define NUM_REGS   16
#define regi_t     __m128i
#define regi_load  _mm_load_si128
#define regi_sub   _mm_sub_epi16
#define regi_add   _mm_add_epi16
#define regi_store _mm_store_si128
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define UNROLL           128
#define NUM_REGS         16
#define regi_t           int16x8_t
#define regi_load(a)     vld1q_s16((int16_t*) (a))
#define regi_sub(a, b)   vsubq_s16(a, b)
#define regi_add(a, b)   vaddq_s16(a, b)
#define regi_store(a, b) vst1q_s16((int16_t*) (a), b)
#else
#define UNROLL           16
#define NUM_REGS         16
#define regi_t           acc_t
#define regi_load(a)     (*(a))
#define regi_sub(a, b)   ((a) - (b))
#define regi_add(a, b)   ((a) + (b))
#define regi_store(a, b) (*(a) = (b))
#endif

extern int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN];
extern int16_t INPUT_BIASES[N_HIDDEN];

typedef struct {
  uint8_t r, a;
  int rem[32];
  int add[32];
} Delta;

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

INLINE void ApplySubSubAddAdd(acc_t* dest, acc_t* src, int f1, int f2, int f3, int f4) {
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

    const size_t o4  = f4 * N_HIDDEN + unrollOffset;
    const regi_t* w4 = (regi_t*) &INPUT_WEIGHTS[o4];
    for (size_t i = 0; i < NUM_REGS; i++)
      regs[i] = regi_add(regs[i], w4[i]);

    for (size_t i = 0; i < NUM_REGS; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

// `dest = src - psq rows + psq rows - threat rows + threat rows` in one pass, the PSQ
// rows int16 and the threat rows int8 widened to the int16 sums. PSQ and threat
// features share the one accumulator.
#if defined(__AVX2__)
// Registers per pass. The accumulator is 64 ymm wide, so a pass holds an eighth of it
// and leaves registers for the widened rows.
#define THREAT_TILE 8

INLINE void ApplyFusedDelta(acc_t* dest, const acc_t* src, const int* pRem, const int pr, const int* pAdd,
                            const int pa, const int* tRem, const int tr, const int* tAdd, const int ta) {
  for (size_t c = 0; c < N_HIDDEN / (16 * THREAT_TILE); c++) {
    const size_t offset = c * 16 * THREAT_TILE;
    const __m256i* in   = (const __m256i*) &src[offset];
    __m256i* out        = (__m256i*) &dest[offset];

    __m256i regs[THREAT_TILE];
    for (size_t i = 0; i < THREAT_TILE; i++)
      regs[i] = _mm256_load_si256(&in[i]);

    for (int k = 0; k < pr; k++) {
      const __m256i* w = (const __m256i*) &INPUT_WEIGHTS[(size_t) pRem[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = _mm256_sub_epi16(regs[i], _mm256_load_si256(&w[i]));
    }

    for (int k = 0; k < pa; k++) {
      const __m256i* w = (const __m256i*) &INPUT_WEIGHTS[(size_t) pAdd[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = _mm256_add_epi16(regs[i], _mm256_load_si256(&w[i]));
    }

    for (int k = 0; k < tr; k++) {
      const __m128i* w = (const __m128i*) &THREAT_WEIGHTS[(size_t) tRem[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = _mm256_sub_epi16(regs[i], _mm256_cvtepi8_epi16(_mm_load_si128(&w[i])));
    }

    for (int k = 0; k < ta; k++) {
      const __m128i* w = (const __m128i*) &THREAT_WEIGHTS[(size_t) tAdd[k] * N_HIDDEN + offset];
      for (size_t i = 0; i < THREAT_TILE; i++)
        regs[i] = _mm256_add_epi16(regs[i], _mm256_cvtepi8_epi16(_mm_load_si128(&w[i])));
    }

    for (size_t i = 0; i < THREAT_TILE; i++)
      _mm256_store_si256(&out[i], regs[i]);
  }
}
#else
INLINE void ApplyFusedDelta(acc_t* dest, const acc_t* src, const int* pRem, const int pr, const int* pAdd,
                            const int pa, const int* tRem, const int tr, const int* tAdd, const int ta) {
  int16_t sums[N_HIDDEN];
  memcpy(sums, src, sizeof(sums));

  for (int k = 0; k < pr; k++)
    for (int i = 0; i < N_HIDDEN; i++)
      sums[i] -= INPUT_WEIGHTS[(size_t) pRem[k] * N_HIDDEN + i];

  for (int k = 0; k < pa; k++)
    for (int i = 0; i < N_HIDDEN; i++)
      sums[i] += INPUT_WEIGHTS[(size_t) pAdd[k] * N_HIDDEN + i];

  for (int k = 0; k < tr; k++)
    for (int i = 0; i < N_HIDDEN; i++)
      sums[i] -= THREAT_WEIGHTS[(size_t) tRem[k] * N_HIDDEN + i];

  for (int k = 0; k < ta; k++)
    for (int i = 0; i < N_HIDDEN; i++)
      sums[i] += THREAT_WEIGHTS[(size_t) tAdd[k] * N_HIDDEN + i];

  memcpy(dest, sums, sizeof(sums));
}
#endif

// `dest = src + every threat row of the board`, for a rebuild. The PSQ part of `src`
// carries the bias.
INLINE void AddAllThreats(acc_t* dest, const acc_t* src, Board* board, const int view) {
  int add[256];
  const int a = ActiveThreats(board, view, add);

  ApplyFusedDelta(dest, src, NULL, 0, NULL, 0, NULL, 0, add, a);
}

// One move's threat changes, as `view` sees them.
INLINE void ThreatIndices(const Accumulator* step, const int view, const int flip, int* rem, int* r, int* add,
                          int* a) {
  for (int i = 0; i < step->nDirty; i++) {
    const uint32_t d = step->dirty[i];
    const int idx    = ThreatIdx(DirtyAttacker(d), DirtyFrom(d), DirtyVictim(d), DirtyTo(d), view, flip);
    if (idx < 0)
      continue;

    // The rows are spread over 61 MB; start the first line's fetch while the rest of
    // the list is indexed.
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

  // The cached state is PSQ only; the threats are rebuilt on top of it.
  AddAllThreats(dest->values[perspective], state->values, board, perspective);
  dest->correct[perspective] = 1;
}

INLINE void ApplyUpdates(acc_t* output, acc_t* prev, Board* board, const Accumulator* step, const int view,
                         const int flip) {
  const Move move      = step->move;
  const int captured   = step->captured;
  const int king       = LSB(PieceBB(KING, view));
  const int movingSide = Moving(move) & 1;

  int pRem[2], pAdd[2], pr = 0, pa = 0;
  pRem[pr++] = FeatureIdx(Moving(move), From(move), king, view);
  pAdd[pa++] = FeatureIdx(IsPromo(move) ? PromoPiece(move, movingSide) : Moving(move), To(move), king, view);

  if (IsCas(move)) {
    pRem[pr++] = FeatureIdx(Piece(ROOK, movingSide), board->cr[CASTLING_ROOK[To(move)]], king, view);
    pAdd[pa++] = FeatureIdx(Piece(ROOK, movingSide), CASTLE_ROOK_DEST[To(move)], king, view);
  } else if (IsCap(move)) {
    int capSq  = IsEP(move) ? To(move) - PawnDir(movingSide) : To(move);
    pRem[pr++] = FeatureIdx(captured, capSq, king, view);
  }

  int tRem[MAX_DIRTY_THREATS], tAdd[MAX_DIRTY_THREATS], tr = 0, ta = 0;
  ThreatIndices(step, view, flip, tRem, &tr, tAdd, &ta);

  ApplyFusedDelta(output, prev, pRem, pr, pAdd, pa, tRem, tr, tAdd, ta);
}

INLINE void ApplyLazyUpdates(Accumulator* live, Board* board, const int view) {
  Accumulator* curr = live;
  while (!(--curr)->correct[view])
    ; // go back to the latest correct accumulator

  // King moves that change the mirror force a refresh, so one flip serves the chain.
  const int flip = ThreatFlip(LSB(PieceBB(KING, view)), view);

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
