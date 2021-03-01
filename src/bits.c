#include <inttypes.h>
#include <stdio.h>

#include "bits.h"
#include "board.h"
#include "types.h"

const BitBoard FILE_MASKS[8] = {A_FILE, B_FILE, C_FILE, D_FILE, E_FILE, F_FILE, G_FILE, H_FILE};
const BitBoard RANK_MASKS[8] = {RANK_8, RANK_7, RANK_6, RANK_5, RANK_4, RANK_3, RANK_2, RANK_1};
const BitBoard ADJACENT_FILE_MASKS[8] = {B_FILE,          A_FILE | C_FILE, B_FILE | D_FILE, C_FILE | E_FILE,
                                         D_FILE | F_FILE, E_FILE | G_FILE, F_FILE | H_FILE, G_FILE};
const BitBoard FORWARD_RANK_MASKS[2][8] = {{
                                               0ULL,
                                               RANK_8,
                                               RANK_8 | RANK_7,
                                               RANK_8 | RANK_7 | RANK_6,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4 | RANK_3,
                                               RANK_8 | RANK_7 | RANK_6 | RANK_5 | RANK_4 | RANK_3 | RANK_2,
                                           },
                                           {
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6 | RANK_7,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5,
                                               RANK_1 | RANK_2 | RANK_3 | RANK_4,
                                               RANK_1 | RANK_2 | RANK_3,
                                               RANK_1 | RANK_2,
                                               RANK_1,
                                               0ULL,
                                           }};

#ifndef POPCOUNT
inline int bits(BitBoard bb) {
  int c;
  for (c = 0; bb; bb &= bb - 1)
    c++;
  return c;
}
#endif

inline BitBoard shift(BitBoard bb, int dir) {
  return dir == N         ? bb >> 8
         : dir == S       ? bb << 8
         : dir == (N + N) ? bb >> 16
         : dir == (S + S) ? bb << 16
         : dir == W       ? (bb & ~A_FILE) >> 1
         : dir == E       ? (bb & ~H_FILE) << 1
         : dir == NE      ? (bb & ~H_FILE) >> 7
         : dir == SW      ? (bb & ~A_FILE) << 7
         : dir == NW      ? (bb & ~A_FILE) >> 9
         : dir == SE      ? (bb & ~H_FILE) << 9
                          : 0;
}

inline BitBoard fill(BitBoard initial, int direction) {
  switch (direction) {
  case S:
    initial |= (initial << 8);
    initial |= (initial << 16);
    return initial | (initial << 32);
  case N:
    initial |= (initial >> 8);
    initial |= (initial >> 16);
    return initial | (initial >> 32);
  default:
    return initial;
  }
}

void printBB(BitBoard bitboard) {
  for (int i = 0; i < 64; i++) {
    if (file(i) == 0)
      printf(" %d ", 8 - rank(i));

    printf(" %d", getBit(bitboard, i) ? 1 : 0);

    if (file(i) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
  printf(" Value: %" PRIu64 "\n\n", bitboard);
}