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

#include "board.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "nn.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

const int8_t PSQT[] = {
    0,  1,  2,  3,  3,  2,  1,  0,   // 1
    4,  5,  6,  7,  7,  6,  5,  4,   // 2
    8,  9,  10, 11, 11, 10, 9,  8,   // 3
    12, 13, 14, 15, 15, 14, 13, 12,  // 4
    16, 17, 18, 19, 19, 18, 17, 16,  // 5
    20, 21, 22, 23, 23, 22, 21, 20,  // 6
    24, 25, 26, 27, 27, 26, 25, 24,  // 7
    28, 29, 30, 31, 31, 30, 29, 28   // 8
};

const int16_t PC_FEATURE_OFFSET[2][12] = {{
                                              0,     // WHITE_PAWN
                                              768,   // BLACK_PAWN
                                              128,   // WHITE_KNIGHT
                                              896,   // BLACK_KNIGHT
                                              256,   // WHITE_BISHOP
                                              1024,  // BLACK_BISHOP
                                              384,   // WHITE_ROOK
                                              1152,  // BLACK_ROOK
                                              512,   // WHITE_QUEEN
                                              1280,  // BLACK_QUEEN
                                              640,   // WHITE_KING
                                              1408,  // BLACK_KING
                                          },
                                          {
                                              768,   // BLACK_PAWN
                                              0,     // WHITE_PAWN
                                              896,   // BLACK_KNIGHT
                                              128,   // WHITE_KNIGHT
                                              1024,  // BLACK_BISHOP
                                              256,   // WHITE_BISHOP
                                              1152,  // BLACK_ROOK
                                              384,   // WHITE_ROOK
                                              1280,  // BLACK_QUEEN
                                              512,   // WHITE_QUEEN
                                              1408,  // BLACK_KING
                                              640,   // WHITE_KING
                                          }};

// piece count key bit mask idx
const uint64_t PIECE_COUNT_IDX[] = {1ULL << 0,  1ULL << 4,  1ULL << 8,  1ULL << 12, 1ULL << 16,
                                    1ULL << 20, 1ULL << 24, 1ULL << 28, 1ULL << 32, 1ULL << 36};

const uint64_t NON_PAWN_PIECE_MASK[] = {0x0F0F0F0F00, 0xF0F0F0F000};

// reset the board to an empty state
void ClearBoard(Board* board) {
  memset(board->pieces, 0, sizeof(board->pieces));
  memset(board->occupancies, 0, sizeof(board->occupancies));
  memset(board->zobristHistory, 0, sizeof(board->zobristHistory));
  memset(board->castlingHistory, 0, sizeof(board->castlingHistory));
  memset(board->captureHistory, 0, sizeof(board->captureHistory));
  memset(board->halfMoveHistory, 0, sizeof(board->halfMoveHistory));

  for (int i = 0; i < 64; i++) board->squares[i] = NO_PIECE;

  board->piecesCounts = 0ULL;
  board->zobrist = 0ULL;

  board->stm = WHITE;
  board->xstm = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
  board->halfMove = 0;
  board->phase = 0;
}

