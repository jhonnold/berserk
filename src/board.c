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

const uint16_t KING_BUCKETS[64] = {15, 15, 14, 14, 14, 14, 15, 15, //
                                   15, 15, 14, 14, 14, 14, 15, 15, //
                                   13, 13, 12, 12, 12, 12, 13, 13, //
                                   13, 13, 12, 12, 12, 12, 13, 13, //
                                   11, 10, 9,  8,  8,  9,  10, 11, //
                                   11, 10, 9,  8,  8,  9,  10, 11, //
                                   7,  6,  5,  4,  4,  5,  6,  7,  //
                                   3,  2,  1,  0,  0,  1,  2,  3};

// piece count key bit mask idx
const uint64_t PIECE_COUNT_IDX[] =
  {1ULL << 0, 1ULL << 4, 1ULL << 8, 1ULL << 12, 1ULL << 16, 1ULL << 20, 1ULL << 24, 1ULL << 28, 1ULL << 32, 1ULL << 36};

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
  board->zobrist      = 0ULL;

  board->stm  = WHITE;
  board->xstm = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->histPly  = 0;
  board->moveNo   = 1;
  board->halfMove = 0;
  board->phase    = 0;
}

void ParseFen(char* fen, Board* board) {
  ClearBoard(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int) *fen];
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

  board->stm  = (*fen++ == 'w' ? 0 : 1);
  board->xstm = board->stm ^ 1;

  fen++;

  int whiteKing = lsb(PieceBB(KING, WHITE) & RANK_1);
  int blackKing = lsb(PieceBB(KING, BLACK) & RANK_8);

  int whiteKingFile = File(whiteKing);
  int blackKingFile = File(blackKing);

  while (*fen != ' ') {
    if (*fen == 'K') {
      board->castling |= 8;
      board->cr[0] = msb(PieceBB(ROOK, WHITE) & RANK_1);
    } else if (*fen == 'Q') {
      board->castling |= 4;
      board->cr[1] = lsb(PieceBB(ROOK, WHITE) & RANK_1);
    } else if (*fen >= 'A' && *fen <= 'H') {
      int file = *fen - 'A';
      board->castling |= (file > whiteKingFile ? 8 : 4);
      board->cr[file > whiteKingFile ? 0 : 1] = A1 + file;
    } else if (*fen == 'k') {
      board->castling |= 2;
      board->cr[2] = msb(PieceBB(ROOK, BLACK) & RANK_8);
    } else if (*fen == 'q') {
      board->castling |= 1;
      board->cr[3] = lsb(PieceBB(ROOK, BLACK) & RANK_8);
    } else if (*fen >= 'a' && *fen <= 'h') {
      int file = *fen - 'a';
      board->castling |= (file > blackKingFile ? 2 : 1);
      board->cr[file > blackKingFile ? 2 : 3] = A8 + file;
    }

    fen++;
  }

  for (int i = 0; i < 64; i++) {
    board->castlingRights[i] = board->castling;

    if (i == whiteKing)
      board->castlingRights[i] &= 3;
    else if (i == blackKing)
      board->castlingRights[i] &= 12;
    else if (i == board->cr[0] && (board->castling & 8))
      board->castlingRights[i] ^= 8;
    else if (i == board->cr[1] && (board->castling & 4))
      board->castlingRights[i] ^= 4;
    else if (i == board->cr[2] && (board->castling & 2))
      board->castlingRights[i] ^= 2;
    else if (i == board->cr[3] && (board->castling & 1))
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

  sscanf(fen, " %d %d", &board->halfMove, &board->moveNo);

  SetOccupancies(board);
  SetSpecialPieces(board);

  board->zobrist = Zobrist(board);
}

