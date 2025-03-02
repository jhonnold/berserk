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

#include "evaluate.h"

#include <immintrin.h>
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

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

int16_t L2_WEIGHTS[N_L2 * N_L3] ALIGN;
int32_t L2_BIASES[N_L3] ALIGN;

int16_t OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
int32_t OUTPUT_BIAS;

#if defined(__AVX2__)
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m256i s0 = _mm256_srai_epi16(in[2 * i + 0], 5);
      __m256i s1 = _mm256_srai_epi16(in[2 * i + 1], 5);
      __m256i s2 = _mm256_srai_epi16(in[2 * i + 2], 5);
      __m256i s3 = _mm256_srai_epi16(in[2 * i + 3], 5);

      out[i] =
        _mm256_max_epi8(_mm256_permute4x64_epi64(_mm256_packs_epi16(s0, s1), 0b11011000), _mm256_setzero_si256());
      out[i + 1] =
        _mm256_max_epi8(_mm256_permute4x64_epi64(_mm256_packs_epi16(s2, s3), 0b11011000), _mm256_setzero_si256());
    }
  }
}
#elif defined(__SSE2__)
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m128i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m128i* in = (__m128i*) acc->values[views[v]];
    __m128i* out      = (__m128i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i++) {
      __m128i s0 = _mm_srai_epi16(in[2 * i + 0], 5);
      __m128i s1 = _mm_srai_epi16(in[2 * i + 1], 5);

      out[i] = _mm_packus_epi16(s0, s1);
    }
  }
}
#else
INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const acc_t* in = acc->values[views[v]];
    uint8_t* out    = &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < N_HIDDEN; i++)
      out[i] = Max(0, in[i]) >> 5;
  }
}
#endif

extern uint64_t activations[N_HIDDEN][N_HIDDEN];

int Propagate(Accumulator* accumulator, const int stm) {
  uint8_t x0[N_L1] ALIGN;
  float x1[N_L2] ALIGN;
  float x2[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);

  for (size_t i = 0; i < N_HIDDEN; i++) {
    if (!(x0[i] || x0[i + N_HIDDEN]))
      continue;

    for (size_t j = 0; j < N_HIDDEN; j++)
      activations[i][j] += (x0[j] > 0 || x0[j + N_HIDDEN] > 0);
  }

  return 0;
}

int Predict(Board* board) {
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  return board->stm == WHITE ? Propagate(board->accumulators, WHITE) : Propagate(board->accumulators, BLACK);
}

const size_t NETWORK_SIZE = sizeof(int16_t) * N_FEATURES * N_HIDDEN + // input weights
                            sizeof(int16_t) * N_HIDDEN +              // input biases
                            sizeof(int8_t) * N_L1 * N_L2 +            // input biases
                            sizeof(int32_t) * N_L2 +                  // input biases
                            sizeof(int16_t) * N_L2 * N_L3 +           // input biases
                            sizeof(int32_t) * N_L3 +                  // input biases
                            sizeof(int16_t) * N_L3 +                  // output weights
                            sizeof(int32_t);                          // output bias

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);

  memcpy(L1_WEIGHTS, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
  offset += N_L1 * N_L2 * sizeof(int8_t);
  memcpy(L1_BIASES, &in[offset], N_L2 * sizeof(int32_t));
  offset += N_L2 * sizeof(int32_t);

  memcpy(L2_WEIGHTS, &in[offset], N_L2 * N_L3 * sizeof(int16_t));
  offset += N_L2 * N_L3 * sizeof(int16_t);
  memcpy(L2_BIASES, &in[offset], N_L3 * sizeof(int32_t));
  offset += N_L3 * sizeof(int32_t);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L3 * N_OUTPUT * sizeof(int16_t));
  offset += N_L3 * N_OUTPUT * sizeof(int16_t);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(int32_t));

  // #if defined(__AVX2__)
  //   const size_t WIDTH         = sizeof(__m256i) / sizeof(int16_t);
  //   const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  //   const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  //   __m256i* weights = (__m256i*) INPUT_WEIGHTS;
  //   __m256i* biases  = (__m256i*) INPUT_BIASES;

  //   for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
  //     __m128i a = _mm256_extracti128_si256(weights[i], 1);     // 64->128
  //     __m128i b = _mm256_extracti128_si256(weights[i + 1], 0); // 128->192

  //     weights[i]     = _mm256_inserti128_si256(weights[i], b, 1);     // insert 128->192 into 64->128
  //     weights[i + 1] = _mm256_inserti128_si256(weights[i + 1], a, 0); // insert 64->128 into 128->192
  //   }

  //   for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
  //     __m128i a = _mm256_extracti128_si256(biases[i], 1);     // 64->128
  //     __m128i b = _mm256_extracti128_si256(biases[i + 1], 0); // 128->192

  //     biases[i]     = _mm256_inserti128_si256(biases[i], b, 1);     // insert 128->192 into 64->128
  //     biases[i + 1] = _mm256_inserti128_si256(biases[i + 1], a, 0); // insert 64->128 into 128->192
  //   }
  // #endif
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

