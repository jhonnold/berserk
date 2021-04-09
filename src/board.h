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

#ifndef BOARD_H
#define BOARD_H

#include "types.h"

#define NO_PIECE 12

#define file(sq) ((sq)&7)
#define rank(sq) ((sq) >> 3)

extern const BitBoard EMPTY;

extern const int PAWN[];
extern const int KNIGHT[];
extern const int BISHOP[];
extern const int ROOK[];
extern const int QUEEN[];
extern const int KING[];
extern const int PIECE_TYPE[];

extern const int casltingRights[];
extern const int MIRROR[];

extern const uint64_t PIECE_COUNT_IDX[];

uint64_t zobrist(Board* board);

void clear(Board* board);
void parseFen(char* fen, Board* board);
void toFen(char* fen, Board* board);
void printBoard(Board* board);

void setSpecialPieces(Board* board);
void setOccupancies(Board* board);

int isSquareAttacked(int sq, int attacker, BitBoard occupancy, Board* board);
int isLegal(Move move, Board* board);
int isRepetition(Board* board);

int hasNonPawn(Board* board);

void nullMove(Board* board);
void undoNullMove(Board* board);
void makeMove(Move move, Board* board);
void undoMove(Move move, Board* board);

#endif