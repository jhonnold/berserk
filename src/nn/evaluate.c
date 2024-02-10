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

#if defined(__SSE2__)
#include <immintrin.h>
#endif

#define QUANT_BITS 5

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

int8_t L2_WEIGHTS[N_L2 * N_L3] ALIGN;
int32_t L2_BIASES[N_L3] ALIGN;

int8_t OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
int32_t OUTPUT_BIAS;

uint16_t LOOKUP_INDICES[256][8] ALIGN;

INLINE void InputReLU(int8_t* outputs, Accumulator* acc, const int stm) {
  const size_t WIDTH  = sizeof(__m256i) / sizeof(acc_t);
  const size_t CHUNKS = N_HIDDEN / WIDTH;
  const int views[2]  = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const __m256i* in = (__m256i*) acc->values[views[v]];
    __m256i* out      = (__m256i*) &outputs[N_HIDDEN * v];

    for (size_t i = 0; i < CHUNKS / 2; i += 2) {
      __m256i s0 = _mm256_srai_epi16(in[2 * i + 0], QUANT_BITS);
      __m256i s1 = _mm256_srai_epi16(in[2 * i + 1], QUANT_BITS);
      __m256i s2 = _mm256_srai_epi16(in[2 * i + 2], QUANT_BITS);
      __m256i s3 = _mm256_srai_epi16(in[2 * i + 3], QUANT_BITS);

      out[i]     = _mm256_max_epi8(_mm256_packs_epi16(s0, s1), _mm256_setzero_si256());
      out[i + 1] = _mm256_max_epi8(_mm256_packs_epi16(s2, s3), _mm256_setzero_si256());
    }
  }
}

INLINE void m256_add_dpbusd_epi32(__m256i* acc, __m256i a, __m256i b) {
  __m256i p0 = _mm256_maddubs_epi16(a, b);
  p0         = _mm256_madd_epi16(p0, _mm256_set1_epi16(1));
  *acc       = _mm256_add_epi32(*acc, p0);
}

INLINE void m256_add_dpbusd_epi32x2(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
  __m256i p0 = _mm256_maddubs_epi16(a0, b0);
  __m256i p1 = _mm256_maddubs_epi16(a1, b1);

  p0   = _mm256_madd_epi16(_mm256_add_epi16(p0, p1), _mm256_set1_epi16(1));
  *acc = _mm256_add_epi32(*acc, p0);
}

INLINE uint32_t NNZ(__m256i chunk) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(chunk, _mm256_setzero_si256())));
}

INLINE size_t FindNNZ(uint16_t* dest, const int32_t* inputs, const size_t chunks) {
  const size_t IN_WIDTH      = sizeof(__m256i) / sizeof(int32_t);
  const size_t CHUNK_SIZE    = IN_WIDTH;
  const size_t NUM_CHUNKS    = chunks / CHUNK_SIZE;
  const size_t IN_PER_CHUNK  = CHUNK_SIZE / IN_WIDTH;
  const size_t OUT_PER_CHUNK = CHUNK_SIZE / 8;

  const __m256i* in = (__m256i*) inputs;

  size_t count = 0;

  const __m128i increment = _mm_set1_epi16(8);
  __m128i base            = _mm_setzero_si128();

  for (size_t i = 0; i < NUM_CHUNKS; i++) {
    uint32_t nnz = 0;

    for (size_t j = 0; j < IN_PER_CHUNK; j++) {
      const __m256i inputChunk = in[i * IN_PER_CHUNK + j];
      nnz |= NNZ(inputChunk) << (j * IN_WIDTH);
    }

    for (size_t j = 0; j < OUT_PER_CHUNK; j++) {
      const uint16_t lookup = (nnz >> (j * 8)) & 0xFF;
      const __m128i offsets = _mm_loadu_si128((__m128i*) (&LOOKUP_INDICES[lookup]));
      _mm_storeu_si128((__m128i*) (dest + count), _mm_add_epi16(base, offsets));
      count += BitCount(lookup);
      base = _mm_add_epi16(base, increment);
    }
  }

  return count;
}

