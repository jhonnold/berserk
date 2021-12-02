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
#include "util.h"

#define NO_PIECE 12

#define PieceType(pc) ((pc) >> 1)
#define file(sq) ((sq)&7)
#define rank(sq) ((sq) >> 3)
#define sq(r, f) ((r)*8 + (f))

extern const int PAWN[];
extern const int KNIGHT[];
extern const int BISHOP[];
extern const int ROOK[];
extern const int QUEEN[];
extern const int KING[];
extern const int8_t PSQT[];

extern const uint64_t PIECE_COUNT_IDX[];

void ClearBoard(Board* board);
void ParseFen(char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

void SetSpecialPieces(Board* board);
void SetOccupancies(Board* board);

int DoesMoveCheck(Move move, Board* board);
int IsRepetition(Board* board, int ply);

int HasNonPawn(Board* board);
int IsOCB(Board* board);
int IsMaterialDraw(Board* board);

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void MakeMoveUpdate(Move move, Board* board, int update);
void UndoMove(Move move, Board* board);

int IsMoveLegal(Move move, Board* board);
int MoveIsLegal(Move move, Board* board);

INLINE int KingIdx(int k, int s) { return (k & 4) == (s & 4); }

INLINE int FeatureIdx(int piece, int sq, int kingsq, const int perspective) {
  if (perspective == WHITE) {
    int pc = piece / 2 + 6 * !!(piece & 1);

    return pc * 64 + KingIdx(kingsq, sq) * 32 + PSQT[sq ^ 56];
  } else {
    int pc = piece / 2 + 6 * !(piece & 1);

    return pc * 64 + KingIdx(kingsq, sq) * 32 + PSQT[sq];
  }
}

#endif