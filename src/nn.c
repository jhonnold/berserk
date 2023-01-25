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

int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN] ALIGN;
int16_t INPUT_BIASES[N_HIDDEN] ALIGN;

int8_t L1_WEIGHTS[N_L1 * N_L2] ALIGN;
int32_t L1_BIASES[N_L2] ALIGN;

float L2_WEIGHTS[N_L2 * N_L3] ALIGN;
float L2_BIASES[N_L3] ALIGN;

float OUTPUT_WEIGHTS[N_L3 * N_OUTPUT] ALIGN;
float OUTPUT_BIAS;

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

INLINE void InputReLU(uint8_t* outputs, Accumulator* acc, const int stm) {
  const int views[2] = {stm, !stm};

  for (int v = 0; v < 2; v++) {
    const int offset = N_HIDDEN * v;
    const int chunks = (16 * N_HIDDEN) / UNROLL;

    __m256i* out = (__m256i*) &outputs[offset];

    for (int i = 0; i < chunks / 2; i++) {
      __m256i s0 = _mm256_srai_epi16(((__m256i*) acc->values[views[v]])[2 * i + 0], 6);
      __m256i s1 = _mm256_srai_epi16(((__m256i*) acc->values[views[v]])[2 * i + 1], 6);

      out[i] = _mm256_permute4x64_epi64(_mm256_packus_epi16(s0, s1), 0b11011000);
    }
  }
}

INLINE void m256_add_dpbusd_epi32x2(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
  __m256i product0 = _mm256_maddubs_epi16(a0, b0);
  __m256i product1 = _mm256_maddubs_epi16(a1, b1);
  product0         = _mm256_adds_epi16(product0, product1);
  product0         = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
  *acc             = _mm256_add_epi32(*acc, product0);
}

INLINE void m256_add_dpbusd_epi32x4(__m256i* acc,
                                    __m256i a0,
                                    __m256i b0,
                                    __m256i a1,
                                    __m256i b1,
                                    __m256i a2,
                                    __m256i b2,
                                    __m256i a3,
                                    __m256i b3) {
  __m256i product0 = _mm256_maddubs_epi16(a0, b0);
  __m256i product1 = _mm256_maddubs_epi16(a1, b1);
  __m256i product2 = _mm256_maddubs_epi16(a2, b2);
  __m256i product3 = _mm256_maddubs_epi16(a3, b3);
  product0         = _mm256_adds_epi16(product0, product1);
  product0         = _mm256_adds_epi16(product0, product2);
  product0         = _mm256_adds_epi16(product0, product3);
  product0         = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
  *acc             = _mm256_add_epi32(*acc, product0);
}

INLINE void m256_add_dpbusd_epi32x4_asm(__m256i* acc,
                                        __m256i a0,
                                        __m256i b0,
                                        __m256i a1,
                                        __m256i b1,
                                        __m256i a2,
                                        __m256i b2,
                                        __m256i a3,
                                        __m256i b3) {
  __m256i tmp0 = _mm256_maddubs_epi16(a0, b0);
  __m256i tmp1 = _mm256_maddubs_epi16(a1, b1);
  __m256i tmp2 = _mm256_maddubs_epi16(a2, b2);
  __m256i tmp3 = _mm256_maddubs_epi16(a3, b3);
  asm(
    "vpaddsw     %[tmp0], %[tmp1], %[tmp0]\n\t"
    "vpaddsw     %[tmp0], %[tmp2], %[tmp0]\n\t"
    "vpaddsw     %[tmp0], %[tmp3], %[tmp0]\n\t"
    "vpmaddwd    %[tmp0], %[ones], %[tmp0]\n\t"
    "vpaddd      %[acc], %[tmp0], %[acc]"
    : [acc] "+v"(*acc), [tmp0] "+&v"(tmp0)
    : [tmp1] "v"(tmp1), [tmp2] "v"(tmp2), [tmp3] "v"(tmp3), [ones] "v"(_mm256_set1_epi16(1)));
}

INLINE void m256_add_dpbusd_epi32x2_asm(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
  __m256i tmp0 = _mm256_maddubs_epi16(a0, b0);
  __m256i tmp1 = _mm256_maddubs_epi16(a1, b1);
  asm(
    "vpaddsw     %[tmp0], %[tmp1], %[tmp0]\n\t"
    "vpmaddwd    %[tmp0], %[ones], %[tmp0]\n\t"
    "vpaddd      %[acc], %[tmp0], %[acc]\n\t"
    : [acc] "+v"(*acc), [tmp0] "+&v"(tmp0)
    : [tmp1] "v"(tmp1), [ones] "v"(_mm256_set1_epi16(1)));
}

