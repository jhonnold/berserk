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

#ifndef SIMD_H
#define SIMD_H

#include "../types.h"
#include "../util.h"

// One register of each lane type: vi16_t holds the accumulator's int16 lanes,
// vu8_t the activations, vi8_t the int8 weights and vi32_t the int32 sums.
// Only NEON tells them apart. UNROLL, NUM_REGS and THREAT_TILE tile the
// accumulator updates.
#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
#define SIMD
#define UNROLL      512
#define NUM_REGS    16
#define THREAT_TILE 16

#define vi16_t              __m512i
#define vu8_t               __m512i
#define vi8_t               __m512i
#define vi32_t              __m512i
#define vi16_load           _mm512_load_si512
#define vi16_store          _mm512_store_si512
#define vi16_load8(a)       _mm512_cvtepi8_epi16(_mm256_load_si256((const __m256i*) (a)))
#define vi16_add            _mm512_add_epi16
#define vi16_sub            _mm512_sub_epi16
#define vi16_zero()         _mm512_setzero_si512()
#define vi16_set1(x)        _mm512_set1_epi16(x)
#define vi16_min(a, b)      _mm512_min_epi16(a, b)
#define vi16_max(a, b)      _mm512_max_epi16(a, b)
#define vi16_slli(a, n)     _mm512_slli_epi16(a, n)
#define vi16_mulhi(a, b)    _mm512_mulhi_epi16(a, b)
#define vi16_packus(a, b)   _mm512_packus_epi16(a, b)
#define vi8_bcast32(p)      _mm512_set1_epi32(*(p))
#define vi32_zero()         _mm512_setzero_si512()
#define vi32_load(p)        (*(const __m512i*) (p))
#define vi32_add(a, b)      _mm512_add_epi32(a, b)
#define vi32_nnz_mask(a)    _mm512_cmpgt_epi32_mask(a, _mm512_setzero_si512())
#define vi32_reduce(a)      _mm512_reduce_add_epi32(a)
#define MULHI_SHIFT         16
#define L1_UNROLL           4

INLINE void vi32_dpbusd(__m512i* acc, __m512i a, __m512i b) {
#if defined(__AVX512VNNI__)
  *acc = _mm512_dpbusd_epi32(*acc, a, b);
#else
  __m512i p0 = _mm512_maddubs_epi16(a, b);
  p0         = _mm512_madd_epi16(p0, _mm512_set1_epi16(1));
  *acc       = _mm512_add_epi32(*acc, p0);
#endif
}

INLINE void vi32_dpbusd_x2(__m512i* acc, __m512i a0, __m512i b0, __m512i a1, __m512i b1) {
#if defined(__AVX512VNNI__)
  *acc = _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(*acc, a0, b0), a1, b1);
#else
  __m512i p0 = _mm512_maddubs_epi16(a0, b0);
  __m512i p1 = _mm512_maddubs_epi16(a1, b1);

  p0   = _mm512_madd_epi16(_mm512_add_epi16(p0, p1), _mm512_set1_epi16(1));
  *acc = _mm512_add_epi32(*acc, p0);
#endif
}
#elif defined(__AVX2__)
#include <immintrin.h>
#define SIMD
#define UNROLL      256
#define NUM_REGS    16
#define THREAT_TILE 8