INLINE int Contains(int* arr, int n, int a) {
  for (int i = 0; i < n; i++)
    if (arr[i] == a)
      return 1;

  return 0;
}

INLINE void SwapInt(int* a, int* b) {
  int temp = *a;
  *a       = *b;
  *b       = temp;
}

INLINE void SwapInt16(int16_t* a, int16_t* b) {
  int16_t temp = *a;
  *a           = *b;
  *b           = temp;
}

INLINE void SwapInt8(int8_t* a, int8_t* b) {
  int8_t temp = *a;
  *a          = *b;
  *b          = temp;
}

void SortAndWrite(uint64_t activity[N_HIDDEN][N_HIDDEN]) {
  int n                = 0;
  int used[N_HIDDEN]   = {0};
  int scores[N_HIDDEN] = {0};

  for (int chunk = 0; chunk < N_HIDDEN / 4; chunk++) {
    int a = -1, b = -1, c = -1, d = -1;

    for (int i = 0; i < N_HIDDEN; i++)
      if (!Contains(used, n, i) && (a == -1 || activity[i][i] > activity[a][a]))
        a = i;
    used[n++] = a;

    for (int i = 0; i < N_HIDDEN; i++)
      if (!Contains(used, n, i) && (b == -1 || activity[a][i] > activity[a][b]))
        b = i;
    used[n++] = b;

    for (int i = 0; i < N_HIDDEN; i++)
      if (!Contains(used, n, i) && (c == -1 || (activity[a][i] + activity[b][i]) > (activity[a][c] + activity[b][c])))
        c = i;
    used[n++] = c;

    for (int i = 0; i < N_HIDDEN; i++)
      if (!Contains(used, n, i) && (d == -1 || (activity[a][i] + activity[b][i] + activity[c][i]) >
                                                 (activity[a][d] + activity[b][d] + activity[c][d])))
        d = i;
    used[n++] = d;
  }

  for (int i = 0; i < N_HIDDEN; i += 4)
    printf(" %3d %3d %3d %3d\n", used[i], used[i + 1], used[i + 2], used[i + 3]);

  for (int i = 0; i < N_HIDDEN; i++) {
    int u = used[i];

    scores[u] = N_HIDDEN - i;
  }

  for (size_t i = 0; i < N_HIDDEN - 1; i++) {
    size_t max = i;

    for (size_t j = max + 1; j < N_HIDDEN; j++) {
      if (scores[j] > scores[max])
        max = j;
    }

    printf("Max #%lu is %lu\n", i, max);

    if (max == i)
      continue;

    SwapInt(scores + i, scores + max);

    // Swap the features to the input
    SwapInt16(INPUT_BIASES + i, INPUT_BIASES + max);
    for (size_t f = 0; f < N_FEATURES; f++) {
      size_t i0   = f * N_HIDDEN + i;
      size_t max0 = f * N_HIDDEN + max;

      SwapInt16(INPUT_WEIGHTS + i0, INPUT_WEIGHTS + max0);
    }

    // Swap the L1 weights
    for (size_t w = 0; w < N_L2; w++) {
      size_t i0   = w * N_L1 + i;
      size_t max0 = w * N_L1 + max;

      SwapInt8(L1_WEIGHTS + i0, L1_WEIGHTS + max0);

      size_t i1   = w * N_L1 + i + N_HIDDEN;
      size_t max1 = w * N_L1 + max + N_HIDDEN;

      SwapInt8(L1_WEIGHTS + i1, L1_WEIGHTS + max1);
    }
  }

  FILE* fout = fopen("newnet.net", "wb");
  fwrite(INPUT_WEIGHTS, sizeof(int16_t), N_FEATURES * N_HIDDEN, fout);
  fwrite(INPUT_BIASES, sizeof(int16_t), N_HIDDEN, fout);
  fwrite(L1_WEIGHTS, sizeof(int8_t), N_L1 * N_L2, fout);
  fwrite(L1_BIASES, sizeof(int32_t), N_L2, fout);
  fwrite(L2_WEIGHTS, sizeof(int16_t), N_L2 * N_L3, fout);
  fwrite(L2_BIASES, sizeof(int32_t), N_L3, fout);
  fwrite(OUTPUT_WEIGHTS, sizeof(int16_t), N_L3 * N_OUTPUT, fout);
  fwrite(&OUTPUT_BIAS, sizeof(int32_t), N_OUTPUT, fout);
  fclose(fout);
}