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

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#include "types.h"

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define INLINE static inline __attribute__((always_inline))

#define LoadRlx(x) atomic_load_explicit(&(x), memory_order_relaxed)

long GetTimeMS();

INLINE void* AlignedMalloc(uint64_t size) {
  void* mem  = malloc(size + ALIGN_ON + sizeof(void*));
  void** ptr = (void**) ((uintptr_t) (mem + ALIGN_ON + sizeof(void*)) & ~(ALIGN_ON - 1));
  ptr[-1]    = mem;
  return ptr;
}

INLINE void AlignedFree(void* ptr) {
  free(((void**) ptr)[-1]);
}

INLINE uint64_t mulHi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(_WIN64)
  __extension__ typedef unsigned __int128 uint128;
  return ((uint128) a * (uint128) b) >> 64;
#else
  uint64_t aL = (uint32_t) a, aH = a >> 32;
  uint64_t bL = (uint32_t) b, bH = b >> 32;
  uint64_t c1 = (aL * bL) >> 32;
  uint64_t c2 = aH * bL + c1;
  uint64_t c3 = aL * bH + (uint32_t) c2;
  return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

#endif