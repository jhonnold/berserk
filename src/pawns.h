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

#ifndef PAWNS_H
#define PAWNS_H

#include "types.h"

extern KPNetwork* KP_NET;

PawnHashEntry* TTPawnProbe(uint64_t hash, ThreadData* thread);
void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns, ThreadData* thread);

int GetKPNetworkIdx(int piece, int sq, int color);
Score KPNetworkScore(Board* board);
float KPNetworkPredict(BitBoard whitePawns, BitBoard blackPawns, int wk, int bk, KPNetwork* network);

void InitKPNetwork();
void SaveKPNetwork(char* path, KPNetwork* network);

Score PawnEval(Board* board, EvalData* data, int side);
Score PasserEval(Board* board, EvalData* data, int side);

#endif