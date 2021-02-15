#include "random.h"

uint32_t randomState = 234;

uint32_t randomInt() {
  uint32_t result = randomState;

  result ^= (result << 13);
  result ^= (result >> 17);
  result ^= (result << 5);

  randomState = result;

  return result;
}

uint64_t randomLong() {
  uint64_t n1 = randomInt() & 0xFFFF;
  uint64_t n2 = randomInt() & 0xFFFF;
  uint64_t n3 = randomInt() & 0xFFFF;
  uint64_t n4 = randomInt() & 0xFFFF;

  return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

uint64_t randomMagic() { return randomLong() & randomLong() & randomLong(); }