INLINE void AffineTransformSparse(int32_t* dest,
                                  int8_t* src,
                                  int8_t* layerWeights,
                                  int32_t* layerBiases,
                                  const size_t inWidth,
                                  const size_t outWidth) {
  const size_t REG_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = inWidth / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = outWidth / REG_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m256i* biases = (__m256i*) layerBiases;
  __m256i* out          = (__m256i*) dest;

  uint16_t nnz[NUM_CHUNKS];
  size_t count = FindNNZ(nnz, in32, NUM_CHUNKS);

  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  size_t i = 0;
  for (; i + 1 < count; i += 2) {
    const uint16_t i0 = nnz[i + 0];
    const uint16_t i1 = nnz[i + 1];

    const __m256i f0 = _mm256_set1_epi32(in32[i0]);
    const __m256i f1 = _mm256_set1_epi32(in32[i1]);

    const __m256i* c0 = (__m256i*) &layerWeights[i0 * outWidth * SPARSE_CHUNK_SIZE];
    const __m256i* c1 = (__m256i*) &layerWeights[i1 * outWidth * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32x2(regs + j, f0, c0[j], f1, c1[j]);
  }

  if (i < count) {
    const uint16_t i0 = nnz[i];
    const __m256i f0  = _mm256_set1_epi32(in32[i0]);
    const __m256i* c0 = (__m256i*) &layerWeights[i0 * outWidth * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}

INLINE void AffineTransform(int32_t* dest,
                            int8_t* src,
                            int8_t* layerWeights,
                            int32_t* layerBiases,
                            const size_t inWidth,
                            const size_t outWidth) {
  const size_t REG_WIDTH  = sizeof(__m256i) / sizeof(int32_t);
  const size_t NUM_CHUNKS = inWidth / SPARSE_CHUNK_SIZE;
  const size_t OUT_CC     = outWidth / REG_WIDTH;

  const int32_t* in32   = (int32_t*) src;
  const __m256i* biases = (__m256i*) layerBiases;
  __m256i* out          = (__m256i*) dest;

  __m256i regs[OUT_CC];
  for (size_t i = 0; i < OUT_CC; i++)
    regs[i] = biases[i];

  for (size_t i = 0; i < NUM_CHUNKS; i++) {
    const __m256i f0  = _mm256_set1_epi32(in32[i]);
    const __m256i* c0 = (__m256i*) &layerWeights[i * outWidth * SPARSE_CHUNK_SIZE];

    for (size_t j = 0; j < OUT_CC; j++)
      m256_add_dpbusd_epi32(regs + j, f0, c0[j]);
  }

  for (size_t i = 0; i < OUT_CC; i++)
    out[i] = regs[i];
}

INLINE int32_t AffineOut(int8_t* src, int8_t* layerWeights, int32_t layerBias, const size_t inWidth) {
  const size_t REG_WIDTH  = sizeof(__m256i) / sizeof(int8_t);
  const size_t NUM_CHUNKS = inWidth / REG_WIDTH;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) layerWeights;

  __m256i s0 = _mm256_setzero_si256();
  for (size_t i = 0; i < NUM_CHUNKS; i++)
    m256_add_dpbusd_epi32(&s0, in[i], weights[i]);

  __m128i sum = _mm_add_epi32(_mm256_castsi256_si128(s0), _mm256_extracti128_si256(s0, 1));
  sum         = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_PERM_BADC));
  sum         = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_PERM_CDAB));

  return _mm_cvtsi128_si32(sum) + layerBias;
}

