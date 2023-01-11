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
#define UNROLL 512
#elif defined(__AVX2__)
#define UNROLL 256
#else
#define UNROLL 128
#endif

INLINE void ApplyFeature(acc_t* dest, acc_t* src, int f, const int add) {
  for (size_t c = 0; c < N_HIDDEN; c += UNROLL)
    for (size_t i = 0; i < UNROLL; i++)
      dest[i + c] = src[i + c] + (2 * add - 1) * INPUT_WEIGHTS[f * N_HIDDEN + i + c];
}

int OutputLayer(acc_t* stm, acc_t* xstm) {
  int result = OUTPUT_BIAS;

  for (size_t c = 0; c < N_HIDDEN; c += UNROLL)
    for (size_t i = 0; i < UNROLL; i++)
      result += max(stm[c + i], 0) * OUTPUT_WEIGHTS[c + i];

  for (size_t c = 0; c < N_HIDDEN; c += UNROLL)
    for (size_t i = 0; i < UNROLL; i++)
      result += max(xstm[c + i], 0) * OUTPUT_WEIGHTS[c + i + N_HIDDEN];

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
  int kingSq     = lsb(PieceBB(KING, perspective));
  int pBucket    = perspective == WHITE ? 0 : 2 * N_KING_BUCKETS;
  int kingBucket = KING_BUCKETS[kingSq ^ (56 * perspective)] + N_KING_BUCKETS * (File(kingSq) > 3);

  AccumulatorKingState* state = &board->refreshTable[pBucket + kingBucket];

  for (int pc = WHITE_PAWN; pc <= BLACK_KING; pc++) {
    BitBoard curr = board->pieces[pc];
    BitBoard prev = state->pcs[pc];

    BitBoard rem = prev & ~curr;
    BitBoard add = curr & ~prev;

    while (rem) {
      int sq = popAndGetLsb(&rem);
      ApplyFeature(state->values, state->values, FeatureIdx(pc, sq, kingSq, perspective), SUB);
    }

    while (add) {
      int sq = popAndGetLsb(&add);
      ApplyFeature(state->values, state->values, FeatureIdx(pc, sq, kingSq, perspective), ADD);
    }

    state->pcs[pc] = curr;
  }

  // Copy in state
  memcpy(dest->values[perspective], state->values, sizeof(acc_t) * N_HIDDEN);
}

// Resets an accumulator from pieces on the board
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective) {
  int kingSq    = lsb(PieceBB(KING, perspective));
  acc_t* values = dest->values[perspective];

  memcpy(values, INPUT_BIASES, sizeof(acc_t) * N_HIDDEN);

  BitBoard occ = OccBB(BOTH);
  while (occ) {
    int sq      = popAndGetLsb(&occ);
    int pc      = board->squares[sq];
    int feature = FeatureIdx(pc, sq, kingSq, perspective);

    ApplyFeature(values, values, feature, ADD);
  }
}

void ApplyUpdates(Board* board, Move move, int captured, const int view) {
  int16_t* output = board->accumulators->values[view];
  int16_t* prev   = (board->accumulators - 1)->values[view];

  const int king = lsb(PieceBB(KING, view));

  int f = FeatureIdx(Moving(move), From(move), king, view);
  ApplyFeature(output, prev, f, SUB);

  int endPc = !Promo(move) ? Moving(move) : Promo(move);
  f         = FeatureIdx(endPc, To(move), king, view);
  ApplyFeature(output, output, f, ADD);

  if (IsCap(move)) {
    int movingSide = Moving(move) & 1;
    int capturedSq = IsEP(move) ? To(move) - PawnDir(movingSide) : To(move);
    f              = FeatureIdx(captured, capturedSq, king, view);

    ApplyFeature(output, output, f, SUB);
  }

  if (IsCas(move)) {
    int movingSide = Moving(move) & 1;
    int rook       = Piece(ROOK, movingSide);

    int rookFrom = board->cr[CASTLING_ROOK[To(move)]];
    int rookTo   = CASTLE_ROOK_DEST[To(move)];

    f = FeatureIdx(rook, rookFrom, king, view);
    ApplyFeature(output, output, f, SUB);
    f = FeatureIdx(rook, rookTo, king, view);
    ApplyFeature(output, output, f, ADD);
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
