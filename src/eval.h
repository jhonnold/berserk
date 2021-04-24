// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#ifndef EVAL_H
#define EVAL_H

#include <inttypes.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

extern const Score PHASE_MULTIPLIERS[5];

extern const int STATIC_MATERIAL_VALUE[7];

extern Score PAWN_PSQT[32];
extern Score KNIGHT_PSQT[32];
extern Score BISHOP_PSQT[32];
extern Score ROOK_PSQT[32];
extern Score QUEEN_PSQT[32];
extern Score KING_PSQT[32];
extern Score KNIGHT_POST_PSQT[32];
extern Score BISHOP_POST_PSQT[32];
extern Score MATERIAL_VALUES[7];

extern Score KNIGHT_OUTPOST_REACHABLE;
extern Score BISHOP_OUTPOST_REACHABLE;

extern Score KNIGHT_MOBILITIES[9];
extern Score BISHOP_MOBILITIES[14];
extern Score ROOK_MOBILITIES[15];
extern Score QUEEN_MOBILITIES[28];

extern Score DOUBLED_PAWN;
extern Score OPPOSED_ISOLATED_PAWN;
extern Score OPEN_ISOLATED_PAWN;
extern Score BACKWARDS_PAWN;
extern Score CONNECTED_PAWN[8];
extern Score PASSED_PAWN[8];
extern Score PASSED_PAWN_ADVANCE_DEFENDED;
extern Score PASSED_PAWN_EDGE_DISTANCE;
extern Score PASSED_PAWN_KING_PROXIMITY;

extern Score ROOK_OPEN_FILE;
extern Score ROOK_SEMI_OPEN;
extern Score ROOK_SEVENTH_RANK;
extern Score ROOK_OPPOSITE_KING;
extern Score ROOK_ADJACENT_KING;
extern Score ROOK_TRAPPED;
extern Score BISHOP_TRAPPED;
extern Score BISHOP_PAIR;

extern Score KNIGHT_THREATS[6];
extern Score BISHOP_THREATS[6];
extern Score ROOK_THREATS[6];
extern Score KING_THREATS[6];

extern Score TEMPO;

extern Score PSQT[12][64];

void InitPSQT();

Score GetPhase(Board* board);
Score Taper(Score mg, Score eg, Score phase);

Score EvaluateKXK(Board* board);

Score Evaluate(Board* board);

uint8_t GetKPKBit(int bit);
int KPKIndex(int ssKing, int wsKing, int p, int stm);
int KPKDraw(int ss, int ssKing, int wsKing, int p, int stm);

int Scale(Board* board, int ss);

#endif