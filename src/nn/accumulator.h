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

#include "../board.h"
#include "../types.h"
#include "../util.h"

#if defined(__SSE2__)
#include <immintrin.h>
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
#define UNROLL     512
#define regi_t     __m512i
#define regi_load  _mm512_load_si512
#define regi_sub   _mm512_sub_epi16
#define regi_add   _mm512_add_epi16
#define regi_store _mm512_store_si512
#elif defined(__AVX2__)
#define UNROLL     256
#define regi_t     __m256i
#define regi_load  _mm256_load_si256
#define regi_sub   _mm256_sub_epi16
#define regi_add   _mm256_add_epi16
#define regi_store _mm256_store_si256
#elif defined(__SSE2__)
#define UNROLL     128
#define regi_t     __m128i
#define regi_load  _mm_load_si128
#define regi_sub   _mm_sub_epi16
#define regi_add   _mm_add_epi16
#define regi_store _mm_store_si128
#else
#define UNROLL           16
#define regi_t           acc_t
#define regi_load(a)     (*(a))
#define regi_sub(a, b)   ((a) - (b))
#define regi_add(a, b)   ((a) + (b))
#define regi_store(a, b) (*(a) = (b))
#endif

#define NUM_REGS 16
#define REG_SIZE (UNROLL / NUM_REGS)

extern int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN];
extern int16_t INPUT_BIASES[N_HIDDEN];

extern int16_t LAZY_INPUT_WEIGHTS[N_FEATURES * N_LAZY];
extern int16_t LAZY_INPUT_BIASES[N_LAZY];

typedef struct {
  uint8_t r, a;
  int rem[32];
  int add[32];
} Delta;

INLINE void ApplyDelta(acc_t* dest, acc_t* src, Delta* delta, const size_t width, const acc_t* inputWeights) {
  regi_t regs[NUM_REGS];

  const size_t chunks = (width + UNROLL - 1) / UNROLL;

  for (size_t c = 0; c < chunks; ++c) {
    const size_t unrollOffset = c * UNROLL;
    const size_t numRegs      = Min(NUM_REGS, (width - unrollOffset) / REG_SIZE);

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_load(&inputs[i]);

    for (size_t r = 0; r < delta->r; r++) {
      const size_t offset   = delta->rem[r] * width + unrollOffset;
      const regi_t* weights = (regi_t*) &inputWeights[offset];
      for (size_t i = 0; i < numRegs; i++)
        regs[i] = regi_sub(regs[i], weights[i]);
    }

    for (size_t a = 0; a < delta->a; a++) {
      const size_t offset   = delta->add[a] * width + unrollOffset;
      const regi_t* weights = (regi_t*) &inputWeights[offset];
      for (size_t i = 0; i < numRegs; i++)
        regs[i] = regi_add(regs[i], weights[i]);
    }

    for (size_t i = 0; i < numRegs; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplySubAdd(acc_t* dest, acc_t* src, int f1, int f2, const size_t width, const acc_t* inputWeights) {
  regi_t regs[NUM_REGS];

  const size_t chunks = (width + UNROLL - 1) / UNROLL;

  for (size_t c = 0; c < chunks; ++c) {
    const size_t unrollOffset = c * UNROLL;
    const size_t numRegs      = Min(NUM_REGS, (width - unrollOffset) / REG_SIZE);

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * width + unrollOffset;
    const regi_t* w1 = (regi_t*) &inputWeights[o1];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    const size_t o2  = f2 * width + unrollOffset;
    const regi_t* w2 = (regi_t*) &inputWeights[o2];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_add(regs[i], w2[i]);

    for (size_t i = 0; i < numRegs; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void
ApplySubSubAdd(acc_t* dest, acc_t* src, int f1, int f2, int f3, const size_t width, const acc_t* inputWeights) {
  regi_t regs[NUM_REGS];

  const size_t chunks = (width + UNROLL - 1) / UNROLL;

  for (size_t c = 0; c < chunks; ++c) {
    const size_t unrollOffset = c * UNROLL;
    const size_t numRegs      = Min(NUM_REGS, (width - unrollOffset) / REG_SIZE);

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * width + unrollOffset;
    const regi_t* w1 = (regi_t*) &inputWeights[o1];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    const size_t o2  = f2 * width + unrollOffset;
    const regi_t* w2 = (regi_t*) &inputWeights[o2];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_sub(regs[i], w2[i]);

    const size_t o3  = f3 * width + unrollOffset;
    const regi_t* w3 = (regi_t*) &inputWeights[o3];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_add(regs[i], w3[i]);

    for (size_t i = 0; i < numRegs; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

INLINE void ApplySubSubAddAdd(acc_t* dest,
                              acc_t* src,
                              int f1,
                              int f2,
                              int f3,
                              int f4,
                              const size_t width,
                              const acc_t* inputWeights) {
  regi_t regs[NUM_REGS];

  const size_t chunks = (width + UNROLL - 1) / UNROLL;

  for (size_t c = 0; c < chunks; ++c) {
    const size_t unrollOffset = c * UNROLL;
    const size_t numRegs      = Min(NUM_REGS, (width - unrollOffset) / REG_SIZE);

    const regi_t* inputs = (regi_t*) &src[unrollOffset];
    regi_t* outputs      = (regi_t*) &dest[unrollOffset];

    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_load(&inputs[i]);

    const size_t o1  = f1 * width + unrollOffset;
    const regi_t* w1 = (regi_t*) &inputWeights[o1];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_sub(regs[i], w1[i]);

    const size_t o2  = f2 * width + unrollOffset;
    const regi_t* w2 = (regi_t*) &inputWeights[o2];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_sub(regs[i], w2[i]);

    const size_t o3  = f3 * width + unrollOffset;
    const regi_t* w3 = (regi_t*) &inputWeights[o3];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_add(regs[i], w3[i]);

    const size_t o4  = f4 * width + unrollOffset;
    const regi_t* w4 = (regi_t*) &inputWeights[o4];
    for (size_t i = 0; i < numRegs; i++)
      regs[i] = regi_add(regs[i], w4[i]);

    for (size_t i = 0; i < numRegs; i++)
      regi_store(&outputs[i], regs[i]);
  }
}

void ResetRefreshTable(AccumulatorKingState* refreshTable);
void RefreshAccumulator(Accumulator* dest, Board* board, const int perspective, const int lazy);

void ResetAccumulator(Accumulator* dest, Board* board, const int perspective, const int lazy);

void BackfillUpdates(Accumulator* live, Board* board, const int view, const int lazy);
int CanEfficientlyUpdate(Accumulator* live, const int view, const int lazy);

#endif