#define vi16_t              __m256i
#define vu8_t               __m256i
#define vi8_t               __m256i
#define vi32_t              __m256i
#define vi16_load           _mm256_load_si256
#define vi16_store          _mm256_store_si256
#define vi16_load8(a)       _mm256_cvtepi8_epi16(_mm_load_si128((const __m128i*) (a)))
#define vi16_add            _mm256_add_epi16
#define vi16_sub            _mm256_sub_epi16
#define vi16_zero()         _mm256_setzero_si256()
#define vi16_set1(x)        _mm256_set1_epi16(x)
#define vi16_min(a, b)      _mm256_min_epi16(a, b)
#define vi16_max(a, b)      _mm256_max_epi16(a, b)
#define vi16_slli(a, n)     _mm256_slli_epi16(a, n)
#define vi16_mulhi(a, b)    _mm256_mulhi_epi16(a, b)
#define vi16_packus(a, b)   _mm256_packus_epi16(a, b)
#define vi8_bcast32(p)      _mm256_set1_epi32(*(p))
#define vi32_zero()         _mm256_setzero_si256()
#define vi32_load(p)        (*(const __m256i*) (p))
#define vi32_add(a, b)      _mm256_add_epi32(a, b)
#define vi32_nnz_mask(a)    _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(a, _mm256_setzero_si256())))
#define MULHI_SHIFT         16
#define L1_UNROLL           4

INLINE void vi32_dpbusd(__m256i* acc, __m256i a, __m256i b) {
#if defined(__AVXVNNI__)
  *acc = _mm256_dpbusd_avx_epi32(*acc, a, b);
#else
  __m256i p0 = _mm256_maddubs_epi16(a, b);
  p0         = _mm256_madd_epi16(p0, _mm256_set1_epi16(1));
  *acc       = _mm256_add_epi32(*acc, p0);
#endif
}

INLINE void vi32_dpbusd_x2(__m256i* acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
#if defined(__AVXVNNI__)
  *acc = _mm256_dpbusd_avx_epi32(_mm256_dpbusd_avx_epi32(*acc, a0, b0), a1, b1);
#else
  __m256i p0 = _mm256_maddubs_epi16(a0, b0);
  __m256i p1 = _mm256_maddubs_epi16(a1, b1);

  p0   = _mm256_madd_epi16(_mm256_add_epi16(p0, p1), _mm256_set1_epi16(1));
  *acc = _mm256_add_epi32(*acc, p0);
#endif
}

