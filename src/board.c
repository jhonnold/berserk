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

// reset the board to an empty state
void ClearBoard(Board* board) {
  memset(board->pieces, 0, sizeof(board->pieces));
  memset(board->occupancies, 0, sizeof(board->occupancies));
  memset(board->history, 0, sizeof(board->history));

  for (int i = 0; i < 64; i++)
    board->squares[i] = NO_PIECE;

  board->piecesCounts = 0ULL;
  board->zobrist      = 0ULL;

  board->stm  = WHITE;
  board->xstm = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->histPly  = 0;
  board->moveNo   = 1;
  board->fmr      = 0;
  board->nullply  = 0;
  board->phase    = 0;
}

void ParseFen(char* fen, Board* board) {
  ClearBoard(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int) *fen];
      SetBit(board->pieces[piece], i);
      board->squares[i] = piece;

      board->phase += PHASE_VALUES[PieceType(piece)];

      if (*fen != 'K' && *fen != 'k')
        board->piecesCounts += PieceCount(piece);
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

  board->castling = 0;
  for (int i = 0; i < 4; i++)
    board->cr[i] = -1;

  int whiteKing = LSB(PieceBB(KING, WHITE));
  int blackKing = LSB(PieceBB(KING, BLACK));

  BitBoard whiteRooks = PieceBB(ROOK, WHITE) & RANK_1;
  BitBoard blackRooks = PieceBB(ROOK, BLACK) & RANK_8;

  while (*(++fen) != ' ') {
    if (*fen == 'K') {
      board->castling |= WHITE_KS, board->cr[0] = MSB(whiteRooks);
    } else if (*fen == 'Q') {
      board->castling |= WHITE_QS, board->cr[1] = LSB(whiteRooks);
    } else if (*fen >= 'A' && *fen <= 'H') {
      board->castling |= ((*fen - 'A') > File(whiteKing) ? WHITE_KS : WHITE_QS);
      board->cr[(*fen - 'A') > File(whiteKing) ? 0 : 1] = A1 + (*fen - 'A');
    } else if (*fen == 'k') {
      board->castling |= BLACK_KS, board->cr[2] = MSB(blackRooks);
    } else if (*fen == 'q') {
      board->castling |= BLACK_QS, board->cr[3] = LSB(blackRooks);
    } else if (*fen >= 'a' && *fen <= 'h') {
      board->castling |= ((*fen - 'a') > File(blackKing) ? BLACK_KS : BLACK_QS);
      board->cr[(*fen - 'a') > File(blackKing) ? 2 : 3] = A8 + (*fen - 'a');
    }
  }

  for (int i = 0; i < 64; i++) {
    board->castlingRights[i] = board->castling;

    if (i == whiteKing)
      board->castlingRights[i] ^= (WHITE_KS | WHITE_QS);
    else if (i == blackKing)
      board->castlingRights[i] ^= (BLACK_KS | BLACK_QS);
    else if (i == board->cr[0])
      board->castlingRights[i] ^= WHITE_KS;
    else if (i == board->cr[1])
      board->castlingRights[i] ^= WHITE_QS;
    else if (i == board->cr[2])
      board->castlingRights[i] ^= BLACK_KS;
    else if (i == board->cr[3])
      board->castlingRights[i] ^= BLACK_QS;
  }

  fen++;

  if (*fen != '-') {
    int f = fen[0] - 'a';
    int r = 8 - (fen[1] - '0');

    board->epSquare = r * 8 + f;
  } else {
    board->epSquare = 0;
  }

  while (*fen && *fen != ' ')
    fen++;

  sscanf(fen, " %d %d", &board->fmr, &board->moveNo);

  OccBB(WHITE) = OccBB(BLACK) = OccBB(BOTH) = 0;
  for (int i = WHITE_PAWN; i <= BLACK_KING; i++)
    OccBB(i & 1) |= board->pieces[i];
  OccBB(BOTH) = OccBB(WHITE) | OccBB(BLACK);

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
        if (c)
          *fen++ = c + '0'; // append the amount of empty sqs

        *fen++ = PIECE_TO_CHAR[piece];
        c      = 0;
      } else {
        c++;
      }
    }

    if (c)
      *fen++ = c + '0';

    *fen++ = (r == 7) ? ' ' : '/';
  }

  *fen++ = board->stm ? 'b' : 'w';
  *fen++ = ' ';

  if (board->castling) {
    if (board->castling & 0x8)
      *fen++ = CHESS_960 ? 'A' + File(board->cr[0]) : 'K';
    if (board->castling & 0x4)
      *fen++ = CHESS_960 ? 'A' + File(board->cr[1]) : 'Q';
    if (board->castling & 0x2)
      *fen++ = CHESS_960 ? 'a' + File(board->cr[2]) : 'k';
    if (board->castling & 0x1)
      *fen++ = CHESS_960 ? 'a' + File(board->cr[3]) : 'q';
  } else {
    *fen++ = '-';
  }

  *fen++ = ' ';

  sprintf(fen, "%s %d %d", board->epSquare ? SQ_TO_COORD[board->epSquare] : "-", board->fmr, board->moveNo);
}