void ParseFen(char* fen, Board* board) {
  ClearBoard(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int)*fen];
      setBit(board->pieces[piece], i);
      board->squares[i] = piece;

      board->phase += PHASE_VALUES[PieceType(piece)];

      if (*fen != 'K' && *fen != 'k') board->piecesCounts += PIECE_COUNT_IDX[piece];
    } else if (*fen >= '0' && *fen <= '9') {
      int offset = *fen - '1';
      i += offset;
    } else if (*fen == '/') {
      i--;
    }

    fen++;
  }

  fen++;

  board->stm = (*fen++ == 'w' ? 0 : 1);
  board->xstm = board->stm ^ 1;

  fen++;

  int whiteKing = lsb(PieceBB(KING, WHITE) & RANK_1);
  int blackKing = lsb(PieceBB(KING, BLACK) & RANK_8);

  int whiteKingFile = File(whiteKing);
  int blackKingFile = File(blackKing);

  while (*fen != ' ') {
    if (*fen == 'K') {
      board->castling |= 8;
      board->castleRooks[0] = msb(PieceBB(ROOK, WHITE) & RANK_1);
    } else if (*fen == 'Q') {
      board->castling |= 4;
      board->castleRooks[1] = lsb(PieceBB(ROOK, WHITE) & RANK_1);
    } else if (*fen >= 'A' && *fen <= 'H') {
      int file = *fen - 'A';
      board->castling |= (file > whiteKingFile ? 8 : 4);
      board->castleRooks[file > whiteKingFile ? 0 : 1] = A1 + file;
    } else if (*fen == 'k') {
      board->castling |= 2;
      board->castleRooks[2] = msb(PieceBB(ROOK, BLACK) & RANK_8);
    } else if (*fen == 'q') {
      board->castling |= 1;
      board->castleRooks[3] = lsb(PieceBB(ROOK, BLACK) & RANK_8);
    } else if (*fen >= 'a' && *fen <= 'h') {
      int file = *fen - 'a';
      board->castling |= (file > blackKingFile ? 2 : 1);
      board->castleRooks[file > blackKingFile ? 2 : 3] = A8 + file;
    }

    fen++;
  }

  for (int i = 0; i < 64; i++) {
    board->castlingRights[i] = board->castling;

    if (i == whiteKing)
      board->castlingRights[i] &= 3;
    else if (i == blackKing)
      board->castlingRights[i] &= 12;
    else if (i == board->castleRooks[0] && (board->castling & 8))
      board->castlingRights[i] ^= 8;
    else if (i == board->castleRooks[1] && (board->castling & 4))
      board->castlingRights[i] ^= 4;
    else if (i == board->castleRooks[2] && (board->castling & 2))
      board->castlingRights[i] ^= 2;
    else if (i == board->castleRooks[3] && (board->castling & 1))
      board->castlingRights[i] ^= 1;
  }

  fen++;

  if (*fen != '-') {
    int f = fen[0] - 'a';
    int r = 8 - (fen[1] - '0');

    board->epSquare = r * 8 + f;
  } else {
    board->epSquare = 0;
  }

  while (*fen && *fen != ' ') fen++;

  int halfMove;
  sscanf(fen, " %d", &halfMove);
  board->halfMove = halfMove;

  SetOccupancies(board);
  SetSpecialPieces(board);

  board->zobrist = Zobrist(board);
}

void BoardToFen(char* fen, Board* board) {
  for (int r = 0; r < 8; r++) {
    int c = 0;
    for (int f = 0; f < 8; f++) {
      int sq = 8 * r + f;
      int piece = board->squares[sq];

      if (piece != NO_PIECE) {
        if (c) *fen++ = c + '0';  // append the amount of empty sqs

        *fen++ = PIECE_TO_CHAR[piece];
        c = 0;
      } else {
        c++;
      }
    }

    if (c) *fen++ = c + '0';

    *fen++ = (r == 7) ? ' ' : '/';
  }

  *fen++ = board->stm ? 'b' : 'w';
  *fen++ = ' ';

  if (board->castling) {
    if (board->castling & 0x8) *fen++ = CHESS_960 ? 'A' + File(board->castleRooks[0]) : 'K';
    if (board->castling & 0x4) *fen++ = CHESS_960 ? 'A' + File(board->castleRooks[1]) : 'Q';
    if (board->castling & 0x2) *fen++ = CHESS_960 ? 'a' + File(board->castleRooks[2]) : 'k';
    if (board->castling & 0x1) *fen++ = CHESS_960 ? 'a' + File(board->castleRooks[3]) : 'q';
  } else {
    *fen++ = '-';
  }

  *fen++ = ' ';

  sprintf(fen, "%s %d %d", board->epSquare ? SQ_TO_COORD[board->epSquare] : "-", board->halfMove, board->moveNo);
}

void PrintBoard(Board* board) {
  static char fenBuffer[128];

  for (int r = 0; r < 8; r++) {
    printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
    printf("|");
    for (int f = 0; f < 16; f++) {
      if (f == 8) printf("\n|");

      int sq = r * 8 + (f > 7 ? f - 8 : f);

      if (f < 8) {
        if (board->squares[sq] == NO_PIECE)
          printf("       |");
        else
          printf("   %c   |", PIECE_TO_CHAR[board->squares[sq]]);
      } else {
        printf("       |");
      }
    }
    printf("\n");
  }
  printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");

  BoardToFen(fenBuffer, board);
  printf("\nFEN: %s\n\n", fenBuffer);
}

inline int HasNonPawn(Board* board) {
  return bits(OccBB(board->stm) ^ PieceBB(KING, board->stm) ^ PieceBB(PAWN, board->stm));
}

