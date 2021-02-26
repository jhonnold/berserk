#include "zobrist.h"
#include "random.h"

uint64_t ZOBRIST_PIECES[12][64];
uint64_t ZOBRIST_EP_KEYS[64];
uint64_t ZOBRIST_CASTLE_KEYS[16];
uint64_t ZOBRIST_SIDE_KEY;

void initZobristKeys() {
  for (int i = 0; i < 12; i++)
    for (int j = 0; j < 64; j++)
      ZOBRIST_PIECES[i][j] = randomLong();

  for (int i = 0; i < 64; i++)
    ZOBRIST_EP_KEYS[i] = randomLong();

  for (int i = 0; i < 16; i++)
    ZOBRIST_CASTLE_KEYS[i] = randomLong();

  ZOBRIST_SIDE_KEY = randomLong();
}