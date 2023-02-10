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

#include "nn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "util.h"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

INCBIN(Embed, EVALFILE);

const int QUANTIZATION_PRECISION_IN  = 16;
const int QUANTIZATION_PRECISION_OUT = 512;

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;
int16_t OUTPUT_WEIGHTS[2 * N_HIDDEN] ALIGN;
int32_t OUTPUT_BIAS;

#if defined(__AVX512F__)
#include <immintrin.h>
#define UNROLL     256
#define NUM_REGS   8
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
#elif defined(__SSE2__)
#include <immintrin.h>
#define UNROLL     128
#define NUM_REGS   16
#define regi_t     __m128i
#define regi_load  _mm_load_si128
#define regi_sub   _mm_sub_epi16
#define regi_add   _mm_add_epi16
#define regi_store _mm_store_si128
#else
#define UNROLL           16
#define NUM_REGS         16
#define regi_t           acc_t
#define regi_load(a)     (*(a))
#define regi_sub(a, b)   ((a) - (b))
#define regi_add(a, b)   ((a) + (b))
#define regi_store(a, b) (*(a) = (b))
#endif

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

int OutputLayer(acc_t* stm, acc_t* xstm) {
  int result = OUTPUT_BIAS;

  for (size_t c = 0; c < N_HIDDEN; c += UNROLL)
    for (size_t i = 0; i < UNROLL; i++)
      result += Max(stm[c + i], 0) * OUTPUT_WEIGHTS[c + i];

  for (size_t c = 0; c < N_HIDDEN; c += UNROLL)
    for (size_t i = 0; i < UNROLL; i++)
      result += Max(xstm[c + i], 0) * OUTPUT_WEIGHTS[c + i + N_HIDDEN];

  return result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}

int Predict(Board* board) {
  Accumulator acc[1];

  ResetAccumulator(acc, board, board->stm);
  ResetAccumulator(acc, board, board->xstm);

  return OutputLayer(acc->values[board->stm], acc->values[board->xstm]);
}

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
}

void ApplyUpdates(Board* board, const Move move, const int captured, const int view) {
  acc_t* output = board->accumulators->values[view];
  acc_t* prev   = (board->accumulators - 1)->values[view];

  const int king       = LSB(PieceBB(KING, view));
  const int movingSide = Moving(move) & 1;

  int from = FeatureIdx(Moving(move), From(move), king, view);
  int to   = FeatureIdx(Promo(move) ?: Moving(move), To(move), king, view);

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

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN + // input weights
                            sizeof(int16_t) * N_HIDDEN +              // input biases
                            sizeof(int16_t) * 2 * N_HIDDEN +          // output weights
                            sizeof(int32_t);                          // output bias

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);
  memcpy(OUTPUT_WEIGHTS, &in[offset], 2 * N_HIDDEN * sizeof(int16_t));
  offset += 2 * N_HIDDEN * sizeof(int16_t);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(int32_t));
}

void LoadDefaultNN() {
  CopyData(EmbedData);
}

int LoadNetwork(char* path) {
  FILE* fin = fopen(path, "rb");
  if (fin == NULL) {
    printf("info string Unable to read file at %s\n", path);
    return 0;
  }

  uint8_t* data = malloc(NETWORK_SIZE);
  if (fread(data, sizeof(uint8_t), NETWORK_SIZE, fin) != NETWORK_SIZE) {
    printf("info string Error reading file at %s\n", path);
    return 0;
  }

  CopyData(data);

  fclose(fin);
  free(data);

  return 1;
}
