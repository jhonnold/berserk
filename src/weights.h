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

#ifndef WEIGHTS_H
#define WEIGHTS_H

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define S(mg, eg) (makeScore((mg), (eg)))

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
extern const Score KING_MOBILITIES[9];

extern const Score MINOR_BEHIND_PAWN;
extern const Score KNIGHT_OUTPOST_REACHABLE;
extern const Score BISHOP_OUTPOST_REACHABLE;
extern const Score BISHOP_TRAPPED;
extern const Score ROOK_TRAPPED;
extern const Score BAD_BISHOP_PAWNS;
extern const Score DRAGON_BISHOP;
extern const Score ROOK_OPEN_FILE_OFFSET;
extern const Score ROOK_OPEN_FILE;
extern const Score ROOK_SEMI_OPEN;
extern const Score ROOK_TO_OPEN;
extern const Score QUEEN_OPPOSITE_ROOK;
extern const Score QUEEN_ROOK_BATTERY;

extern const Score DEFENDED_PAWN;
extern const Score DOUBLED_PAWN;
extern const Score ISOLATED_PAWN[4];
extern const Score OPEN_ISOLATED_PAWN;
extern const Score BACKWARDS_PAWN;
extern const Score CONNECTED_PAWN[4][8];
extern const Score CANDIDATE_PASSER[8];
extern const Score CANDIDATE_EDGE_DISTANCE;

extern const Score PASSED_PAWN[8];
extern const Score PASSED_PAWN_ADVANCE_DEFENDED[5];
extern const Score PASSED_PAWN_EDGE_DISTANCE;
extern const Score PASSED_PAWN_KING_PROXIMITY;
extern const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND;
extern const Score PASSED_PAWN_SQ_RULE;
extern const Score PASSED_PAWN_UNSUPPORTED;
extern const Score PASSED_PAWN_OUTSIDE_V_KNIGHT;

extern const Score KNIGHT_THREATS[6];
extern const Score BISHOP_THREATS[6];
extern const Score ROOK_THREATS[6];
extern const Score KING_THREAT;
extern const Score PAWN_THREAT;
extern const Score PAWN_PUSH_THREAT;
extern const Score PAWN_PUSH_THREAT_PINNED;
extern const Score HANGING_THREAT;
extern const Score KNIGHT_CHECK_QUEEN;
extern const Score BISHOP_CHECK_QUEEN;
extern const Score ROOK_CHECK_QUEEN;

extern const Score SPACE;

extern const Score IMBALANCE[5][5];

extern const Score PAWN_SHELTER[4][8];
extern const Score PAWN_STORM[4][8];
extern const Score BLOCKED_PAWN_STORM[8];
extern const Score CAN_CASTLE;

extern const Score COMPLEXITY_PAWNS;
extern const Score COMPLEXITY_PAWNS_BOTH_SIDES;
extern const Score COMPLEXITY_OFFSET;

extern const Score KS_ATTACKER_WEIGHTS[5];
extern const Score KS_PINNED;
extern const Score KS_WEAK_SQS;
extern const Score KS_KNIGHT_CHECK;
extern const Score KS_BISHOP_CHECK;
extern const Score KS_ROOK_CHECK;
extern const Score KS_QUEEN_CHECK;
extern const Score KS_UNSAFE_CHECK;
extern const Score KS_ENEMY_QUEEN;
extern const Score KS_KNIGHT_DEFENSE;

extern const Score TEMPO;

#endif