void PrintBoard(Board* board) {
  static char fenBuffer[128];

  for (int r = 0; r < 8; r++) {
    printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
    printf("|");
    for (int f = 0; f < 16; f++) {
      if (f == 8)
        printf("\n|");

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
  return BitCount(OccBB(board->stm) ^ PieceBB(KING, board->stm) ^ PieceBB(PAWN, board->stm));
}

inline int IsOCB(Board* board) {
  BitBoard nonBishopMaterial = PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK) | PieceBB(ROOK, WHITE) |
                               PieceBB(ROOK, BLACK) | PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK);

  return !nonBishopMaterial && BitCount(PieceBB(BISHOP, WHITE)) == 1 && BitCount(PieceBB(BISHOP, BLACK)) == 1 &&
         BitCount((PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK)) & DARK_SQS) == 1;
}

// Special pieces are those giving check, and those that are pinned
// these must be recalculated every move for faster move legality purposes
inline void SetSpecialPieces(Board* board) {
  const int stm  = board->stm;
  const int xstm = board->xstm;

  int kingSq = LSB(PieceBB(KING, stm));

  board->pinned   = 0;
  board->checkers = (GetKnightAttacks(kingSq) & PieceBB(KNIGHT, xstm)) | // knight and
                    (GetPawnAttacks(kingSq, stm) & PieceBB(PAWN, xstm)); // pawns are easy

  BitBoard sliders = ((PieceBB(BISHOP, xstm) | PieceBB(QUEEN, xstm)) & GetBishopAttacks(kingSq, 0)) |
                     ((PieceBB(ROOK, xstm) | PieceBB(QUEEN, xstm)) & GetRookAttacks(kingSq, 0));
  while (sliders) {
    int sq = PopLSB(&sliders);

    BitBoard blockers = BetweenSquares(kingSq, sq) & OccBB(BOTH);
    if (!blockers)
      SetBit(board->checkers, sq);
    else if (BitCount(blockers) == 1)
      board->pinned |= (blockers & OccBB(stm));
  }
}

void MakeMove(Move move, Board* board) {
  MakeMoveUpdate(move, board, 1);
}

