#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <inttypes.h>

uint64_t zobristPieces[12][64];
uint64_t zobristEpKeys[64];
uint64_t zobristCastleKeys[16];
uint64_t zobristSideKey;

void initZobristKeys();

#endif