INLINE void ClippedReLU(int8_t* dest, int32_t* src, const size_t width) {
  const size_t REG_WIDTH = sizeof(__m256i) / sizeof(int8_t);

  if (width % REG_WIDTH == 0) {
    const size_t NUM_CHUNKS = width / REG_WIDTH;
    const __m256i ZERO      = _mm256_setzero_si256();
    const __m256i CONTROL   = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

    const __m256i* in = (__m256i*) src;
    __m256i* out      = (__m256i*) dest;

    for (size_t i = 0; i < NUM_CHUNKS; i++) {
      const __m256i s0 = _mm256_srai_epi16(_mm256_packs_epi32(in[4 * i + 0], in[4 * i + 1]), QUANT_BITS);
      const __m256i s1 = _mm256_srai_epi16(_mm256_packs_epi32(in[4 * i + 2], in[4 * i + 3]), QUANT_BITS);

      out[i] = _mm256_permutevar8x32_epi32(_mm256_max_epi8(_mm256_packs_epi16(s0, s1), ZERO), CONTROL);
    }
  } else {
    const size_t NUM_CHUNKS = width / (REG_WIDTH / 2);
    const __m128i ZERO      = _mm_setzero_si128();

    const __m128i* in = (__m128i*) src;
    __m128i* out      = (__m128i*) dest;

    for (size_t i = 0; i < NUM_CHUNKS; i++) {
      const __m128i s0 = _mm_srai_epi16(_mm_packs_epi32(in[4 * i + 0], in[4 * i + 1]), QUANT_BITS);
      const __m128i s1 = _mm_srai_epi16(_mm_packs_epi32(in[4 * i + 2], in[4 * i + 3]), QUANT_BITS);

      out[i] = _mm_max_epi8(_mm_packs_epi16(s0, s1), ZERO);
    }
  }
}

int Propagate(Accumulator* accumulator, const int stm) {
  int8_t x0[N_L1] ALIGN;
  int32_t x1[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);

  AffineTransformSparse(x1, x0, L1_WEIGHTS, L1_BIASES, N_L1, N_L2);

  int32_t skip = x1[N_L2 - 1];
  x1[N_L2 - 1] = 0;

  ClippedReLU(x0, x1, N_L2);
  AffineTransform(x1, x0, L2_WEIGHTS, L2_BIASES, N_L2, N_L3);
  ClippedReLU(x0, x1, N_L3);
  return (skip + AffineOut(x0, OUTPUT_WEIGHTS, OUTPUT_BIAS, N_L3)) >> QUANT_BITS;
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
                            sizeof(int8_t) * N_L2 * N_L3 +            // input biases
                            sizeof(int32_t) * N_L3 +                  // input biases
                            sizeof(int8_t) * N_L3 +                   // output weights
                            sizeof(int32_t);                          // output bias

INLINE int WeightIdxScrambled(int idx, const size_t inWidth, const size_t outWidth) {
  return ((idx / SPARSE_CHUNK_SIZE) % (inWidth / SPARSE_CHUNK_SIZE) * outWidth * SPARSE_CHUNK_SIZE) +
         (idx / inWidth * SPARSE_CHUNK_SIZE) + (idx % SPARSE_CHUNK_SIZE);
}

INLINE void CopyData(const unsigned char* in) {
  size_t offset = 0;

  int8_t* l1 = calloc(N_L1 * N_L2, sizeof(int8_t));
  int8_t* l2 = calloc(N_L2 * N_L3, sizeof(int8_t));

  memcpy(INPUT_WEIGHTS, &in[offset], N_FEATURES * N_HIDDEN * sizeof(int16_t));
  offset += N_FEATURES * N_HIDDEN * sizeof(int16_t);
  memcpy(INPUT_BIASES, &in[offset], N_HIDDEN * sizeof(int16_t));
  offset += N_HIDDEN * sizeof(int16_t);

  memcpy(l1, &in[offset], N_L1 * N_L2 * sizeof(int8_t));
  offset += N_L1 * N_L2 * sizeof(int8_t);
  memcpy(L1_BIASES, &in[offset], N_L2 * sizeof(int32_t));
  offset += N_L2 * sizeof(int32_t);

  memcpy(l2, &in[offset], N_L2 * N_L3 * sizeof(int8_t));
  offset += N_L2 * N_L3 * sizeof(int8_t);
  memcpy(L2_BIASES, &in[offset], N_L3 * sizeof(int32_t));
  offset += N_L3 * sizeof(int32_t);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L3 * N_OUTPUT * sizeof(int8_t));
  offset += N_L3 * N_OUTPUT * sizeof(int8_t);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(int32_t));

#if defined(__SSSE3__)
  // Shuffle the L1 weights for sparse matmul
  for (int i = 0; i < N_L1 * N_L2; i++)
    L1_WEIGHTS[WeightIdxScrambled(i, N_L1, N_L2)] = l1[i];
  for (int i = 0; i < N_L2 * N_L3; i++)
    L2_WEIGHTS[WeightIdxScrambled(i, N_L2, N_L3)] = l2[i];