void MakeMoveUpdate(Move move, Board* board, int update) {
  int from     = From(move);
  int to       = To(move);
  int piece    = Moving(move);
  int captured = IsEP(move) ? Piece(PAWN, board->xstm) : board->squares[to];

  // store hard to recalculate values
  board->history[board->histPly].capture  = captured;
  board->history[board->histPly].castling = board->castling;
  board->history[board->histPly].ep       = board->epSquare;
  board->history[board->histPly].fmr      = board->fmr;
  board->history[board->histPly].nullply  = board->nullply;
  board->history[board->histPly].zobrist  = board->zobrist;
  board->history[board->histPly].checkers = board->checkers;
  board->history[board->histPly].pinned   = board->pinned;

  board->fmr++;
  board->nullply++;

  FlipBits(board->pieces[piece], from, to);
  FlipBits(OccBB(board->stm), from, to);
  FlipBits(OccBB(BOTH), from, to);

  board->squares[from] = NO_PIECE;
  board->squares[to]   = piece;

  board->zobrist ^= ZOBRIST_PIECES[piece][from] ^ ZOBRIST_PIECES[piece][to];

  if (IsCas(move)) {
    int rookFrom = board->cr[CASTLING_ROOK[to]];
    int rookTo   = CASTLE_ROOK_DEST[to];
    int rook     = Piece(ROOK, board->stm);

    FlipBits(PieceBB(ROOK, board->stm), rookFrom, rookTo);
    FlipBits(OccBB(board->stm), rookFrom, rookTo);
    FlipBits(OccBB(BOTH), rookFrom, rookTo);

    // chess960 can have the king going where the rook started
    // do this check to prevent accidental wipe outs
    if (rookFrom != to)
      board->squares[rookFrom] = NO_PIECE;
    board->squares[rookTo] = rook;

    board->zobrist ^= ZOBRIST_PIECES[rook][rookFrom] ^ ZOBRIST_PIECES[rook][rookTo];
  } else if (IsCap(move)) {
    int capSq = IsEP(move) ? to - PawnDir(board->stm) : to;
    if (IsEP(move))
      board->squares[capSq] = NO_PIECE;

    FlipBit(board->pieces[captured], capSq);
    FlipBit(OccBB(board->xstm), capSq);
    FlipBit(OccBB(BOTH), capSq);

    board->zobrist ^= ZOBRIST_PIECES[captured][capSq];

    board->piecesCounts -= PieceCount(captured);
    board->phase -= PHASE_VALUES[PieceType(captured)];
    board->fmr = 0;
  }

  if (board->epSquare)
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  if (board->castling) {
    board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
    board->castling &= board->castlingRights[from];
    board->castling &= board->castlingRights[to];
    board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  }

  if (PieceType(piece) == PAWN) {
    if (IsDP(move)) {
      int epSquare = to - PawnDir(board->stm);

      if (GetPawnAttacks(epSquare, board->stm) & PieceBB(PAWN, board->xstm)) {
        board->epSquare = epSquare;
        board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
      }
    } else if (Promo(move)) {
      int promoted = Promo(move);
      FlipBit(board->pieces[piece], to);
      FlipBit(board->pieces[promoted], to);

      board->squares[to] = promoted;

      board->zobrist ^= ZOBRIST_PIECES[piece][to] ^ ZOBRIST_PIECES[promoted][to];
      board->piecesCounts += PieceCount(promoted) - PieceCount(piece);
      board->phase += PHASE_VALUES[PieceType(promoted)];
    }

    board->fmr = 0;
  }

  board->histPly++;
  board->moveNo += (board->stm == BLACK);
  board->xstm = board->stm;
  board->stm ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;

  // special pieces must be loaded after the stm has changed
  // this is because the new stm to move will be the one in check
  SetSpecialPieces(board);

  if (update) {
    board->accumulators->move     = move;
    board->accumulators->captured = captured;

    board->accumulators++;
    board->accumulators->correct[WHITE] = board->accumulators->correct[BLACK] = 0;
  }
}

