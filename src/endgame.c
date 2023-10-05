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

#include "endgame.h"

#include "search.h"
#include "types.h"
#include "util.h"

int EvaluateKnownPositions(Board* board) {
  switch (board->piecesCounts) {
    // See IsMaterialDraw
    case 0x0:      // Kk
    case 0x100:    // KNk
    case 0x200:    // KNNk
    case 0x1000:   // Kkn
    case 0x2000:   // Kknn
    case 0x1100:   // KNkn
    case 0x10000:  // KBk
    case 0x100000: // Kkb
    case 0x11000:  // KBkn
    case 0x100100: // KNkb
    case 0x110000: // KBkb
      return 0;
  }

  return UNKNOWN;
}
