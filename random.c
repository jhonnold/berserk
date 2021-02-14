#include "random.h"

unsigned int randomState = 234;

unsigned int randomInt() {
  unsigned int result = randomState;

  result ^= (result << 13);
  result ^= (result >> 17);
  result ^= (result << 5);

  randomState = result;

  return result;
}

unsigned long randomLong() {
  unsigned long n1 = randomInt() & 0xFFFF;
  unsigned long n2 = randomInt() & 0xFFFF;
  unsigned long n3 = randomInt() & 0xFFFF;
  unsigned long n4 = randomInt() & 0xFFFF;

  return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

unsigned long randomMagic() {
  return randomLong() & randomLong() & randomLong();
}