void UndoMove(Move move, Board* board) {
  int from  = From(move);
  int to    = To(move);
  int piece = Moving(move);

  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;
  board->moveNo -= (board->stm == BLACK);
  board->accumulators--;

  // reload historical values
  board->castling = board->history[board->histPly].castling;
  board->epSquare = board->history[board->histPly].ep;
  board->fmr      = board->history[board->histPly].fmr;
  board->nullply  = board->history[board->histPly].nullply;
  board->zobrist  = board->history[board->histPly].zobrist;
  board->checkers = board->history[board->histPly].checkers;
  board->pinned   = board->history[board->histPly].pinned;

  if (Promo(move)) {
    int promoted = Promo(move);
    FlipBit(board->pieces[piece], to);
    FlipBit(board->pieces[promoted], to);
    board->squares[to] = piece;
    board->piecesCounts -= PieceCount(promoted) - PieceCount(piece);
    board->phase -= PHASE_VALUES[PieceType(promoted)];
  }

  FlipBits(board->pieces[piece], to, from);
  FlipBits(OccBB(board->stm), to, from);
  FlipBits(OccBB(BOTH), to, from);

  board->squares[from] = piece;
  board->squares[to]   = NO_PIECE;

  if (IsCas(move)) {
    int rookFrom = board->cr[CASTLING_ROOK[to]];
    int rookTo   = CASTLE_ROOK_DEST[to];
    int rook     = Piece(ROOK, board->stm);

    FlipBits(PieceBB(ROOK, board->stm), rookTo, rookFrom);
    FlipBits(OccBB(board->stm), rookTo, rookFrom);
    FlipBits(OccBB(BOTH), rookTo, rookFrom);

    if (from != rookTo)
      board->squares[rookTo] = NO_PIECE;
    board->squares[rookFrom] = rook;
  } else if (IsCap(move)) {
    int capSq    = IsEP(move) ? to - PawnDir(board->stm) : to;
    int captured = board->history[board->histPly].capture;

    FlipBit(board->pieces[captured], capSq);
    FlipBit(OccBB(board->xstm), capSq);
    FlipBit(OccBB(BOTH), capSq);

    board->squares[capSq] = captured;

    board->piecesCounts += PieceCount(captured);
    board->phase += PHASE_VALUES[PieceType(captured)];
  }
}

void MakeNullMove(Board* board) {
  board->history[board->histPly].capture  = NO_PIECE;
  board->history[board->histPly].castling = board->castling;
  board->history[board->histPly].ep       = board->epSquare;
  board->history[board->histPly].fmr      = board->fmr;
  board->history[board->histPly].nullply  = board->nullply;
  board->history[board->histPly].zobrist  = board->zobrist;
  board->history[board->histPly].checkers = board->checkers;
  board->history[board->histPly].pinned   = board->pinned;

  board->fmr++;
  board->nullply = 0;

  if (board->epSquare)
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;

  board->histPly++;
  board->stm = board->xstm;
  board->xstm ^= 1;

  SetSpecialPieces(board);
}

void UndoNullMove(Board* board) {
  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;

  // reload historical values
  board->castling = board->history[board->histPly].castling;
  board->epSquare = board->history[board->histPly].ep;
  board->fmr      = board->history[board->histPly].fmr;
  board->nullply  = board->history[board->histPly].nullply;
  board->zobrist  = board->history[board->histPly].zobrist;
  board->checkers = board->history[board->histPly].checkers;
  board->pinned   = board->history[board->histPly].pinned;
}

inline int IsDraw(Board* board, int ply) {
  return IsRepetition(board, ply) || IsMaterialDraw(board) || IsFiftyMoveRule(board);
}

