// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

#include "zobrist.h"

#include "bits.h"
#include "random.h"
#include "types.h"

uint64_t ZOBRIST_PIECES[15][64];
uint64_t ZOBRIST_EP_KEYS[64];
uint64_t ZOBRIST_CASTLE_KEYS[16];
uint64_t ZOBRIST_SIDE_KEY;

void InitZobristKeys() {

  for (int i = 0; i < 15; i++)
    for (int j = 0; j < 64; j++)
      ZOBRIST_PIECES[i][j] = RandomUInt64();

  for (int i = 0; i < 64; i++)
    ZOBRIST_EP_KEYS[i] = RandomUInt64();

  for (int i = 0; i < 16; i++)
    ZOBRIST_CASTLE_KEYS[i] = RandomUInt64();

  ZOBRIST_SIDE_KEY = RandomUInt64();
}

// Generate a Zobirst key for the current board state
uint64_t Zobrist(Board* board) {
  uint64_t hash = 0ULL;

  for (int color = WHITE; color <= BLACK; color++) {
    for (int pt = PAWN; pt <= KING; pt++) {
      const int piece = Piece(pt, color);

      BitBoard pcs = board->pieces[piece];
      while (pcs)
        hash ^= ZOBRIST_PIECES[piece][PopLSB(&pcs)];
    }
  }

  if (board->epSquare)
    hash ^= ZOBRIST_EP_KEYS[board->epSquare];

  hash ^= ZOBRIST_CASTLE_KEYS[board->castling];

  if (board->stm)
    hash ^= ZOBRIST_SIDE_KEY;

  return hash;
}