void BoardToFen(char* fen, Board* board) {
  for (int r = 0; r < 8; r++) {
    int c = 0;
    for (int f = 0; f < 8; f++) {
      int sq    = 8 * r + f;
      int piece = board->squares[sq];

      if (piece != NO_PIECE) {
        if (c) *fen++ = c + '0'; // append the amount of empty sqs

        *fen++ = PIECE_TO_CHAR[piece];
        c      = 0;
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
    if (board->castling & 0x8) *fen++ = CHESS_960 ? 'A' + File(board->cr[0]) : 'K';
    if (board->castling & 0x4) *fen++ = CHESS_960 ? 'A' + File(board->cr[1]) : 'Q';
    if (board->castling & 0x2) *fen++ = CHESS_960 ? 'a' + File(board->cr[2]) : 'k';
    if (board->castling & 0x1) *fen++ = CHESS_960 ? 'a' + File(board->cr[3]) : 'q';
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
  OccBB(BOTH)  = 0;

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
    kingSq         = lsb(PieceBB(KING, kingColor));

    // get full rook/bishop rays from the king that intersect that piece type of the enemy
    BitBoard enemyPiece = ((PieceBB(BISHOP, enemyColor) | PieceBB(QUEEN, enemyColor)) & GetBishopAttacks(kingSq, 0)) |
                          ((PieceBB(ROOK, enemyColor) | PieceBB(QUEEN, enemyColor)) & GetRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      // is something in the way
      BitBoard blockers = BetweenSquares(kingSq, sq) & OccBB(BOTH);

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

void MakeMove(Move move, Board* board) {
  MakeMoveUpdate(move, board, 1);
}

void MakeMoveUpdate(Move move, Board* board, int update) {
  int from     = From(move);
  int to       = To(move);
  int piece    = Moving(move);
  int promoted = Promo(move);
  int capture  = IsCap(move);
  int dub      = IsDP(move);
  int ep       = IsEP(move);
  int castle   = IsCas(move);
  int captured = !ep ? board->squares[to] : Piece(PAWN, board->xstm);

  // store hard to recalculate values
  board->zobristHistory[board->histPly]  = board->zobrist;
  board->castlingHistory[board->histPly] = board->castling;
  board->epSquareHistory[board->histPly] = board->epSquare;
  board->captureHistory[board->histPly]  = NO_PIECE; // this might get overwritten
  board->halfMoveHistory[board->histPly] = board->halfMove;
  board->checkersHistory[board->histPly] = board->checkers;
  board->pinnedHistory[board->histPly]   = board->pinned;

  flipBits(board->pieces[piece], from, to);

  board->squares[from] = NO_PIECE;
  board->squares[to]   = piece;

  board->zobrist ^= ZOBRIST_PIECES[piece][from];
  board->zobrist ^= ZOBRIST_PIECES[piece][to];

  if (piece == Piece(PAWN, board->stm))
    board->halfMove = 0; // reset on pawn move
  else
    board->halfMove++;

  if (capture && !ep) {
    board->captureHistory[board->histPly] = captured;
    flipBit(board->pieces[captured], to);

    board->zobrist ^= ZOBRIST_PIECES[captured][to];

    board->piecesCounts -= PIECE_COUNT_IDX[captured]; // when there's a capture, we need to update our piece counts
    board->halfMove = 0;                              // reset on capture

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

    board->piecesCounts -= PIECE_COUNT_IDX[Piece(PAWN, board->xstm)];
    board->halfMove = 0; // this is a capture

    // skip the phase as pawns = 0
  }

  if (board->epSquare) {
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
    board->epSquare = 0;
  }

  if (dub) {
    int epSquare = to - PawnDir(board->stm);

    if (GetPawnAttacks(epSquare, board->stm) & PieceBB(PAWN, board->xstm)) {
      board->epSquare = epSquare;
      board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
    }
  }

  if (castle) {
    int rookFrom = board->cr[CASTLING_ROOK[to]];
    int rookTo   = CASTLE_ROOK_DEST[to];
    int rook     = Piece(ROOK, board->stm);

    flipBits(PieceBB(ROOK, board->stm), rookFrom, rookTo);

    // chess960 can have the king going where the rook started
    // do this check to prevent accidental wipe outs
    if (rookFrom != to) board->squares[rookFrom] = NO_PIECE;
    board->squares[rookTo] = rook;

    board->zobrist ^= ZOBRIST_PIECES[rook][rookFrom];
    board->zobrist ^= ZOBRIST_PIECES[rook][rookTo];
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= board->castlingRights[from];
  board->castling &= board->castlingRights[to];
  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];

  SetOccupancies(board);

  int stm = board->stm;

  board->histPly++;
  board->acc++;
  board->moveNo += (board->stm == BLACK);
  board->xstm = board->stm;
  board->stm ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);

  // special pieces must be loaded after the stm has changed
  // this is because the new stm to move will be the one in check
  SetSpecialPieces(board);

  if (update) {
    if (stm == BLACK) from ^= 56, to ^= 56;

    if (MoveRequiresRefresh(piece, from, to)) {
      int colorToRefresh = piece & 1;
      RefreshAccumulator(board->accumulators[colorToRefresh][board->acc], board, colorToRefresh);
      ApplyUpdates(board, move, captured, !colorToRefresh);
    } else {
      ApplyUpdates(board, move, captured, WHITE);
      ApplyUpdates(board, move, captured, BLACK);
    }
  }
}

void UndoMove(Move move, Board* board) {
  int from     = From(move);
  int to       = To(move);
  int piece    = Moving(move);
  int promoted = Promo(move);
  int capture  = IsCap(move);
  int ep       = IsEP(move);
  int castle   = IsCas(move);

  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;
  board->acc--;
  board->moveNo -= (board->stm == BLACK);

  // reload historical values
  board->epSquare = board->epSquareHistory[board->histPly];
  board->castling = board->castlingHistory[board->histPly];
  board->zobrist  = board->zobristHistory[board->histPly];
  board->halfMove = board->halfMoveHistory[board->histPly];
  board->checkers = board->checkersHistory[board->histPly];
  board->pinned   = board->pinnedHistory[board->histPly];

  if (!promoted)
    flipBits(board->pieces[piece], to, from);
  else
    flipBit(board->pieces[piece], from);

  board->squares[to]   = NO_PIECE;
  board->squares[from] = piece;

  if (capture && !ep) {
    int captured = board->captureHistory[board->histPly];
    flipBit(board->pieces[captured], to);

    board->squares[to] = captured;

    board->piecesCounts += PIECE_COUNT_IDX[captured];
    board->phase += PHASE_VALUES[PieceType(captured)];
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
    int rookFrom = board->cr[CASTLING_ROOK[to]];
    int rookTo   = CASTLE_ROOK_DEST[to];
    int rook     = Piece(ROOK, board->stm);

    flipBits(PieceBB(ROOK, board->stm), rookTo, rookFrom);
    if (from != rookTo) board->squares[rookTo] = NO_PIECE;
    board->squares[rookFrom] = rook;
  }

  SetOccupancies(board);
}

void MakeNullMove(Board* board) {
  board->zobristHistory[board->histPly]  = board->zobrist;
  board->castlingHistory[board->histPly] = board->castling;
  board->epSquareHistory[board->histPly] = board->epSquare;
  board->captureHistory[board->histPly]  = NO_PIECE;
  board->halfMoveHistory[board->histPly] = board->halfMove;
  board->checkersHistory[board->histPly] = board->checkers;
  board->pinnedHistory[board->histPly]   = board->pinned;

  board->halfMove++;

  if (board->epSquare) board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;

  board->histPly++;
  board->stm = board->xstm;
  board->xstm ^= 1;

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoNullMove(Board* board) {
  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;

  board->zobrist  = board->zobristHistory[board->histPly];
  board->castling = board->castlingHistory[board->histPly];
  board->epSquare = board->epSquareHistory[board->histPly];
  board->halfMove = board->halfMoveHistory[board->histPly];
  board->checkers = board->checkersHistory[board->histPly];
  board->pinned   = board->pinnedHistory[board->histPly];
}

inline int IsDraw(Board* board, int ply) {
  return IsRepetition(board, ply) || IsMaterialDraw(board) || IsFiftyMoveRule(board);
}

inline int IsRepetition(Board* board, int ply) {
  int reps = 0;

  // Check as far back as the last non-reversible move
  for (int i = board->histPly - 2; i >= 0 && i >= board->histPly - board->halfMove; i -= 2) {
    if (board->zobristHistory[i] == board->zobrist) {
      if (i > board->histPly - ply) // within our search tree
        return 1;

      reps++;
      if (reps == 2) // 3-fold before+including root
        return 1;
    }
  }

  return 0;
}

inline int IsMaterialDraw(Board* board) {
  switch (board->piecesCounts) {
    case 0x0:      // Kk
    case 0x100:    // KNk
    case 0x200:    // KNNk
    case 0x1000:   // Kkn
    case 0x2000:   // Kknn
    case 0x1100:   // KNkn
    case 0x10000:  // KBk
    case 0x100000: // Kkb
    case 0x11000:  // KBkn
    case 0x100100: // KNkb
    case 0x110000: // KBkb
      return 1;
    default: return 0;
  }
}

inline int IsFiftyMoveRule(Board* board) {
  if (board->halfMove > 99) {
    if (board->checkers) {
      SimpleMoveList moves[1];
      RootMoves(moves, board);

      return moves->count > 0;
    }

    return 1;
  }

  return 0;
}

int IsPseudoLegal(Move move, Board* board) {
  int from   = From(move);
  int to     = To(move);
  int piece  = Moving(move);
  int pcType = PieceType(piece);

  if (!move || (piece & 1) != board->stm || piece != board->squares[from]) return 0;

  if (IsCas(move)) {
    if (board->checkers) return 0;

    int kingsq = lsb(PieceBB(KING, board->stm));

    switch (to) {
      case G1: {
        if (!CanCastle(WHITE_KS)) return 0;
        BitBoard between = BetweenSquares(kingsq, G1) | BetweenSquares(board->cr[0], F1) | bit(G1) | bit(F1);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->cr[0])) & between) return 0;
        break;
      }
      case C1: {
        if (!CanCastle(WHITE_QS)) return 0;
        BitBoard between = BetweenSquares(kingsq, C1) | BetweenSquares(board->cr[1], D1) | bit(C1) | bit(D1);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->cr[1])) & between) return 0;
        break;
      }
      case G8: {
        if (!CanCastle(BLACK_KS)) return 0;
        BitBoard between = BetweenSquares(kingsq, G8) | BetweenSquares(board->cr[2], F8) | bit(G8) | bit(F8);
        if ((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ bit(board->cr[2])) & between) return 0;
        break;
      }
      case C8: {
        if (!CanCastle(BLACK_QS)) return 0;
        BitBoard between = BetweenSquares(kingsq, C8) | BetweenSquares(board->cr[3], D8) | bit(C8) | bit(D8);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ bit(board->cr[3])) & between) return 0;
        break;
      }
    }

    return 1;
  }

  if (Promo(move)) {
    if (bits(board->checkers) > 1) return 0;

    int king       = lsb(PieceBB(KING, board->stm));
    BitBoard valid = board->checkers ? (board->checkers | BetweenSquares(king, lsb(board->checkers))) : ALL;
    BitBoard goal  = board->stm == WHITE ? RANK_8 : RANK_1;
    BitBoard opts =
      (ShiftPawnDir(bit(from), board->stm) & ~OccBB(BOTH)) | (GetPawnAttacks(from, board->stm) & OccBB(board->xstm));

    return !!getBit(goal & opts & valid, to);
  }

  if (IsCap(move) && !IsEP(move) && board->squares[to] == NO_PIECE) return 0;
  if (!IsCap(move) && board->squares[to] != NO_PIECE) return 0;
  if (IsEP(move) && to != board->epSquare) return 0;
  if (getBit(OccBB(board->stm), to)) return 0;

  if (pcType == PAWN) {
    if (getBit(RANK_1 | RANK_8, to)) return 0;

    if (!IsEP(move)) {
      if (!(GetPawnAttacks(from, board->stm) & OccBB(board->xstm) & bit(to))         // capture
          && !((from + PawnDir(board->stm) == to) && board->squares[to] == NO_PIECE) // single push
          && !((from + 2 * PawnDir(board->stm) == to)                                // double push
               && board->squares[to] == NO_PIECE                                     //
               && board->squares[to - PawnDir(board->stm)] == NO_PIECE               //
               && (Rank(from ^ (56 * board->stm)) == 6)                              //
               ))
        return 0;
    }
  } else if (!getBit(GetPieceAttacks(from, OccBB(BOTH), pcType), to)) {
    return 0;
  }

  if (board->checkers && pcType != KING) {
    if (bits(board->checkers) > 1) return 0;

    int kingsq                 = lsb(PieceBB(KING, board->stm));
    BitBoard blocksAndCaptures = board->checkers | BetweenSquares(kingsq, lsb(board->checkers));
    int to2                    = IsEP(move) ? to - PawnDir(board->stm) : to;
    if (!getBit(blocksAndCaptures, to2)) return 0;
  }

  return 1;
}