inline int IsRepetition(Board* board, int ply) {
  int reps = 0, distance = Min(board->fmr, board->nullply);

  // Check as far back as the last non-reversible move
  for (int i = board->histPly - 4; i >= 0 && i >= board->histPly - distance; i -= 2) {
    if (board->history[i].zobrist == board->zobrist) {
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
  if (board->fmr > 99) {
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

  if (!move || (piece & 1) != board->stm || piece != board->squares[from])
    return 0;

  if (IsCas(move)) {
    if (board->checkers || !board->castling)
      return 0;

    int kingsq = LSB(PieceBB(KING, board->stm));

    switch (to) {
      case G1: {
        if (!CanCastle(WHITE_KS))
          return 0;
        if (GetBit(board->pinned, board->cr[0]))
          return 0;
        BitBoard between = BetweenSquares(kingsq, G1) | BetweenSquares(board->cr[0], F1) | Bit(G1) | Bit(F1);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ Bit(board->cr[0])) & between)
          return 0;
        break;
      }
      case C1: {
        if (!CanCastle(WHITE_QS))
          return 0;
        if (GetBit(board->pinned, board->cr[1]))
          return 0;
        BitBoard between = BetweenSquares(kingsq, C1) | BetweenSquares(board->cr[1], D1) | Bit(C1) | Bit(D1);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ Bit(board->cr[1])) & between)
          return 0;
        break;
      }
      case G8: {
        if (!CanCastle(BLACK_KS))
          return 0;
        if (GetBit(board->pinned, board->cr[2]))
          return 0;
        BitBoard between = BetweenSquares(kingsq, G8) | BetweenSquares(board->cr[2], F8) | Bit(G8) | Bit(F8);
        if ((OccBB(BOTH) ^ PieceBB(KING, BLACK) ^ Bit(board->cr[2])) & between)
          return 0;
        break;
      }
      case C8: {
        if (!CanCastle(BLACK_QS))
          return 0;
        if (GetBit(board->pinned, board->cr[3]))
          return 0;
        BitBoard between = BetweenSquares(kingsq, C8) | BetweenSquares(board->cr[3], D8) | Bit(C8) | Bit(D8);
        if ((OccBB(BOTH) ^ PieceBB(KING, WHITE) ^ Bit(board->cr[3])) & between)
          return 0;
        break;
      }
    }

    return 1;
  }

  if (Promo(move)) {
    if (BitCount(board->checkers) > 1)
      return 0;

    int king       = LSB(PieceBB(KING, board->stm));
    BitBoard valid = board->checkers ? (board->checkers | BetweenSquares(king, LSB(board->checkers))) : ALL;
    BitBoard goal  = board->stm == WHITE ? RANK_8 : RANK_1;
    BitBoard opts =
      (ShiftPawnDir(Bit(from), board->stm) & ~OccBB(BOTH)) | (GetPawnAttacks(from, board->stm) & OccBB(board->xstm));

    return !!GetBit(goal & opts & valid, to);
  }

  if (IsCap(move) && !IsEP(move) && board->squares[to] == NO_PIECE)
    return 0;
  if (!IsCap(move) && board->squares[to] != NO_PIECE)
    return 0;
  if (IsEP(move) && to != board->epSquare)
    return 0;
  if (GetBit(OccBB(board->stm), to))
    return 0;

  if (pcType == PAWN) {
    if (GetBit(RANK_1 | RANK_8, to))
      return 0;

    if (!IsEP(move)) {
      if (!(GetPawnAttacks(from, board->stm) & OccBB(board->xstm) & Bit(to))         // capture
          && !((from + PawnDir(board->stm) == to) && board->squares[to] == NO_PIECE) // single push
          && !((from + 2 * PawnDir(board->stm) == to)                                // double push
               && board->squares[to] == NO_PIECE                                     //
               && board->squares[to - PawnDir(board->stm)] == NO_PIECE               //
               && (Rank(from ^ (56 * board->stm)) == 6)                              //
               ))
        return 0;
    }
  } else if (!GetBit(GetPieceAttacks(from, OccBB(BOTH), pcType), to)) {
    return 0;
  }

  if (board->checkers && pcType != KING) {
    if (BitCount(board->checkers) > 1)
      return 0;

    int kingsq                 = LSB(PieceBB(KING, board->stm));
    BitBoard blocksAndCaptures = board->checkers | BetweenSquares(kingsq, LSB(board->checkers));
    int to2                    = IsEP(move) ? to - PawnDir(board->stm) : to;
    if (!GetBit(blocksAndCaptures, to2))
      return 0;
  }

  return 1;
}

