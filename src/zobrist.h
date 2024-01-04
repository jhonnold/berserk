// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"
#include "move.h"
#include "types.h"
#include "util.h"

extern uint64_t ZOBRIST_PIECES[12][64];
extern uint64_t ZOBRIST_EP_KEYS[64];
extern uint64_t ZOBRIST_CASTLE_KEYS[16];
extern uint64_t ZOBRIST_SIDE_KEY;

void InitZobristKeys();
uint64_t Zobrist(Board* board);
uint64_t PawnZobrist(Board* board);

INLINE uint64_t KeyAfter(Board* board, const Move move) {
  if (!move)
    return board->zobrist ^ ZOBRIST_SIDE_KEY;

  const int from   = From(move);
  const int to     = To(move);
  const int moving = Moving(move);

  uint64_t newKey = board->zobrist ^ ZOBRIST_SIDE_KEY ^ ZOBRIST_PIECES[moving][from] ^ ZOBRIST_PIECES[moving][to];

  if (board->squares[to] != NO_PIECE)
    newKey ^= ZOBRIST_PIECES[board->squares[to]][to];

  return newKey;
}

// 42-bit key input -> 32-bit hash output
// https://cgi.cse.unsw.edu.au/~reports/papers/201703.pdf
INLINE uint32_t MurmurHash(uint64_t key) {
  key ^= key >> 33;
  key *= 0xff51afd7ed558ccdull;
  key ^= key >> 33;
  key *= 0xc4ceb9fe1a85ec53ull;
  key ^= key >> 33;

  return key;
}

#endif