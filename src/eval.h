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

#include <stdlib.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))
#define psqtIdx(sq) ((rank((sq)) << 2) + (file((sq)) > 3 ? file((sq)) ^ 7 : file((sq))))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))

#define UNKNOWN INT32_MAX
extern EvalCoeffs C;

extern const int MAX_SCALE;
extern const int MAX_PHASE;
extern const Score PHASE_MULTIPLIERS[5];

extern const int STATIC_MATERIAL_VALUE[7];

extern const Score MATERIAL_VALUES[7];
extern const Score BISHOP_PAIR;

extern const Score PAWN_PSQT[2][32];
extern const Score KNIGHT_PSQT[2][32];
extern const Score BISHOP_PSQT[2][32];
extern const Score ROOK_PSQT[2][32];
extern const Score QUEEN_PSQT[2][32];
extern const Score KING_PSQT[2][32];

extern const Score KNIGHT_POST_PSQT[12];
extern const Score BISHOP_POST_PSQT[12];

extern const Score KNIGHT_MOBILITIES[9];
extern const Score BISHOP_MOBILITIES[14];
extern const Score ROOK_MOBILITIES[15];
extern const Score QUEEN_MOBILITIES[28];

extern const Score KNIGHT_OUTPOST_REACHABLE;
extern const Score BISHOP_OUTPOST_REACHABLE;
extern const Score BISHOP_TRAPPED;
extern const Score ROOK_TRAPPED;
extern const Score BAD_BISHOP_PAWNS;
extern const Score DRAGON_BISHOP;
extern const Score ROOK_OPEN_FILE;
extern const Score ROOK_SEMI_OPEN;

extern const Score DEFENDED_PAWN;
extern const Score DOUBLED_PAWN;
extern const Score OPPOSED_ISOLATED_PAWN;
extern const Score OPEN_ISOLATED_PAWN;
extern const Score BACKWARDS_PAWN;
extern const Score CONNECTED_PAWN[8];
extern const Score CANDIDATE_PASSER[8];

extern const Score PASSED_PAWN[8];
extern const Score PASSED_PAWN_ADVANCE_DEFENDED;
extern const Score PASSED_PAWN_EDGE_DISTANCE;
extern const Score PASSED_PAWN_KING_PROXIMITY;

extern const Score KNIGHT_THREATS[6];
extern const Score BISHOP_THREATS[6];
extern const Score ROOK_THREATS[6];
extern const Score KING_THREAT;
extern const Score PAWN_THREAT;
extern const Score PAWN_PUSH_THREAT;
extern const Score HANGING_THREAT;

extern const Score SPACE[15];

extern const Score TEMPO;

extern const Score PAWN_SHELTER[4][8];
extern const Score PAWN_STORM[4][8];
extern const Score BLOCKED_PAWN_STORM[8];
extern const Score KS_KING_FILE[4];

extern const Score KS_ATTACKER_WEIGHTS[5];
extern const Score KS_ATTACK;
extern const Score KS_PINNED;
extern const Score KS_WEAK_SQS;
extern const Score KS_SAFE_CHECK;
extern const Score KS_UNSAFE_CHECK;
extern const Score KS_ENEMY_QUEEN;
extern const Score KS_KNIGHT_DEFENSE;

extern Score PSQT[12][2][64];

void InitPSQT();

int Scale(Board* board, int ss);
Score GetPhase(Board* board);

Score Evaluate(Board* board, ThreadData* thread);

#endif