inline int IsOCB(Board* board) {
  BitBoard nonBishopMaterial = PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK) | PieceBB(ROOK, WHITE) |
                               PieceBB(ROOK, BLACK) | PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK);

  return !nonBishopMaterial && bits(PieceBB(BISHOP, WHITE)) == 1 && bits(PieceBB(BISHOP, BLACK)) == 1 &&
         bits((PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)) & DARK_SQS) == 1;
}

// Reset general piece locations on the board
inline void SetOccupancies(Board* board) {
  OccBB(WHITE) = 0;
  OccBB(BLACK) = 0;
  OccBB(BOTH) = 0;

  for (int i = WHITE_PAWN; i <= BLACK_KING; i++) OccBB(i & 1) |= board->pieces[i];
  OccBB(BOTH) = OccBB(WHITE) | OccBB(BLACK);
}

// Special pieces are those giving check, and those that are pinned
// these must be recalculated every move for faster move legality purposes
inline void SetSpecialPieces(Board* board) {
  int kingSq = lsb(PieceBB(KING, board->stm));

  // Reset pinned pieces
  board->pinned = 0;
  // checked can be initialized easily with non-blockable checks
  board->checkers = (GetKnightAttacks(kingSq) & PieceBB(KNIGHT, board->xstm)) |
                    (GetPawnAttacks(kingSq, board->stm) & PieceBB(PAWN, board->xstm));

  // for each side
  for (int kingColor = WHITE; kingColor <= BLACK; kingColor++) {
    int enemyColor = 1 - kingColor;
    kingSq = lsb(PieceBB(KING, kingColor));

    // get full rook/bishop rays from the king that intersect that piece type of the enemy
    BitBoard enemyPiece = ((PieceBB(BISHOP, enemyColor) | PieceBB(QUEEN, enemyColor)) & GetBishopAttacks(kingSq, 0)) |
                          ((PieceBB(ROOK, enemyColor) | PieceBB(QUEEN, enemyColor)) & GetRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      // is something in the way
      BitBoard blockers = GetInBetweenSquares(kingSq, sq) & OccBB(BOTH);

      if (!blockers)
        // no? then its check
        board->checkers |= (enemyPiece & -enemyPiece);
      else if (bits(blockers) == 1)
        // just 1? then its pinned
        board->pinned |= (blockers & OccBB(kingColor));

      // TODO: Similar logic can be applied for discoveries, but apply for self pieces

      popLsb(enemyPiece);
    }
  }
}

void MakeMove(Move move, Board* board) { MakeMoveUpdate(move, board, 1); }

