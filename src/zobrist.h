#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <inttypes.h>

extern uint64_t zobristPieces[12][64];
extern uint64_t zobristEpKeys[64];
extern uint64_t zobristCastleKeys[16];
extern uint64_t zobristSideKey;

void initZobristKeys();

#endif