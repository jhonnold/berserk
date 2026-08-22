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

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#include "bits.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"

const int DEPTH_OFFSET = -2;

// Global TT
TTTable TT = {0};

size_t TTInit(int mb) {
  if (TT.mem)
    TTFree();

  uint64_t size = (uint64_t) mb * MEGABYTE;

#if defined(__linux__)
  const size_t alignment = 2 * MEGABYTE;
#else
  const size_t alignment = 4096;
#endif

  TT.mem = AlignedMalloc(size, alignment);

#if defined(MADV_HUGEPAGE)
  madvise(TT.mem, size, MADV_HUGEPAGE);
#endif

  TT.buckets = (TTBucket*) TT.mem;
  TT.count   = size / sizeof(TTBucket);

  TTClear();
  return size;
}

void TTFree() {
  AlignedFree(TT.mem);
}

void TTClearPart(int idx) {
  int count = Threads.count;

  const uint64_t size   = TT.count * sizeof(TTBucket);
  const uint64_t slice  = (size + count - 1) / count;
  const uint64_t blocks = (slice + 2 * MEGABYTE - 1) / (2 * MEGABYTE);
  const uint64_t begin  = Min(size, idx * blocks * 2 * MEGABYTE);
  const uint64_t end    = Min(size, begin + blocks * 2 * MEGABYTE);

  memset(TT.buckets + begin / sizeof(TTBucket), 0, end - begin);
}

inline void TTClear() {
  for (int i = 0; i < Threads.count; i++)
    ThreadWake(Threads.threads[i], THREAD_TT_CLEAR);
  for (int i = 0; i < Threads.count; i++)
    ThreadWaitUntilSleep(Threads.threads[i]);
}

inline void TTUpdate() {
  TT.age += AGE_INC;
}

int TTFull() {
  int c = 0;

  for (int i = 0; i < 1000; i++)
    for (int j = 0; j < BUCKET_SIZE; j++)
      c += TT.buckets[i].entries[j].depth && (TT.buckets[i].entries[j].agePvBound & AGE_MASK) == TT.age;

  return c / BUCKET_SIZE;
}