INLINE int32_t vi32_reduce(__m256i a) {
  const __m128i a4 = _mm_add_epi32(_mm256_castsi256_si128(a), _mm256_extracti128_si256(a, 1));
  const __m128i a2 = _mm_add_epi32(a4, _mm_shuffle_epi32(a4, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1);
}
#elif defined(__SSE4_1__)
#include <immintrin.h>
#define SIMD
#define UNROLL      128
#define NUM_REGS    16
#define THREAT_TILE 8

#define vi16_t              __m128i
#define vu8_t               __m128i
#define vi8_t               __m128i
#define vi32_t              __m128i
#define vi16_load           _mm_load_si128
#define vi16_store          _mm_store_si128
#define vi16_load8(a)       _mm_cvtepi8_epi16(_mm_loadl_epi64((const __m128i*) (a)))
#define vi16_add            _mm_add_epi16
#define vi16_sub            _mm_sub_epi16
#define vi16_zero()         _mm_setzero_si128()
#define vi16_set1(x)        _mm_set1_epi16(x)
#define vi16_min(a, b)      _mm_min_epi16(a, b)
#define vi16_max(a, b)      _mm_max_epi16(a, b)
#define vi16_slli(a, n)     _mm_slli_epi16(a, n)
#define vi16_mulhi(a, b)    _mm_mulhi_epi16(a, b)
#define vi16_packus(a, b)   _mm_packus_epi16(a, b)
#define vi8_bcast32(p)      _mm_set1_epi32(*(p))
#define vi32_zero()         _mm_setzero_si128()
#define vi32_load(p)        (*(const __m128i*) (p))
#define vi32_add(a, b)      _mm_add_epi32(a, b)
#define vi32_nnz_mask(a)    _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(a, _mm_setzero_si128())))
#define vi32_nnz_mask2(a, b) (vi32_nnz_mask(a) | (vi32_nnz_mask(b) << 4))
#define MULHI_SHIFT         16
// 16 xmm registers have no room for a second set of sums
#define L1_UNROLL           2

INLINE void vi32_dpbusd(__m128i* acc, __m128i a, __m128i b) {
  __m128i p0 = _mm_maddubs_epi16(a, b);
  p0         = _mm_madd_epi16(p0, _mm_set1_epi16(1));
  *acc       = _mm_add_epi32(*acc, p0);
}

INLINE void vi32_dpbusd_x2(__m128i* acc, __m128i a0, __m128i b0, __m128i a1, __m128i b1) {
  __m128i p0 = _mm_maddubs_epi16(a0, b0);
  __m128i p1 = _mm_maddubs_epi16(a1, b1);

  p0   = _mm_madd_epi16(_mm_add_epi16(p0, p1), _mm_set1_epi16(1));
  *acc = _mm_add_epi32(*acc, p0);
}

INLINE int32_t vi32_reduce(__m128i a) {
  const __m128i a2 = _mm_add_epi32(a, _mm_shuffle_epi32(a, 0x4E));
  const __m128i a1 = _mm_add_epi32(a2, _mm_shuffle_epi32(a2, 0xB1));

  return _mm_cvtsi128_si32(a1);
}
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define SIMD
#define UNROLL      128
#define NUM_REGS    16
#define THREAT_TILE 16

#define vi16_t              int16x8_t
#define vu8_t               uint8x16_t
#define vi8_t               int8x16_t
#define vi32_t              int32x4_t
#define vi16_load(a)        vld1q_s16((int16_t*) (a))
#define vi16_store(a, b)    vst1q_s16((int16_t*) (a), b)
#define vi16_load8(a)       vmovl_s8(vld1_s8((const int8_t*) (a)))
#define vi16_add(a, b)      vaddq_s16(a, b)
#define vi16_sub(a, b)      vsubq_s16(a, b)
#define vi16_zero()         vdupq_n_s16(0)
#define vi16_set1(x)        vdupq_n_s16(x)
#define vi16_min(a, b)      vminq_s16(a, b)
#define vi16_max(a, b)      vmaxq_s16(a, b)
#define vi16_slli(a, n)     vshlq_n_s16(a, n)
// vqdmulh doubles the product, so the pre-shift is one less than the x86 mulhi path
#define vi16_mulhi(a, b)    vqdmulhq_s16(a, b)
#define vi16_packus(a, b)   vcombine_u8(vqmovun_s16(a), vqmovun_s16(b))
#define vi8_bcast32(p)      vreinterpretq_s8_s32(vld1q_dup_s32(p))
#define vi32_zero()         vdupq_n_s32(0)
#define vi32_load(p)        vld1q_s32(p)
#define vi32_add(a, b)      vaddq_s32(a, b)
#define vi32_reduce(a)      vaddvq_s32(a)
#define MULHI_SHIFT         15
#define L1_UNROLL           4

INLINE uint32_t vi32_nnz_mask2(uint8x16_t a, uint8x16_t b) {
  const uint16_t lanes[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const uint32x4_t c0     = vreinterpretq_u32_u8(a);
  const uint32x4_t c1     = vreinterpretq_u32_u8(b);
  const uint16x8_t nz     = vcombine_u16(vmovn_u32(vtstq_u32(c0, c0)), vmovn_u32(vtstq_u32(c1, c1)));

  return vaddvq_u16(vandq_u16(nz, vld1q_u16(lanes)));
}

// Every uint8 activation is capped at 127, so the signed dot product is exact.
INLINE void vi32_dpbusd(int32x4_t* acc, int8x16_t a, int8x16_t b) {
#if defined(__ARM_FEATURE_DOTPROD)
  *acc = vdotq_s32(*acc, a, b);
#else
  const int16x8_t p0 = vmull_s8(vget_low_s8(a), vget_low_s8(b));
  const int16x8_t p1 = vmull_high_s8(a, b);

  *acc = vpadalq_s16(*acc, vpaddq_s16(p0, p1));
#endif
}

INLINE void vi32_dpbusd_x2(int32x4_t* acc, int8x16_t a0, int8x16_t b0, int8x16_t a1, int8x16_t b1) {
#if defined(__ARM_FEATURE_DOTPROD)
  *acc = vdotq_s32(vdotq_s32(*acc, a0, b0), a1, b1);
#else
  const int16x8_t p0 = vmull_s8(vget_low_s8(a0), vget_low_s8(b0));
  const int16x8_t p1 = vmull_high_s8(a0, b0);
  const int16x8_t p2 = vmull_s8(vget_low_s8(a1), vget_low_s8(b1));
  const int16x8_t p3 = vmull_high_s8(a1, b1);

  *acc = vpadalq_s16(vpadalq_s16(*acc, vpaddq_s16(p0, p1)), vpaddq_s16(p2, p3));
#endif
}
#else
// No SIMD: the accumulator updates run one lane at a time, the network layers
// take the scalar branch of each method.
#define UNROLL      16
#define NUM_REGS    16
#define THREAT_TILE 16

#define vi16_t           acc_t
#define vi16_load(a)     (*(a))
#define vi16_store(a, b) (*(a) = (b))
#define vi16_load8(a)    ((acc_t) (a)[0])
#define vi16_add(a, b)   ((a) + (b))
#define vi16_sub(a, b)   ((a) - (b))
#endif

// The activations after L1 and L2 run 128 bits wide on every arch, and the NNZ
// index list is written 8 indices at a time.
#if defined(__SSE4_1__)
#define v128_32_t           __m128i
#define v128_16_t           __m128i
#define v128_8_t            __m128i
#define v128_packs32(a, b)  _mm_packs_epi32(a, b)
#define v128_max16(a, b)    _mm_max_epi16(a, b)
#define v128_zero16()       _mm_setzero_si128()
#define v128_clamp(a0, a1)  _mm_max_epi8(_mm_packs_epi16(_mm_srai_epi16(a0, 6), _mm_srai_epi16(a1, 6)), _mm_setzero_si128())
// statements, not one expression: gcc evaluates arguments right to left
INLINE __m128i v128_squareq(__m128i a0, __m128i a1) {
  const __m128i s0 = _mm_srli_epi16(_mm_mulhi_epi16(a0, a0), 3);
  const __m128i s1 = _mm_srli_epi16(_mm_mulhi_epi16(a1, a1), 3);
  return _mm_packs_epi16(s0, s1);
}

#define nnz_t           __m128i
#define nnz_zero()      _mm_setzero_si128()
#define nnz_set1(x)     _mm_set1_epi16(x)
#define nnz_add(a, b)   _mm_add_epi16(a, b)
#define nnz_load(p)     _mm_loadu_si128((const __m128i*) (p))
#define nnz_store(p, a) _mm_storeu_si128((__m128i*) (p), a)
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#define v128_32_t           int32x4_t
#define v128_16_t           int16x8_t
#define v128_8_t            int8x16_t
#define v128_packs32(a, b)  vcombine_s16(vqmovn_s32(a), vqmovn_s32(b))
#define v128_max16(a, b)    vmaxq_s16(a, b)
#define v128_zero16()       vdupq_n_s16(0)
#define v128_clamp(a0, a1)  vmaxq_s8(vcombine_s8(vqshrn_n_s16(a0, 6), vqshrn_n_s16(a1, 6)), vdupq_n_s8(0))
// vqdmulh is (2 * a * a) >> 16, one more shift recovers the x86 mulhi >> 3
#define v128_squareq(a0, a1) \
  vcombine_s8(vqshrn_n_s16(vqdmulhq_s16(a0, a0), 4), vqshrn_n_s16(vqdmulhq_s16(a1, a1), 4))

#define nnz_t           uint16x8_t
#define nnz_zero()      vdupq_n_u16(0)
#define nnz_set1(x)     vdupq_n_u16(x)
#define nnz_add(a, b)   vaddq_u16(a, b)
#define nnz_load(p)     vld1q_u16(p)
#define nnz_store(p, a) vst1q_u16(p, a)
#endif

#endif
