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

#ifndef UTIL_H
#define UTIL_H

#include <stdatomic.h>
#include <stdlib.h>

#include "types.h"

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define INLINE static inline __attribute__((always_inline))

#define LoadRlx(x) atomic_load_explicit(&(x), memory_order_relaxed)
#define IncRlx(x)  atomic_fetch_add_explicit(&(x), 1, memory_order_relaxed)

long GetTimeMS();

INLINE void* AlignedMalloc(uint64_t size, const size_t on) {
  void* mem  = malloc(size + on + sizeof(void*));
  void** ptr = (void**) ((uintptr_t) (mem + on + sizeof(void*)) & ~(on - 1));
  ptr[-1]    = mem;
  return ptr;
}

INLINE void AlignedFree(void* ptr) {
  free(((void**) ptr)[-1]);
}

INLINE float FastLog2(float x) {
  // https://stackoverflow.com/questions/9411823/fast-log2float-x-implementation-c/9411984#9411984

  union {
    int32_t i32;
    float f;
  } u;

  u.f          = x;
  float result = (float) (((u.i32 >> 23) & 255) - 128);
  u.i32 &= ~(255 << 23);
  u.i32 += 127 << 23;
  result += ((-0.33333333f) * u.f + 2.0f) * u.f - 0.66666666f;
  return result;
}

#endif