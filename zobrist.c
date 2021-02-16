#include "zobrist.h"
#include "random.h"

void initZobristKeys() {
  for (int i = 0; i < 12; i++)
    for (int j = 0; j < 64; j++)
      zobristPieces[i][j] = randomLong();

  for (int i = 0; i < 64; i++)
    zobristEpKeys[i] = randomLong();

  for (int i = 0; i < 16; i++)
    zobristCastleKeys[i] = randomLong();

  zobristSideKey = randomLong();
}