#include <stdio.h>

#include "types.h"
#include "bits.h"

void printBB(bb_t bb) {
  for (int i = 0; i < 64; i++) {
    if ((i & 7) == 0)
      printf(" %d ", 8 - (i >> 3));

    printf(" %d", getBit(bb, i) ? 1 : 0);

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
  printf(" Value: %llu\n\n", bb);
}