INLINE void L1AffineReLU(float* dest, uint8_t* src, int8_t* srcWeights, int32_t* srcBiases) {
  const size_t IN_WIDTH   = sizeof(__m256i) / sizeof(uint8_t);
  const size_t IN_CHUNKS  = (2 * N_HIDDEN) / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L2 / OUT_CC;

  const __m256i* in      = (__m256i*) src;
  const __m256i* weights = (__m256i*) srcWeights;
  const __m256i* biases  = (__m256i*) srcBiases;
  __m256* out            = (__m256*) dest;

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    __m256i a0 = _mm256_setzero_si256();
    __m256i a1 = _mm256_setzero_si256();
    __m256i a2 = _mm256_setzero_si256();
    __m256i a3 = _mm256_setzero_si256();
    __m256i a4 = _mm256_setzero_si256();
    __m256i a5 = _mm256_setzero_si256();
    __m256i a6 = _mm256_setzero_si256();
    __m256i a7 = _mm256_setzero_si256();

    const int o0 = (OUT_CC * i) * IN_CHUNKS;
    const int o1 = (OUT_CC * i + 1) * IN_CHUNKS;
    const int o2 = (OUT_CC * i + 2) * IN_CHUNKS;
    const int o3 = (OUT_CC * i + 3) * IN_CHUNKS;
    const int o4 = (OUT_CC * i + 4) * IN_CHUNKS;
    const int o5 = (OUT_CC * i + 5) * IN_CHUNKS;
    const int o6 = (OUT_CC * i + 6) * IN_CHUNKS;
    const int o7 = (OUT_CC * i + 7) * IN_CHUNKS;

    for (size_t j = 0; j < IN_CHUNKS; j += 4) {
      m256_add_dpbusd_epi32x4_asm(&a0,
                                  in[j],
                                  weights[o0 + j],
                                  in[j + 1],
                                  weights[o0 + j + 1],
                                  in[j + 2],
                                  weights[o0 + j + 2],
                                  in[j + 3],
                                  weights[o0 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a1,
                                  in[j],
                                  weights[o1 + j],
                                  in[j + 1],
                                  weights[o1 + j + 1],
                                  in[j + 2],
                                  weights[o1 + j + 2],
                                  in[j + 3],
                                  weights[o1 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a2,
                                  in[j],
                                  weights[o2 + j],
                                  in[j + 1],
                                  weights[o2 + j + 1],
                                  in[j + 2],
                                  weights[o2 + j + 2],
                                  in[j + 3],
                                  weights[o2 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a3,
                                  in[j],
                                  weights[o3 + j],
                                  in[j + 1],
                                  weights[o3 + j + 1],
                                  in[j + 2],
                                  weights[o3 + j + 2],
                                  in[j + 3],
                                  weights[o3 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a4,
                                  in[j],
                                  weights[o4 + j],
                                  in[j + 1],
                                  weights[o4 + j + 1],
                                  in[j + 2],
                                  weights[o4 + j + 2],
                                  in[j + 3],
                                  weights[o4 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a5,
                                  in[j],
                                  weights[o5 + j],
                                  in[j + 1],
                                  weights[o5 + j + 1],
                                  in[j + 2],
                                  weights[o5 + j + 2],
                                  in[j + 3],
                                  weights[o5 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a6,
                                  in[j],
                                  weights[o6 + j],
                                  in[j + 1],
                                  weights[o6 + j + 1],
                                  in[j + 2],
                                  weights[o6 + j + 2],
                                  in[j + 3],
                                  weights[o6 + j + 3]);
      m256_add_dpbusd_epi32x4_asm(&a7,
                                  in[j],
                                  weights[o7 + j],
                                  in[j + 1],
                                  weights[o7 + j + 1],
                                  in[j + 2],
                                  weights[o7 + j + 2],
                                  in[j + 3],
                                  weights[o7 + j + 3]);
      // m256_add_dpbusd_epi32x2_asm(&a0, in[j], weights[o0 + j], in[j + 1], weights[o0 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a1, in[j], weights[o1 + j], in[j + 1], weights[o1 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a2, in[j], weights[o2 + j], in[j + 1], weights[o2 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a3, in[j], weights[o3 + j], in[j + 1], weights[o3 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a4, in[j], weights[o4 + j], in[j + 1], weights[o4 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a5, in[j], weights[o5 + j], in[j + 1], weights[o5 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a6, in[j], weights[o6 + j], in[j + 1], weights[o6 + j + 1]);
      // m256_add_dpbusd_epi32x2_asm(&a7, in[j], weights[o7 + j], in[j + 1], weights[o7 + j + 1]);
      // a0 = _mm256_add_epi32(a0, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o0 + j])));
      // a1 = _mm256_add_epi32(a1, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o1 + j])));
      // a2 = _mm256_add_epi32(a2, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o2 + j])));
      // a3 = _mm256_add_epi32(a3, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o3 + j])));
      // a4 = _mm256_add_epi32(a4, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o4 + j])));
      // a5 = _mm256_add_epi32(a5, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o5 + j])));
      // a6 = _mm256_add_epi32(a6, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o6 + j])));
      // a7 = _mm256_add_epi32(a7, _mm256_madd_epi16(one, _mm256_maddubs_epi16(in[j], weights[o7 + j])));
    }

    a0 = _mm256_hadd_epi32(a0, a1);
    a2 = _mm256_hadd_epi32(a2, a3);
    a0 = _mm256_hadd_epi32(a0, a2);

    a4 = _mm256_hadd_epi32(a4, a5);
    a6 = _mm256_hadd_epi32(a6, a7);
    a4 = _mm256_hadd_epi32(a4, a6);

    const __m128i a0hi = _mm256_castsi256_si128(a0);
    const __m128i a0lo = _mm256_extracti128_si256(a0, 1);
    const __m128i t0   = _mm_add_epi32(a0hi, a0lo);

    const __m128i a4hi = _mm256_castsi256_si128(a4);
    const __m128i a4lo = _mm256_extracti128_si256(a4, 1);
    const __m128i t4   = _mm_add_epi32(a4hi, a4lo);

    __m256i sum = _mm256_inserti128_si256(_mm256_castsi128_si256(t0), t4, 1);

    sum = _mm256_add_epi32(sum, biases[i]);
    sum = _mm256_max_epi32(sum, _mm256_setzero_si256());

    out[i] = _mm256_cvtepi32_ps(sum);
  }
}

INLINE void L2AffineReLU(float* dest, float* src, float* srcWeights, float* srcBiases) {
  const size_t IN_WIDTH   = sizeof(__m256) / sizeof(float);
  const size_t IN_CHUNKS  = N_L2 / IN_WIDTH;
  const size_t OUT_CC     = 8;
  const size_t OUT_CHUNKS = N_L3 / OUT_CC;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) srcWeights;
  const __m256* biases  = (__m256*) srcBiases;
  __m256* out           = (__m256*) dest;

  for (size_t i = 0; i < OUT_CHUNKS; i++) {
    __m256 a0 = _mm256_setzero_ps();
    __m256 a1 = _mm256_setzero_ps();
    __m256 a2 = _mm256_setzero_ps();
    __m256 a3 = _mm256_setzero_ps();
    __m256 a4 = _mm256_setzero_ps();
    __m256 a5 = _mm256_setzero_ps();
    __m256 a6 = _mm256_setzero_ps();
    __m256 a7 = _mm256_setzero_ps();

    const int o0 = (OUT_CC * i) * IN_CHUNKS;
    const int o1 = (OUT_CC * i + 1) * IN_CHUNKS;
    const int o2 = (OUT_CC * i + 2) * IN_CHUNKS;
    const int o3 = (OUT_CC * i + 3) * IN_CHUNKS;
    const int o4 = (OUT_CC * i + 4) * IN_CHUNKS;
    const int o5 = (OUT_CC * i + 5) * IN_CHUNKS;
    const int o6 = (OUT_CC * i + 6) * IN_CHUNKS;
    const int o7 = (OUT_CC * i + 7) * IN_CHUNKS;

    for (size_t j = 0; j < IN_CHUNKS; j++) {
      a0 = _mm256_fmadd_ps(in[j], weights[o0 + j], a0);
      a1 = _mm256_fmadd_ps(in[j], weights[o1 + j], a1);
      a2 = _mm256_fmadd_ps(in[j], weights[o2 + j], a2);
      a3 = _mm256_fmadd_ps(in[j], weights[o3 + j], a3);
      a4 = _mm256_fmadd_ps(in[j], weights[o4 + j], a4);
      a5 = _mm256_fmadd_ps(in[j], weights[o5 + j], a5);
      a6 = _mm256_fmadd_ps(in[j], weights[o6 + j], a6);
      a7 = _mm256_fmadd_ps(in[j], weights[o7 + j], a7);
    }

    a0 = _mm256_hadd_ps(a0, a1);
    a2 = _mm256_hadd_ps(a2, a3);
    a0 = _mm256_hadd_ps(a0, a2);

    a4 = _mm256_hadd_ps(a4, a5);
    a6 = _mm256_hadd_ps(a6, a7);
    a4 = _mm256_hadd_ps(a4, a6);

    const __m128 a0hi = _mm256_castps256_ps128(a0);
    const __m128 a0lo = _mm256_extractf128_ps(a0, 1);
    const __m128 t0   = _mm_add_ps(a0hi, a0lo);

    const __m128 a4hi = _mm256_castps256_ps128(a4);
    const __m128 a4lo = _mm256_extractf128_ps(a4, 1);
    const __m128 t4   = _mm_add_ps(a4hi, a4lo);

    __m256 sum = _mm256_insertf128_ps(_mm256_castps128_ps256(t0), t4, 1);

    sum    = _mm256_add_ps(sum, biases[i]);
    out[i] = _mm256_max_ps(sum, _mm256_setzero_ps());
  }
}

INLINE float L3Transform(float* src, float* srcWeights, float bias) {
  const size_t WIDTH  = sizeof(__m256) / sizeof(float);
  const size_t CHUNKS = N_L3 / WIDTH;

  const __m256* in      = (__m256*) src;
  const __m256* weights = (__m256*) srcWeights;

  __m256 a0 = _mm256_setzero_ps();
  for (size_t i = 0; i < CHUNKS; i++)
    a0 = _mm256_fmadd_ps(in[i], weights[i], a0);

  const __m128 a0lo = _mm256_castps256_ps128(a0);
  const __m128 a0hi = _mm256_extractf128_ps(a0, 1);
  const __m128 a4   = _mm_add_ps(a0lo, a0hi);
  const __m128 a2   = _mm_add_ps(a4, _mm_movehl_ps(a4, a4));
  const __m128 a1   = _mm_add_ss(a2, _mm_shuffle_ps(a2, a2, 0x1));

  return _mm_cvtss_f32(a1) + bias;
}

#define NUM_REGS 16

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

int Propagate(Accumulator* accumulator, const int stm) {
  uint8_t x0[N_L1] ALIGN;
  float x1[N_L2] ALIGN;
  float x2[N_L3] ALIGN;

  InputReLU(x0, accumulator, stm);
  L1AffineReLU(x1, x0, L1_WEIGHTS, L1_BIASES);
  L2AffineReLU(x2, x1, L2_WEIGHTS, L2_BIASES);
  return L3Transform(x2, OUTPUT_WEIGHTS, OUTPUT_BIAS) / 32.0;
}

int Predict(Board* board) {
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);

  return board->stm == WHITE ? Propagate(board->accumulators, WHITE) : Propagate(board->accumulators, BLACK);
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
                            sizeof(int8_t) * N_L1 * N_L2 +            // input biases
                            sizeof(int32_t) * N_L2 +                  // input biases
                            sizeof(float) * N_L2 * N_L3 +             // input biases
                            sizeof(float) * N_L3 +                    // input biases
                            sizeof(float) * N_L3 +                    // output weights
                            sizeof(float);                            // output bias

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

  memcpy(L2_WEIGHTS, &in[offset], N_L2 * N_L3 * sizeof(float));
  offset += N_L2 * N_L3 * sizeof(float);
  memcpy(L2_BIASES, &in[offset], N_L3 * sizeof(float));
  offset += N_L3 * sizeof(float);

  memcpy(OUTPUT_WEIGHTS, &in[offset], N_L3 * N_OUTPUT * sizeof(float));
  offset += N_L3 * N_OUTPUT * sizeof(float);
  memcpy(&OUTPUT_BIAS, &in[offset], sizeof(float));
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
