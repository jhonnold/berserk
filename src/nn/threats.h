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

#ifndef THREATS_H
#define THREATS_H

#include "../bits.h"
#include "../board.h"
#include "../types.h"
#include "../util.h"

// from | to << 8 | attacker << 16 | victim << 20 | add << 31
#define DirtyFrom(d)     ((d) & 63)
#define DirtyTo(d)       (((d) >> 8) & 63)
#define DirtyAttacker(d) (((d) >> 16) & 15)
#define DirtyVictim(d)   (((d) >> 20) & 15)
#define DirtyAdd(d)      ((d) >> 31)

extern int8_t THREAT_WEIGHTS[N_THREATS * N_HIDDEN];

extern int32_t THREAT_PAIR[10][12];
extern uint16_t THREAT_OFFSET[10][64];
extern BitBoard THREAT_EMPTY[10][64];

void InitThreats();

int MovePiecesWithThreats(Board* board, Move move, int captured, uint32_t* dirty);

int ActiveThreats(Board* board, const int view, int* indices);

INLINE int ThreatIdx(int attacker, int from, int victim, int to, const int view, const int flip) {
  const int block = 5 * ((attacker ^ view) & 1) + (attacker >> 1);
  const int rel   = 6 * ((victim ^ view) & 1) + (victim >> 1);
  const int32_t e = THREAT_PAIR[block][rel];

  from ^= flip;
  to ^= flip;

  if (e < 0 || ((e & 1) && from < to))
    return -1;

  return (e >> 1) + THREAT_OFFSET[block][from] + BitCount(THREAT_EMPTY[block][from] & (Bit(to) - 1));
}

#endif
