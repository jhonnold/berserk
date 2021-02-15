#include <inttypes.h>
#include <stdio.h>

#include "bits.h"
#include "types.h"

void printBB(BitBoard bitboard) {
  for (int i = 0; i < 64; i++) {
    if ((i & 7) == 0)
      printf(" %d ", 8 - (i >> 3));

    printf(" %d", getBit(bitboard, i) ? 1 : 0);

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
  printf(" Value: %" PRIu64 "\n\n", bitboard);
}