// this is NOT a legality checker for ALL moves
// it is only used by move generation for king moves, castles, and ep captures
int IsLegal(Move move, Board* board) {
  int from   = From(move);
  int to     = To(move);
  int kingSq = lsb(PieceBB(KING, board->stm));

  if (IsEP(move)) {
    // ep is checked by just applying the move
    int captureSq = to - PawnDir(board->stm);
    BitBoard occ  = (OccBB(BOTH) ^ bit(from) ^ bit(captureSq)) | bit(to);
    // EP can only be illegal due to crazy discover checks
    return !(GetBishopAttacks(kingSq, occ) & (PieceBB(BISHOP, board->xstm) | PieceBB(QUEEN, board->xstm))) &&
           !(GetRookAttacks(kingSq, occ) & (PieceBB(ROOK, board->xstm) | PieceBB(QUEEN, board->xstm)));
  }

  if (IsCas(move)) {
    int step = to > from ? -1 : 1;

    // pieces in-between have been checked, now check that it's not castling through or into check
    for (int i = to; i != from; i += step)
      if (AttacksToSquare(board, i, OccBB(BOTH)) & OccBB(board->xstm)) return 0;

    return !CHESS_960 || !getBit(board->pinned, To(move));
  }

  if (PieceType(Moving(move)) == KING)
    return !(AttacksToSquare(board, to, OccBB(BOTH) ^ PieceBB(KING, board->stm)) & OccBB(board->xstm));

  return !getBit(board->pinned, from) || getBit(PinnedMoves(from, kingSq), to);
}
