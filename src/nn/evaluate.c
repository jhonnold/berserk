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

#include "evaluate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bits.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"
#include "../thread.h"
#include "../util.h"
#include "accumulator.h"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "../incbin.h"

INCBIN(Embed, EVALFILE);

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int16_t OUTPUT_WEIGHTS[N_L1 * N_OUTPUT] ALIGN;
int32_t OUTPUT_BIAS;

const size_t OUT_WIDTH = sizeof(regi_t) / sizeof(acc_t);
const size_t OUT_CC    = N_HIDDEN / OUT_WIDTH;

int Propagate(Accumulator* accumulator, const int stm) {
  const int views[2] = {stm, !stm};

  regi_out_t acc    = regi_zero();
  const regi_t zero = regi_zero();

  for (size_t v = 0; v < 2; v++) {
    const regi_t* in      = (regi_t*) accumulator->values[views[v]];
    const regi_t* weights = (regi_t*) &OUTPUT_WEIGHTS[v * N_HIDDEN];

    for (size_t c = 0; c < OUT_CC; c++) {
      const regi_t a0 = regi_max(zero, in[c]);

      acc = regi_add32(acc, regi_madd(a0, weights[c]));
    }
  }

#if defined(__AVX512F__) && defined(__AVX512BW__)
  const __m256i r8 = _mm256_add_epi32(_mm512_castsi512_si256(acc), _mm512_extracti32x8_epi32(acc, 1));
#elif defined(__AVX2__)
  const __m256i r8 = acc;
#endif

#if defined(__AVX2__)
  const __m128i r4 = _mm_add_epi32(_mm256_castsi256_si128(r8), _mm256_extractf128_si256(r8, 1));
#elif defined(__SSE2__)
  const __m128i r4 = acc;
#endif

#if defined(__SSE2__)
  const __m128i r2     = _mm_add_epi32(r4, _mm_srli_si128(r4, 8));
  const __m128i r1     = _mm_add_epi32(r2, _mm_srli_si128(r2, 4));
  const int32_t result = _mm_cvtsi128_si32(r1);
#else
  const int32_t result = acc;
#endif

  return (result + OUTPUT_BIAS) / 16 / 256;
}

int Predict(Board* board) {
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  return board->stm == WHITE ? Propagate(board->accumulators, WHITE) : Propagate(board->accumulators, BLACK);
}

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN + // input weights
                            sizeof(int16_t) * N_HIDDEN +              // input biases
                            sizeof(int16_t) * N_L1 * N_OUTPUT +       // input biases
                            sizeof(int32_t) * N_OUTPUT;               // input biases

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L1 * N_OUTPUT * sizeof(int16_t));
  offset += N_L1 * N_OUTPUT * sizeof(int16_t);
  memcpy(&OUTPUT_BIAS, &in[offset], N_OUTPUT * sizeof(int32_t));
  offset += N_OUTPUT * sizeof(int32_t);
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

  for (int i = 0; i < Threads.count; i++)
    ResetRefreshTable(Threads.threads[i]->refreshTable);

  fclose(fin);
  free(data);

  return 1;
}
