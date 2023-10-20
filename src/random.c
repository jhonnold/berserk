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
#include "random.h"

#include <inttypes.h>

// I dunno anything about random number generators, and had a bad one for a
// while Thanks to Martin Sedlák (author of Cheng) this one is really cool and
// works :) http://www.vlasak.biz/cheng/

static uint64_t keys[2];

void SeedRandom(uint64_t seed) {
  keys[0] = keys[1] = seed;

  for (int i = 0; i < 64; i++)
    RandomUInt64();
}

static uint64_t Rotate(uint64_t v, uint8_t s) {
  return (v >> s) | (v << (64 - s));
}

uint64_t RandomUInt64() {
  uint64_t tmp = keys[0];
  keys[0] += Rotate(keys[1] ^ 0xc5462216u ^ ((uint64_t) 0xcf14f4ebu << 32), 1);
  return keys[1] += Rotate(tmp ^ 0x75ecfc58u ^ ((uint64_t) 0x9576080cu << 32), 9);
}

// Magic's are combined to try and get one with few bits
uint64_t RandomMagic() {
  return RandomUInt64() & RandomUInt64() & RandomUInt64();
}