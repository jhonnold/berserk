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

#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
#define UNROLL        512
#define NUM_REGS      16
#define regi_t        __m512i
#define regi_load     _mm512_load_si512
#define regi_sub      _mm512_sub_epi16
#define regi_add      _mm512_add_epi16
#define regi_store    _mm512_store_si512
#define regi_load8(a) _mm512_cvtepi8_epi16(_mm256_load_si256((const __m256i*) (a)))
#define THREAT_TILE   16
#elif defined(__AVX2__)
#include <immintrin.h>
#define UNROLL        256
#define NUM_REGS      16
#define regi_t        __m256i
#define regi_load     _mm256_load_si256
#define regi_sub      _mm256_sub_epi16
#define regi_add      _mm256_add_epi16
#define regi_store    _mm256_store_si256
#define regi_load8(a) _mm256_cvtepi8_epi16(_mm_load_si128((const __m128i*) (a)))
#define THREAT_TILE   8
#elif defined(__SSE4_1__)
#include <immintrin.h>
#define UNROLL        128
#define NUM_REGS      16
#define regi_t        __m128i
#define regi_load     _mm_load_si128
#define regi_sub      _mm_sub_epi16
#define regi_add      _mm_add_epi16
#define regi_store    _mm_store_si128
#define regi_load8(a) _mm_cvtepi8_epi16(_mm_loadl_epi64((const __m128i*) (a)))
#define THREAT_TILE   8
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define UNROLL        128
#define NUM_REGS      16
#define regi_t        int16x8_t
#define regi_load(a)  vld1q_s16((int16_t*) (a))
#define regi_sub(a,   b)   vsubq_s16(a, b)
#define regi_add(a,   b)   vaddq_s16(a, b)
#define regi_store(a, b) vst1q_s16((int16_t*) (a), b)
#define regi_load8(a) vmovl_s8(vld1_s8((const int8_t*) (a)))
#define THREAT_TILE   16
#else
#define UNROLL        16
#define NUM_REGS      16
#define regi_t        acc_t
#define regi_load(a)  (*(a))
#define regi_sub(a,   b)   ((a) - (b))
#define regi_add(a,   b)   ((a) + (b))
#define regi_store(a, b) (*(a) = (b))
#define regi_load8(a)    ((acc_t) (a)[0])
#define THREAT_TILE      16
#endif

#endif
