// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#define Piece(pc, c) (((pc) << 1) + c)
#define PieceType(pc) ((pc) >> 1)
#define PieceBB(pc, c) (board->pieces[Piece(pc, (c))])
#define OccBB(c) (board->occupancies[c])

#define File(sq) ((sq)&7)
#define Rank(sq) ((sq) >> 3)
#define Sq(r, f) ((r)*8 + (f))

extern const int8_t PSQT[64];

extern const uint64_t PIECE_COUNT_IDX[];

void ClearBoard(Board* board);
void ParseFen(char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

void SetSpecialPieces(Board* board);
void SetOccupancies(Board* board);

int DoesMoveCheck(Move move, Board* board);

int IsDraw(Board* board, int ply);
int IsRepetition(Board* board, int ply);
int IsMaterialDraw(Board* board);
int IsFiftyMoveRule(Board* board);

int HasNonPawn(Board* board);
int IsOCB(Board* board);

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void MakeMoveUpdate(Move move, Board* board, int update);
void UndoMove(Move move, Board* board);

int IsMoveLegal(Move move, Board* board);
int MoveIsLegal(Move move, Board* board);

INLINE int MoveRequiresRefresh(int piece, int from, int to) {
  if (PieceType(piece) != KING) return 0;

  return (from & 4) != (to & 4) || (from & 32) != (to & 32);
}

INLINE int KingQuadrant(int k, int s) {
  int file = ~(k ^ s) & 4;
  int rank = ~(k ^ s) & 32;

  return (file >> 1) | (rank >> 5);
}

INLINE int FeatureIdx(int piece, int sq, int kingsq, const int perspective) {
  int color = piece & 1;
  int pc = PieceType(piece);

  if (perspective == WHITE)
    sq ^= 56, kingsq ^= 56;
  else
    color ^= 1;

  return color * 6 * 4 * 32               //
         + pc * 4 * 32                    //
         + KingQuadrant(kingsq, sq) * 32  //
         + PSQT[sq];
}

#endif