// this is NOT a legality checker for ALL moves
// it is only used by move generation for king moves, castles, and ep captures
int IsLegal(Move move, Board* board) {
  int from   = From(move);
  int to     = To(move);
  int kingSq = LSB(PieceBB(KING, board->stm));

  if (IsEP(move)) {
    // ep is checked by just applying the move
    int captureSq = to - PawnDir(board->stm);
    BitBoard occ  = (OccBB(BOTH) ^ Bit(from) ^ Bit(captureSq)) | Bit(to);
    // EP can only be illegal due to crazy discover checks
    return !(GetBishopAttacks(kingSq, occ) & (PieceBB(BISHOP, board->xstm) | PieceBB(QUEEN, board->xstm))) &&
           !(GetRookAttacks(kingSq, occ) & (PieceBB(ROOK, board->xstm) | PieceBB(QUEEN, board->xstm)));
  }

  if (IsCas(move)) {
    int step = to > from ? -1 : 1;

    // pieces in-between have been checked, now check that it's not castling
    // through or into check
    for (int i = to; i != from; i += step)
      if (AttacksToSquare(board, i, OccBB(BOTH)) & OccBB(board->xstm))
        return 0;

    return 1;
  }

  if (PieceType(Moving(move)) == KING)
    return !(AttacksToSquare(board, to, OccBB(BOTH) ^ PieceBB(KING, board->stm)) & OccBB(board->xstm));

  return !GetBit(board->pinned, from) || GetBit(PinnedMoves(from, kingSq), to);
}

uint64_t cuckoo[8192];
Move cuckooMove[8192];

inline uint64_t Hash1(uint64_t hash) {
  return hash & 0x1fff;
}

inline uint64_t Hash2(uint64_t hash) {
  return (hash >> 16) & 0x1fff;
}

void InitCuckoo() {
  int validate = 0;

  for (int pc = WHITE_PAWN; pc <= BLACK_KING; pc++) {
    int pcType = PieceType(pc);
    if (pcType == PAWN)
      continue;

    for (int s1 = 0; s1 < 64; s1++) {
      for (int s2 = s1 + 1; s2 < 64; s2++) {
        if (!GetBit(GetPieceAttacks(s1, 0, pcType), s2))
          continue;

        Move move     = BuildMove(s1, s2, pc, NO_PROMO, QUIET);
        uint64_t hash = ZOBRIST_PIECES[pc][s1] ^ ZOBRIST_PIECES[pc][s2] ^ ZOBRIST_SIDE_KEY;

        uint32_t i = Hash1(hash);
        while (1) {
          uint64_t temp = cuckoo[i];
          cuckoo[i]     = hash;
          hash          = temp;

          Move tempMove = cuckooMove[i];
          cuckooMove[i] = move;
          move          = tempMove;

          if (move == 0)
            break;

          i = (i == Hash1(hash)) ? Hash2(hash) : Hash1(hash);
        }

        validate++;
      }
    }
  }

  if (validate != 3668)
    printf("Failed to set cuckoo tables.\n"), exit(1);
}

// Upcoming repetition detection
// http://www.open-chess.org/viewtopic.php?f=5&t=2300
// Implemented originally in SF
// Paper no longer seems accessible @ http://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
int HasCycle(Board* board, int ply) {
  int distance = Min(board->fmr, board->nullply);
  if (distance < 3)
    return 0;

  uint64_t original  = board->zobrist;
  BoardHistory* prev = &board->history[board->histPly - 1];

  for (int i = 3; i <= distance; i += 2) {
    prev -= 2;

    uint32_t h;
    uint64_t moveKey = original ^ prev->zobrist;
    if ((h = Hash1(moveKey), cuckoo[h] == moveKey) || (h = Hash2(moveKey), cuckoo[h] == moveKey)) {
      Move move        = cuckooMove[h];
      BitBoard between = BetweenSquares(From(move), To(move));
      if (between & OccBB(BOTH))
        continue;

      if (ply > i)
        return 1;

      int pc = board->squares[From(move)] != NO_PIECE ? board->squares[From(move)] : board->squares[To(move)];
      if ((pc & 1) != board->stm)
        continue;

      BoardHistory* prev2 = prev - 2;
      for (int j = i + 4; j <= distance; j += 2) {
        prev2 -= 2;
        if (prev2->zobrist == prev->zobrist)
          return 1;
      }
    }
  }

  return 0;
}