#include <stdint.h>

#include "random.h"

// I dunno anything about random number generators, and had a bad one for a while
// Thanks to Martin Sedlák (author of Cheng) this one is really cool and works :)
// http://www.vlasak.biz/cheng/

uint64_t keys[2];

inline uint64_t rotate(uint64_t v, uint8_t s) { return (v >> s) | (v << (64 - s)); }

inline uint64_t randomLong() {
  uint64_t tmp = keys[0];
  keys[0] += rotate(keys[1] ^ 0xc5462216u ^ ((uint64_t)0xcf14f4ebu << 32), 1);
  return keys[1] += rotate(tmp ^ 0x75ecfc58u ^ ((uint64_t)0x9576080cu << 32), 9);
}

void seedRandom(uint64_t seed) {
  keys[0] = keys[1] = seed;

  for (int i = 0; i < 64; i++)
    randomLong();
}

uint64_t randomMagic() { return randomLong() & randomLong() & randomLong(); }