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

extern Score PHASE_MULTIPLIERS[5];

extern int STATIC_MATERIAL_VALUE[7];

extern TScore PAWN_PSQT[32];
extern TScore KNIGHT_PSQT[32];
extern TScore BISHOP_PSQT[32];
extern TScore ROOK_PSQT[32];
extern TScore QUEEN_PSQT[32];
extern TScore KING_PSQT[32];
extern TScore KNIGHT_POST_PSQT[32];
extern TScore BISHOP_POST_PSQT[32];
extern TScore MATERIAL_VALUES[7];

extern TScore KNIGHT_OUTPOST_REACHABLE;
extern TScore BISHOP_OUTPOST_REACHABLE;

extern TScore KNIGHT_MOBILITIES[9];
extern TScore BISHOP_MOBILITIES[14];
extern TScore ROOK_MOBILITIES[15];
extern TScore QUEEN_MOBILITIES[28];

extern TScore DOUBLED_PAWN;
extern TScore OPPOSED_ISOLATED_PAWN;
extern TScore OPEN_ISOLATED_PAWN;
extern TScore BACKWARDS_PAWN;
extern TScore CONNECTED_PAWN[8];
extern TScore PASSED_PAWN[8];
extern TScore PASSED_PAWN_ADVANCE_DEFENDED;
extern TScore PASSED_PAWN_EDGE_DISTANCE;
extern TScore PASSED_PAWN_KING_PROXIMITY;

extern TScore ROOK_OPEN_FILE;
extern TScore ROOK_SEMI_OPEN;
extern TScore ROOK_SEVENTH_RANK;
extern TScore ROOK_OPPOSITE_KING;
extern TScore ROOK_ADJACENT_KING;
extern TScore ROOK_TRAPPED;
extern TScore BISHOP_TRAPPED;
extern TScore BISHOP_PAIR;

extern TScore KNIGHT_THREATS[6];
extern TScore BISHOP_THREATS[6];
extern TScore ROOK_THREATS[6];
extern TScore KING_THREATS[6];

extern TScore TEMPO;

void InitPSQT();

Score GetPhase(Board* board);
Score Taper(Score mg, Score eg, Score phase);

Score ToScore(EvalData* data, Board* board);
int IsMaterialDraw(Board* board);
void EvaluateSide(Board* board, int side, EvalData* data);
void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData);
void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData);
Score Evaluate(Board* board);
int Scale(Board* board, int ss);
void PrintEvaluation(Board* board);

#endif