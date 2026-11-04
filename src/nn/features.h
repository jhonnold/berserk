// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#ifndef FEATURES_H
#define FEATURES_H

#include "../board.h"
#include "../types.h"
#include "../util.h"

// Input feature layout: king buckets and the per view square/piece indexing

extern const uint16_t KING_BUCKETS[64];

INLINE Bool MoveRequiresRefresh(Piece piece, Square from, Square to) {
  if (TypeOf(piece) != KING)
    return 0;

  if ((from & 4) != (to & 4))
    return 1;
  return KING_BUCKETS[from] != KING_BUCKETS[to];
}

// xor a square with this to see it from the view, mirrored so the king is on files e-h
INLINE int FeatureFlip(Square kingsq, const Color view) {
  return (7 * !(kingsq & 4)) ^ (56 * view);
}

INLINE int FeatureIdx(Piece piece, Square sq, Square kingsq, const Color view) {
  int oP     = 6 * ((piece ^ view) & 0x1) + TypeOf(piece);
  Square oK  = FeatureFlip(kingsq, view) ^ kingsq;
  Square oSq = FeatureFlip(kingsq, view) ^ sq;

  return KING_BUCKETS[oK] * 12 * 64 + oP * 64 + oSq;
}

#endif
