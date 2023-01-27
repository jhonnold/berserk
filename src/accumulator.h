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

#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include "types.h"

#define NUM_REGS 16

#if defined(__AVX512F__)
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
#else
#define UNROLL     128
#define regi_t     __m128i
#define regi_load  _mm_load_si128
#define regi_sub   _mm_sub_epi16
#define regi_add   _mm_add_epi16
#define regi_store _mm_store_si128
#endif

typedef struct {
  uint8_t r, a;
  int rem[32];
  int add[32];
} Delta;

void ResetRefreshTable(AccumulatorKingState* refreshTable);
void RefreshAccumulator(Accumulator* dest, Board* board, const int perspective);
void ResetAccumulator(Accumulator* dest, Board* board, const int perspective);
void ApplyUpdates(Board* board, Move move, int captured, const int view);

#endif