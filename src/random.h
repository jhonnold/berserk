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

#ifndef RANDOM_H
#define RANDOM_H

#include <inttypes.h>

#include "util.h"

uint64_t rotate(uint64_t v, uint8_t s);
uint64_t RandomUInt64();
void SeedRandom(uint64_t seed);
uint64_t RandomMagic();

// 42-bit key input -> 32-bit hash output
// https://cgi.cse.unsw.edu.au/~reports/papers/201703.pdf
INLINE uint32_t MurmurHash(uint64_t key) {
  key ^= key >> 33;
  key *= 0xff51afd7ed558ccdull;
  key ^= key >> 33;
  key *= 0xc4ceb9fe1a85ec53ull;
  key ^= key >> 33;

  return key;
}

#endif