void MakeMoveUpdate(Move move, Board* board, int update) {
  NNUpdate wUpdates[1] = {0};
  NNUpdate bUpdates[1] = {0};

  int from = From(move);
  int to = To(move);
  int piece = Moving(move);
  int promoted = Promo(move);
  int capture = IsCap(move);
  int dub = IsDP(move);
  int ep = IsEP(move);
  int castle = IsCas(move);
  int captured = board->squares[to];

  int wkingSq = lsb(PieceBB(KING, WHITE));
  int bkingSq = lsb(PieceBB(KING, BLACK));

  // store hard to recalculate values
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE;  // this might get overwritten
  board->halfMoveHistory[board->moveNo] = board->halfMove;
  board->checkersHistory[board->moveNo] = board->checkers;
  board->pinnedHistory[board->moveNo] = board->pinned;

  flipBits(board->pieces[piece], from, to);

  board->squares[from] = NO_PIECE;
  board->squares[to] = piece;

  board->zobrist ^= ZOBRIST_PIECES[piece][from];
  board->zobrist ^= ZOBRIST_PIECES[piece][to];

  AddRemoval(FeatureIdx(piece, from, wkingSq, WHITE), wUpdates);
  AddRemoval(FeatureIdx(piece, from, bkingSq, BLACK), bUpdates);
  AddAddition(FeatureIdx(promoted ? promoted : piece, to, wkingSq, WHITE), wUpdates);
  AddAddition(FeatureIdx(promoted ? promoted : piece, to, bkingSq, BLACK), bUpdates);

  if (piece == Piece(PAWN, board->stm))
    board->halfMove = 0;  // reset on pawn move
  else
    board->halfMove++;

  if (capture && !ep) {
    board->captureHistory[board->moveNo] = captured;
    flipBit(board->pieces[captured], to);

    board->zobrist ^= ZOBRIST_PIECES[captured][to];

    AddRemoval(FeatureIdx(captured, to, wkingSq, WHITE), wUpdates);
    AddRemoval(FeatureIdx(captured, to, bkingSq, BLACK), bUpdates);

    board->piecesCounts -= PIECE_COUNT_IDX[captured];  // when there's a capture, we need to update our piece counts
    board->halfMove = 0;                               // reset on capture

    board->phase -= PHASE_VALUES[PieceType(captured)];
  }

  if (promoted) {
    flipBit(board->pieces[piece], to);
    flipBit(board->pieces[promoted], to);

    board->squares[to] = promoted;

    board->zobrist ^= ZOBRIST_PIECES[piece][to];
    board->zobrist ^= ZOBRIST_PIECES[promoted][to];

    board->piecesCounts -= PIECE_COUNT_IDX[piece];
    board->piecesCounts += PIECE_COUNT_IDX[promoted];

    board->phase += PHASE_VALUES[PieceType(promoted)];
  }

  if (ep) {
    // ep has to be handled specially since the to location won't remove the opponents pawn
    flipBit(PieceBB(PAWN, board->xstm), to - PawnDir(board->stm));

    board->squares[to - PawnDir(board->stm)] = NO_PIECE;

    board->zobrist ^= ZOBRIST_PIECES[Piece(PAWN, board->xstm)][to - PawnDir(board->stm)];

    AddRemoval(FeatureIdx(Piece(PAWN, board->xstm), to - PawnDir(board->stm), wkingSq, WHITE), wUpdates);
    AddRemoval(FeatureIdx(Piece(PAWN, board->xstm), to - PawnDir(board->stm), bkingSq, BLACK), bUpdates);

    board->piecesCounts -= PIECE_COUNT_IDX[Piece(PAWN, board->xstm)];
    board->halfMove = 0;  // this is a capture

    // skip the phase as pawns = 0
  }

  if (board->epSquare) {
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
    board->epSquare = 0;
  }

  if (dub) {
    board->epSquare = to - PawnDir(board->stm);
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  }

  if (castle) {
    // move the rook during a castle, the king encoding is handled automatically
    if (to == G1) {
      flipBits(PieceBB(ROOK, WHITE), board->castleRooks[0], F1);

      if (board->castleRooks[0] != to) board->squares[board->castleRooks[0]] = NO_PIECE;
      board->squares[F1] = WHITE_ROOK;

      board->zobrist ^= ZOBRIST_PIECES[WHITE_ROOK][board->castleRooks[0]];
      board->zobrist ^= ZOBRIST_PIECES[WHITE_ROOK][F1];

      AddRemoval(FeatureIdx(WHITE_ROOK, board->castleRooks[0], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(WHITE_ROOK, board->castleRooks[0], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(WHITE_ROOK, F1, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(WHITE_ROOK, F1, bkingSq, BLACK), bUpdates);
    } else if (to == C1) {
      flipBits(PieceBB(ROOK, WHITE), board->castleRooks[1], D1);

      if (board->castleRooks[1] != to) board->squares[board->castleRooks[1]] = NO_PIECE;
      board->squares[D1] = WHITE_ROOK;

      board->zobrist ^= ZOBRIST_PIECES[WHITE_ROOK][board->castleRooks[1]];
      board->zobrist ^= ZOBRIST_PIECES[WHITE_ROOK][D1];

      AddRemoval(FeatureIdx(WHITE_ROOK, board->castleRooks[1], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(WHITE_ROOK, board->castleRooks[1], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(WHITE_ROOK, D1, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(WHITE_ROOK, D1, bkingSq, BLACK), bUpdates);
    } else if (to == G8) {
      flipBits(PieceBB(ROOK, BLACK), board->castleRooks[2], F8);

      if (board->castleRooks[2] != to) board->squares[board->castleRooks[2]] = NO_PIECE;
      board->squares[F8] = BLACK_ROOK;

      board->zobrist ^= ZOBRIST_PIECES[BLACK_ROOK][board->castleRooks[2]];
      board->zobrist ^= ZOBRIST_PIECES[BLACK_ROOK][F8];

      AddRemoval(FeatureIdx(BLACK_ROOK, board->castleRooks[2], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(BLACK_ROOK, board->castleRooks[2], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(BLACK_ROOK, F8, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(BLACK_ROOK, F8, bkingSq, BLACK), bUpdates);
    } else if (to == C8) {
      flipBits(PieceBB(ROOK, BLACK), board->castleRooks[3], D8);

      if (board->castleRooks[3] != to) board->squares[board->castleRooks[3]] = NO_PIECE;
      board->squares[D8] = BLACK_ROOK;

      board->zobrist ^= ZOBRIST_PIECES[BLACK_ROOK][board->castleRooks[3]];
      board->zobrist ^= ZOBRIST_PIECES[BLACK_ROOK][D8];

      AddRemoval(FeatureIdx(BLACK_ROOK, board->castleRooks[3], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(BLACK_ROOK, board->castleRooks[3], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(BLACK_ROOK, D8, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(BLACK_ROOK, D8, bkingSq, BLACK), bUpdates);
    }
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= board->castlingRights[from];
  board->castling &= board->castlingRights[to];
  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];

  SetOccupancies(board);

  board->moveNo++;
  board->ply++;
  board->xstm = board->stm;
  board->stm ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;

  // special pieces must be loaded after the stm has changed
  // this is because the new stm to move will be the one in check
  SetSpecialPieces(board);

  if (update) {
    if ((piece == WHITE_KING || piece == BLACK_KING) && ((from & 4) != (to & 4) || (from & 32) != (to & 32))) {
      if (piece == WHITE_KING) {
        RefreshAccumulator(board->accumulators[WHITE][board->ply], board, WHITE);
        ApplyUpdates(board, BLACK, bUpdates);
      } else {
        ApplyUpdates(board, WHITE, wUpdates);
        RefreshAccumulator(board->accumulators[BLACK][board->ply], board, BLACK);
      }
    } else {
      ApplyUpdates(board, WHITE, wUpdates);
      ApplyUpdates(board, BLACK, bUpdates);
    }
  }

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoMove(Move move, Board* board) {
  int from = From(move);
  int to = To(move);
  int piece = Moving(move);
  int promoted = Promo(move);
  int capture = IsCap(move);
  int ep = IsEP(move);
  int castle = IsCas(move);

  board->stm = board->xstm;
  board->xstm ^= 1;
  board->moveNo--;
  board->ply--;

  // reload historical values
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->zobrist = board->zobristHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];

  if (!promoted)
    flipBits(board->pieces[piece], to, from);
  else
    flipBit(board->pieces[piece], from);

  board->squares[to] = NO_PIECE;
  board->squares[from] = piece;

  if (capture) {
    int captured = board->captureHistory[board->moveNo];
    flipBit(board->pieces[captured], to);

    if (!ep) {
      board->squares[to] = captured;

      board->piecesCounts += PIECE_COUNT_IDX[captured];

      board->phase += PHASE_VALUES[PieceType(captured)];
    }
  }

  if (promoted) {
    flipBit(board->pieces[promoted], to);

    board->piecesCounts -= PIECE_COUNT_IDX[promoted];
    board->piecesCounts += PIECE_COUNT_IDX[piece];

    board->phase -= PHASE_VALUES[PieceType(promoted)];
  }

  if (ep) {
    flipBit(PieceBB(PAWN, board->xstm), to - PawnDir(board->stm));
    board->squares[to - PawnDir(board->stm)] = Piece(PAWN, board->xstm);

    board->piecesCounts += PIECE_COUNT_IDX[Piece(PAWN, board->xstm)];
  }

  if (castle) {
    // put the rook back
    if (to == G1) {
      flipBits(PieceBB(ROOK, WHITE), F1, board->castleRooks[0]);

      if (from != F1) board->squares[F1] = NO_PIECE;

      board->squares[board->castleRooks[0]] = WHITE_ROOK;
    } else if (to == C1) {
      flipBits(PieceBB(ROOK, WHITE), D1, board->castleRooks[1]);

      if (from != D1) board->squares[D1] = NO_PIECE;
      board->squares[board->castleRooks[1]] = WHITE_ROOK;
    } else if (to == G8) {
      flipBits(PieceBB(ROOK, BLACK), F8, board->castleRooks[2]);

      if (from != F8) board->squares[F8] = NO_PIECE;
      board->squares[board->castleRooks[2]] = BLACK_ROOK;
    } else if (to == C8) {
      flipBits(PieceBB(ROOK, BLACK), D8, board->castleRooks[3]);

      if (from != D8) board->squares[D8] = NO_PIECE;
      board->squares[board->castleRooks[3]] = BLACK_ROOK;
    }
  }

  SetOccupancies(board);
}

void MakeNullMove(Board* board) {
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE;
  board->halfMoveHistory[board->moveNo] = board->halfMove;
  board->checkersHistory[board->moveNo] = board->checkers;
  board->pinnedHistory[board->moveNo] = board->pinned;

  board->halfMove++;

  if (board->epSquare) board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;

  board->moveNo++;
  board->stm = board->xstm;
  board->xstm ^= 1;

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoNullMove(Board* board) {
  board->stm = board->xstm;
  board->xstm ^= 1;
  board->moveNo--;

  board->zobrist = board->zobristHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];
}

inline int IsDraw(Board* board, int ply) {
  return IsRepetition(board, ply) || IsMaterialDraw(board) || IsFiftyMoveRule(board);
}

inline int IsRepetition(Board* board, int ply) {
  int reps = 0;

  // Check as far back as the last non-reversible move
  for (int i = board->moveNo - 2; i >= 0 && i >= board->moveNo - board->halfMove; i -= 2) {
    if (board->zobristHistory[i] == board->zobrist) {
      if (i > board->moveNo - ply)  // within our search tree
        return 1;

      reps++;
      if (reps == 2)  // 3-fold before+including root
        return 1;
    }
  }

  return 0;
}

inline int IsMaterialDraw(Board* board) {
  switch (board->piecesCounts) {
    case 0x0:       // Kk
    case 0x100:     // KNk
    case 0x200:     // KNNk
    case 0x1000:    // Kkn
    case 0x2000:    // Kknn
    case 0x1100:    // KNkn
    case 0x10000:   // KBk
    case 0x100000:  // Kkb
    case 0x11000:   // KBkn
    case 0x100100:  // KNkb
    case 0x110000:  // KBkb
      return 1;
    default:
      return 0;
  }
}

inline int IsFiftyMoveRule(Board* board) {
  if (board->halfMove > 99) {
    if (board->checkers) {
      SimpleMoveList moves[1];
      RootMoves(moves, board);

      return moves->count > 0;
    }
  }

  return 0;
}

int MoveIsLegal(Move move, Board* board) {
  int piece = Moving(move);
  int from = From(move);
  int to = To(move);

  // moving piece isn't where it says it is
  if (!move || piece > BLACK_KING || (piece & 1) != board->stm || piece != board->squares[from]) return 0;

  // can't capture our own piece unless castling
  if (board->squares[to] != NO_PIECE && !IsCas(move) && (!IsCap(move) || (board->squares[to] & 1) == board->stm))
    return 0;

  // can't capture air
  if (IsCap(move) && board->squares[to] == NO_PIECE && !IsEP(move)) return 0;

  // non-pawns can't promote/dp/ep
  if ((Promo(move) || IsDP(move) || IsEP(move)) && PieceType(piece) != PAWN) return 0;

  // non-kings can't castle
  if (IsCas(move) && (PieceType(piece) != KING || IsCap(move))) return 0;

  BitBoard possible = -1ULL;
  if (getBit(board->pinned, from)) possible &= GetPinnedMovementSquares(from, lsb(PieceBB(KING, board->stm)));

  if (board->checkers)
    possible &= GetInBetweenSquares(lsb(board->checkers), lsb(PieceBB(KING, board->stm))) | board->checkers;

  if (bits(board->checkers) > 1 && PieceType(piece) != KING) return 0;

  switch (PieceType(piece)) {
    case KING:
      if (!IsCas(move) && !getBit(GetKingAttacks(from), to)) return 0;
      break;
    case KNIGHT:
      return !!getBit(GetKnightAttacks(from) & possible, to);
    case BISHOP:
      return !!getBit(GetBishopAttacks(from, OccBB(BOTH)) & possible, to);
    case ROOK:
      return !!getBit(GetRookAttacks(from, OccBB(BOTH)) & possible, to);
    case QUEEN:
      return !!getBit(GetQueenAttacks(from, OccBB(BOTH)) & possible, to);
    case PAWN:
      if (IsEP(move)) break;

      BitBoard atx = GetPawnAttacks(from, board->stm);
      BitBoard adv = board->stm == WHITE ? ShiftN(bit(from)) : ShiftS(bit(from));
      adv &= ~OccBB(BOTH);

      if (Promo(move))
        return !!getBit((board->stm == WHITE ? RANK_8 : RANK_1) & ((atx & OccBB(board->xstm)) | adv) & possible, to) &&
               Promo(move) >= WHITE_KNIGHT && Promo(move) <= BLACK_QUEEN && (Promo(move) & 1) == board->stm &&
               !IsDP(move);

      adv |= board->stm == WHITE ? ShiftN(adv & RANK_3) : ShiftS(adv & RANK_6);
      adv &= ~OccBB(BOTH);

      return !!getBit((board->stm == WHITE ? ~RANK_8 : ~RANK_1) & ((atx & OccBB(board->xstm)) | adv) & possible, to) &&
             IsDP(move) == (abs(from - to) == 16);
  }

  if (IsEP(move)) {
    if (!IsCap(move) || IsDP(move) || Promo(move) || !board->epSquare || board->epSquare != to ||
        PieceType(piece) != PAWN || !getBit(GetPawnAttacks(from, board->stm), board->epSquare))
      return 0;
  }

  if (IsCas(move)) {
    if (board->checkers) return 0;

    int kingSq = lsb(PieceBB(KING, board->stm));

    if (from != kingSq) return 0;

    if ((board->stm == WHITE && to != G1 && to != C1) || (board->stm == BLACK && to != G8 && to != C8)) return 0;

    if (to == G1) {
      if (!(board->castling & 0x8)) return 0;

      if (getBit(board->pinned, board->castleRooks[0])) return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, G1) | GetInBetweenSquares(board->castleRooks[0], F1) | bit(G1) | bit(F1);
      if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->castleRooks[0])) & between) return 0;
    }

    if (to == C1) {
      if (!(board->castling & 0x4)) return 0;

      if (getBit(board->pinned, board->castleRooks[1])) return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, C1) | GetInBetweenSquares(board->castleRooks[1], D1) | bit(C1) | bit(D1);
      if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->castleRooks[1])) & between) return 0;
    }

    if (to == G8) {
      if (!(board->castling & 0x2)) return 0;

      if (getBit(board->pinned, board->castleRooks[2])) return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, G8) | GetInBetweenSquares(board->castleRooks[2], F8) | bit(G8) | bit(F8);
      if ((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ bit(board->castleRooks[2])) & between) return 0;
    }

    if (to == C8) {
      if (!(board->castling & 0x1)) return 0;

      if (getBit(board->pinned, board->castleRooks[3])) return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, C8) | GetInBetweenSquares(board->castleRooks[3], D8) | bit(C8) | bit(D8);
      if ((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ bit(board->castleRooks[3])) & between) return 0;
    }
  }

  // this is a legality checker for ep/king/castles (used by movegen)
  return IsMoveLegal(move, board);
}

// this is NOT a legality checker for ALL moves
// it is only used by move generation for king moves, castles, and ep captures
int IsMoveLegal(Move move, Board* board) {
  int from = From(move);
  int to = To(move);

  if (IsEP(move)) {
    // ep is checked by just applying the move
    int kingSq = lsb(PieceBB(KING, board->stm));
    int captureSq = to - PawnDir(board->stm);
    BitBoard newOccupancy = OccBB(BOTH);
    popBit(newOccupancy, from);
    popBit(newOccupancy, captureSq);
    setBit(newOccupancy, to);

    // EP can only be illegal due to crazy discover checks
    return !(GetBishopAttacks(kingSq, newOccupancy) & (PieceBB(BISHOP, board->xstm) | PieceBB(QUEEN, board->xstm))) &&
           !(GetRookAttacks(kingSq, newOccupancy) & (PieceBB(ROOK, board->xstm) | PieceBB(QUEEN, board->xstm)));
  } else if (IsCas(move)) {
    int step = (to > from) ? -1 : 1;

    // pieces in-between have been checked, now check that it's not castling through or into check
    for (int i = to; i != from; i += step) {
      if (AttacksToSquare(board, i, OccBB(BOTH)) & OccBB(board->xstm)) return 0;
    }

    return 1;
  } else if (Moving(move) >= WHITE_KING) {
    return !(AttacksToSquare(board, to, OccBB(BOTH) ^ PieceBB(KING, board->stm)) & OccBB(board->xstm));
  }

  return 1;
}