#else
  for (int i = 0; i < N_L1 * N_L2; i++)
    L1_WEIGHTS[i] = l1[i];
  for (int i = 0; i < N_L1 * N_L2; i++)
    L2_WEIGHTS[i] = l2[i];
#endif

  free(l1);
  free(l2);

#if defined(__AVX512F__) && defined(__AVX512BW__)
  const size_t WIDTH         = sizeof(__m512i) / sizeof(int16_t);
  const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  __m512i* weights = (__m512i*) INPUT_WEIGHTS;
  __m512i* biases  = (__m512i*) INPUT_BIASES;

  for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
    __m128i a1 = _mm512_extracti32x4_epi32(weights[i], 1);
    __m128i a2 = _mm512_extracti32x4_epi32(weights[i], 2);
    __m128i a3 = _mm512_extracti32x4_epi32(weights[i], 3);
    __m128i b0 = _mm512_extracti32x4_epi32(weights[i + 1], 0);
    __m128i b1 = _mm512_extracti32x4_epi32(weights[i + 1], 1);
    __m128i b2 = _mm512_extracti32x4_epi32(weights[i + 1], 2);

    weights[i]     = _mm512_inserti32x4(weights[i], a2, 1);
    weights[i]     = _mm512_inserti32x4(weights[i], b0, 2);
    weights[i]     = _mm512_inserti32x4(weights[i], b2, 3);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], a1, 0);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], a3, 1);
    weights[i + 1] = _mm512_inserti32x4(weights[i + 1], b1, 2);
  }

  for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
    __m128i a1 = _mm512_extracti32x4_epi32(biases[i], 1);
    __m128i a2 = _mm512_extracti32x4_epi32(biases[i], 2);
    __m128i a3 = _mm512_extracti32x4_epi32(biases[i], 3);
    __m128i b0 = _mm512_extracti32x4_epi32(biases[i + 1], 0);
    __m128i b1 = _mm512_extracti32x4_epi32(biases[i + 1], 1);
    __m128i b2 = _mm512_extracti32x4_epi32(biases[i + 1], 2);

    biases[i]     = _mm512_inserti32x4(biases[i], a2, 1);
    biases[i]     = _mm512_inserti32x4(biases[i], b0, 2);
    biases[i]     = _mm512_inserti32x4(biases[i], b2, 3);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], a1, 0);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], a3, 1);
    biases[i + 1] = _mm512_inserti32x4(biases[i + 1], b1, 2);
  }
#elif defined(__AVX2__)
  const size_t WIDTH         = sizeof(__m256i) / sizeof(int16_t);
  const size_t WEIGHT_CHUNKS = (N_FEATURES * N_HIDDEN) / WIDTH;
  const size_t BIAS_CHUNKS   = N_HIDDEN / WIDTH;

  __m256i* weights = (__m256i*) INPUT_WEIGHTS;
  __m256i* biases  = (__m256i*) INPUT_BIASES;

  for (size_t i = 0; i < WEIGHT_CHUNKS; i += 2) {
    __m128i a1 = _mm256_extracti128_si256(weights[i], 1);
    __m128i b0 = _mm256_extracti128_si256(weights[i + 1], 0);

    weights[i]     = _mm256_inserti128_si256(weights[i], b0, 1);
    weights[i + 1] = _mm256_inserti128_si256(weights[i + 1], a1, 0);
  }

  for (size_t i = 0; i < BIAS_CHUNKS; i += 2) {
    __m128i a1 = _mm256_extracti128_si256(biases[i], 1);
    __m128i b0 = _mm256_extracti128_si256(biases[i + 1], 0);

    biases[i]     = _mm256_inserti128_si256(biases[i], b0, 1);
    biases[i + 1] = _mm256_inserti128_si256(biases[i + 1], a1, 0);
  }
#endif
}

INLINE void InitLookupIndices() {
  for (size_t i = 0; i < 256; i++) {
    uint64_t j = i;
    uint64_t k = 0;
    while (j)
      LOOKUP_INDICES[i][k++] = PopLSB(&j);
  }
}

void LoadDefaultNN() {
  InitLookupIndices();

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
