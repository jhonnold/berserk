// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

#include <math.h>

#include "types.h"
#include "util.h"

extern const uint16_t KING_BUCKETS[64];

void ClearBoard(Board* board);
void ParseFen(char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

void SetSpecialPieces(Board* board);
void SetThreatsAndEasyCaptures(Board* board);

int DoesMoveCheck(Move move, Board* board);

int IsDraw(Board* board, int ply);
int IsRepetition(Board* board, int ply);
int IsMaterialDraw(Board* board);
int IsFiftyMoveRule(Board* board);

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void MakeMoveUpdate(Move move, Board* board, int update);
void UndoMove(Move move, Board* board);

int IsPseudoLegal(Move move, Board* board);
int IsLegal(Move move, Board* board);

void InitCuckoo();
int HasCycle(Board* board, int ply);

INLINE int HasNonPawn(Board* board, const int color) {
  return !!(OccBB(color) ^ PieceBB(KING, color) ^ PieceBB(PAWN, color));
}

INLINE int MoveRequiresRefresh(int piece, int from, int to) {
  if (PieceType(piece) != KING)
    return 0;

  if ((from & 4) != (to & 4))
    return 1;
  return KING_BUCKETS[from] != KING_BUCKETS[to];
}

INLINE int FeatureIdx(int piece, int sq, int kingsq, const int view) {
  int oP  = 6 * (PieceColor(piece) != view) + PieceType(piece) - 1;
  int oK  = (7 * !(kingsq & 4)) ^ (56 * view) ^ kingsq;
  int oSq = (7 * !(kingsq & 4)) ^ (56 * view) ^ sq;

  return KING_BUCKETS[oK] * 12 * 64 + oP * 64 + oSq;
}

#endif