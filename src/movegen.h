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

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

extern const BitBoard PROMOTION_RANKS[];
extern const BitBoard HOME_RANKS[];
extern const BitBoard THIRD_RANKS[];
extern const int PAWN_DIRECTIONS[];

extern const int HASH;
extern const int GOOD_CAPTURE;
extern const int BAD_CAPTURE;
extern const int KILLER1;
extern const int KILLER2;
extern const int COUNTER;

void addMove(MoveList* moveList, Move move);
void generateMoves(MoveList* moveList, SearchData* data);
void generateQuiesceMoves(MoveList* moveList, SearchData* data);
void printMoves(MoveList* moveList);
Move parseMove(char* moveStr, Board* board);
char* moveStr(Move move);
void bubbleTopMove(MoveList* moveList, int from);
void addKiller(SearchData* data, Move move);
void addCounter(SearchData* data, Move move, Move parent);
void addHistoryHeuristic(SearchData* data, Move move, int depth);
void addBFHeuristic(SearchData* data, Move move, int depth);

#endif
