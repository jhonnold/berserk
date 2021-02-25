#include <inttypes.h>
#include <stdio.h>

#include "bits.h"
#include "types.h"

const BitBoard COLUMN_MASKS[8] = {A_FILE, B_FILE, C_FILE, D_FILE, E_FILE, F_FILE, G_FILE, H_FILE};
// These are intentionally in reverse as 0 is a8
const BitBoard RANK_MASKS[8] = {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1};

#ifndef POPCOUNT
inline int bits(BitBoard bb) {
  int c;
  for (c = 0; bb; bb &= bb - 1)
    c++;
  return c;
}
#endif

inline BitBoard shift(BitBoard bb, int dir) {
  return dir == -8    ? bb >> 8
         : dir == 8   ? bb << 8
         : dir == -16 ? bb >> 16
         : dir == 16  ? bb << 16
         : dir == -1  ? (bb & ~A_FILE) >> 1
         : dir == 1   ? (bb & ~H_FILE) << 1
         : dir == -7  ? (bb & ~H_FILE) >> 7
         : dir == 7   ? (bb & ~A_FILE) << 7
         : dir == -9  ? (bb & ~A_FILE) >> 9
         : dir == 9   ? (bb & ~H_FILE) << 9
                      : 0;
}

inline BitBoard fill(BitBoard initial, int direction) {
  if (direction == 8) {
    initial |= (initial << 8);
    initial |= (initial << 16);
    return initial | (initial << 32);
  } else if (direction == -8) {
    initial |= (initial >> 8);
    initial |= (initial >> 16);
    return initial | (initial >> 32);
  }

  return initial;
}

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