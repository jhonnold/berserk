// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "nn/threats.h"
#include "options.h"
#include "types.h"
#include "zobrist.h"

// MakeMove and UndoMove copy Board's leading fields to and from a BoardHistory
// entry with one memcpy, so the two layouts must agree up to Board.stm
#define SAME_OFFSET(b, h) _Static_assert(offsetof(Board, b) == offsetof(BoardHistory, h), #b " != " #h)
SAME_OFFSET(castling, castling);
SAME_OFFSET(epSquare, ep);
SAME_OFFSET(fmr, fmr);
SAME_OFFSET(nullply, nullply);
SAME_OFFSET(zobrist, zobrist);
SAME_OFFSET(pawnZobrist, pawnZobrist);
SAME_OFFSET(nonPawnZobrist, nonPawnZobrist);
SAME_OFFSET(checkers, checkers);
SAME_OFFSET(pinned, pinned);
SAME_OFFSET(threatened, threatened);
SAME_OFFSET(threatenedBy, threatenedBy);
SAME_OFFSET(stm, capture);
#undef SAME_OFFSET

// reset the board to an empty state
static void ClearBoard(Board* board) {
  memset(board->pieces, 0, sizeof(board->pieces));
  memset(board->occupancies, 0, sizeof(board->occupancies));
  memset(board->history, 0, sizeof(board->history));

  for (int i = 0; i < 64; i++)
    board->squares[i] = NO_PIECE;

  board->piecesCounts          = 0ULL;
  board->zobrist               = 0ULL;
  board->pawnZobrist           = 0ULL;
  board->nonPawnZobrist[WHITE] = board->nonPawnZobrist[BLACK] = 0ULL;

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

// these must be recalculated every move for faster move legality purposes
INLINE void SetSpecialPieces(Board* board) {
  const int stm  = board->stm;
  const int xstm = board->xstm;

  int kingSq = LSB(PieceBB(board, KING, stm));

  board->pinned   = 0;
  board->checkers = (GetKnightAttacks(kingSq) & PieceBB(board, KNIGHT, xstm)) | // knight and
                    (GetPawnAttacks(kingSq, stm) & PieceBB(board, PAWN, xstm)); // pawns are easy

  BitBoard sliders = ((PieceBB(board, BISHOP, xstm) | PieceBB(board, QUEEN, xstm)) & BISHOP_RAYS[kingSq]) |
                     ((PieceBB(board, ROOK, xstm) | PieceBB(board, QUEEN, xstm)) & ROOK_RAYS[kingSq]);
  while (sliders) {
    int sq = PopLSB(&sliders);

    BitBoard blockers = BetweenSquares(kingSq, sq) & OccBB(board, BOTH);
    if (!blockers)
      SetBit(board->checkers, sq);
    else if (BitCount(blockers) == 1)
      board->pinned |= (blockers & OccBB(board, stm));
  }
}

INLINE void SetThreats(Board* board) {
  // Invert these to be confusing.
  const int stm  = board->xstm;
  const int xstm = board->stm;

  // Take the enemy king off for through threats.
  BitBoard occ = OccBB(board, BOTH) ^ PieceBB(board, KING, xstm);

  BitBoard pawnAttacks = stm == WHITE ? ShiftNW(PieceBB(board, PAWN, WHITE)) | ShiftNE(PieceBB(board, PAWN, WHITE)) :
                                        ShiftSW(PieceBB(board, PAWN, BLACK)) | ShiftSE(PieceBB(board, PAWN, BLACK));
  board->threatenedBy[PAWN] = pawnAttacks;
  board->threatened         = board->threatenedBy[PAWN];

  board->threatenedBy[KNIGHT] = 0;
  BitBoard knights            = PieceBB(board, KNIGHT, stm);
  while (knights)
    board->threatenedBy[KNIGHT] |= GetKnightAttacks(PopLSB(&knights));
  board->threatened |= board->threatenedBy[KNIGHT];

  board->threatenedBy[BISHOP] = 0;
  BitBoard bishops            = PieceBB(board, BISHOP, stm);
  while (bishops)
    board->threatenedBy[BISHOP] |= GetBishopAttacks(PopLSB(&bishops), occ);
  board->threatened |= board->threatenedBy[BISHOP];

  board->threatenedBy[ROOK] = 0;
  BitBoard rooks            = PieceBB(board, ROOK, stm);
  while (rooks)
    board->threatenedBy[ROOK] |= GetRookAttacks(PopLSB(&rooks), occ);
  board->threatened |= board->threatenedBy[ROOK];

  board->threatenedBy[QUEEN] = 0;
  BitBoard queens            = PieceBB(board, QUEEN, stm);
  while (queens)
    board->threatenedBy[QUEEN] |= GetQueenAttacks(PopLSB(&queens), occ);
  board->threatened |= board->threatenedBy[QUEEN];

  board->threatenedBy[KING] = GetKingAttacks(LSB(PieceBB(board, KING, stm)));
  board->threatened |= board->threatenedBy[KING];
}

void ParseFen(const char* fen, Board* board) {
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

  int whiteKing = LSB(PieceBB(board, KING, WHITE));
  int blackKing = LSB(PieceBB(board, KING, BLACK));

  BitBoard whiteRooks = PieceBB(board, ROOK, WHITE) & RANK_1;
  BitBoard blackRooks = PieceBB(board, ROOK, BLACK) & RANK_8;

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

  OccBB(board, WHITE) = OccBB(board, BLACK) = OccBB(board, BOTH) = 0;
  for (int i = WHITE_PAWN; i <= BLACK_KING; i++)
    OccBB(board, i & 1) |= board->pieces[i];
  OccBB(board, BOTH) = OccBB(board, WHITE) | OccBB(board, BLACK);

  SetSpecialPieces(board);
  SetThreats(board);

  board->zobrist               = Zobrist(board);
  board->pawnZobrist           = PawnZobrist(board);
  board->nonPawnZobrist[WHITE] = NonPawnZobrist(board, WHITE);
  board->nonPawnZobrist[BLACK] = NonPawnZobrist(board, BLACK);
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

INLINE void RemovePiece(Board* board, int pc, int s) {
  FlipBit(board->pieces[pc], s);
  FlipBit(OccBB(board, pc & 1), s);
  FlipBit(OccBB(board, BOTH), s);
  board->squares[s] = NO_PIECE;
}

INLINE void AddPiece(Board* board, int pc, int s) {
  FlipBit(board->pieces[pc], s);
  FlipBit(OccBB(board, pc & 1), s);
  FlipBit(OccBB(board, BOTH), s);
  board->squares[s] = pc;
}

INLINE void SwapPiece(Board* board, int old, int new, int s) {
  FlipBit(board->pieces[old], s);
  FlipBit(OccBB(board, old & 1), s);
  FlipBit(board->pieces[new], s);
  FlipBit(OccBB(board, new & 1), s);
  board->squares[s] = new;
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
  memcpy(&board->history[board->histPly], board, offsetof(Board, stm));
  board->history[board->histPly].capture = captured;

  // without an update the threats are still found, so perft runs search's board code
  uint32_t scratch[MAX_DIRTY_THREATS];
  DirtyThreatList list = {update ? board->accumulators->dirty : scratch, 0};
  const int stm        = piece & 1;

  if (update) {
    board->accumulators->pawns[0][WHITE] = PieceBB(board, PAWN, WHITE);
    board->accumulators->pawns[0][BLACK] = PieceBB(board, PAWN, BLACK);
  }

  board->fmr++;
  board->nullply++;

  board->zobrist ^= ZOBRIST_PIECES[piece][from] ^ ZOBRIST_PIECES[piece][to];
  if (PieceType(piece) == PAWN)
    board->pawnZobrist ^= ZOBRIST_PIECES[piece][from] ^ ZOBRIST_PIECES[piece][to];
  else
    board->nonPawnZobrist[stm] ^= ZOBRIST_PIECES[piece][from] ^ ZOBRIST_PIECES[piece][to];

  // each piece leaves before the next arrives, so lines through its square are open
  if (IsCas(move)) {
    const int rookFrom = board->cr[CASTLING_ROOK[to]];
    const int rookTo   = CASTLE_ROOK_DEST[to];
    const int rook     = Piece(ROOK, stm);

    PieceThreats(board, &list, piece, from, SUB);
    RemovePiece(board, piece, from);
    PieceThreats(board, &list, rook, rookFrom, SUB);
    RemovePiece(board, rook, rookFrom);
    AddPiece(board, piece, to);
    PieceThreats(board, &list, piece, to, ADD);
    AddPiece(board, rook, rookTo);
    PieceThreats(board, &list, rook, rookTo, ADD);

    board->zobrist ^= ZOBRIST_PIECES[rook][rookFrom] ^ ZOBRIST_PIECES[rook][rookTo];
    board->nonPawnZobrist[stm] ^= ZOBRIST_PIECES[rook][rookFrom] ^ ZOBRIST_PIECES[rook][rookTo];
  } else {
    const int placed = IsPromo(move) ? PromoPiece(move, stm) : piece;

    PieceThreats(board, &list, piece, from, SUB);
    RemovePiece(board, piece, from);

    if (IsCap(move)) {
      const int capSq = IsEP(move) ? to - PawnDir(stm) : to;

      if (IsEP(move)) {
        PieceThreats(board, &list, captured, capSq, SUB);
        RemovePiece(board, captured, capSq);
        AddPiece(board, placed, to);
        PieceThreats(board, &list, placed, to, ADD);
      } else {
        SwapThreats(board, &list, captured, placed, to);
        SwapPiece(board, captured, placed, to);
      }

      board->zobrist ^= ZOBRIST_PIECES[captured][capSq];
      if (PieceType(captured) == PAWN)
        board->pawnZobrist ^= ZOBRIST_PIECES[captured][capSq];
      else
        board->nonPawnZobrist[stm ^ 1] ^= ZOBRIST_PIECES[captured][capSq];

      board->piecesCounts -= PieceCount(captured);
      board->phase -= PHASE_VALUES[PieceType(captured)];
      board->fmr = 0;
    } else {
      AddPiece(board, placed, to);
      PieceThreats(board, &list, placed, to, ADD);
    }
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
    if ((from ^ to) == 16) {
      int epSquare = to - PawnDir(board->stm);

      if (GetPawnAttacks(epSquare, board->stm) & PieceBB(board, PAWN, board->xstm)) {
        board->epSquare = epSquare;
        board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
      }
    } else if (IsPromo(move)) {
      int promoted = PromoPiece(move, board->stm);

      board->zobrist ^= ZOBRIST_PIECES[piece][to] ^ ZOBRIST_PIECES[promoted][to];
      board->pawnZobrist ^= ZOBRIST_PIECES[piece][to];
      board->nonPawnZobrist[stm] ^= ZOBRIST_PIECES[promoted][to];
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
  board->pawnZobrist ^= ZOBRIST_SIDE_KEY;
  board->nonPawnZobrist[WHITE] ^= ZOBRIST_SIDE_KEY;
  board->nonPawnZobrist[BLACK] ^= ZOBRIST_SIDE_KEY;

  // special pieces must be loaded after the stm has changed
  // this is because the new stm to move will be the one in check
  SetSpecialPieces(board);
  SetThreats(board);

  if (update) {
    board->accumulators->move            = move;
    board->accumulators->captured        = captured;
    board->accumulators->nDirty          = list.n;
    board->accumulators->pawns[1][WHITE] = PieceBB(board, PAWN, WHITE);
    board->accumulators->pawns[1][BLACK] = PieceBB(board, PAWN, BLACK);

    board->accumulators++;
    board->accumulators->correct[WHITE] = board->accumulators->correct[BLACK] = 0;
  }
}

void UndoMove(Move move, Board* board) {
  UndoMoveUpdate(move, board, 1);
}

// update must match the MakeMoveUpdate call being undone: without one the
// accumulator stack was never advanced, so it must not be stepped back
void UndoMoveUpdate(Move move, Board* board, int update) {
  int from  = From(move);
  int to    = To(move);
  int piece = Moving(move);

  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;
  board->moveNo -= (board->stm == BLACK);
  if (update)
    board->accumulators--;

  // reload historical values
  memcpy(board, &board->history[board->histPly], offsetof(Board, stm));

  if (IsPromo(move)) {
    int promoted = PromoPiece(move, board->stm);
    FlipBit(board->pieces[piece], to);
    FlipBit(board->pieces[promoted], to);
    board->squares[to] = piece;
    board->piecesCounts -= PieceCount(promoted) - PieceCount(piece);
    board->phase -= PHASE_VALUES[PieceType(promoted)];
  }

  FlipBits(board->pieces[piece], to, from);
  FlipBits(OccBB(board, board->stm), to, from);
  FlipBits(OccBB(board, BOTH), to, from);

  board->squares[to]   = NO_PIECE;
  board->squares[from] = piece;

  if (IsCas(move)) {
    int rookFrom = board->cr[CASTLING_ROOK[to]];
    int rookTo   = CASTLE_ROOK_DEST[to];
    int rook     = Piece(ROOK, board->stm);

    FlipBits(PieceBB(board, ROOK, board->stm), rookTo, rookFrom);
    FlipBits(OccBB(board, board->stm), rookTo, rookFrom);
    FlipBits(OccBB(board, BOTH), rookTo, rookFrom);

    if (from != rookTo)
      board->squares[rookTo] = NO_PIECE;
    board->squares[rookFrom] = rook;
  } else if (IsCap(move)) {
    int capSq    = IsEP(move) ? to - PawnDir(board->stm) : to;
    int captured = board->history[board->histPly].capture;

    FlipBit(board->pieces[captured], capSq);
    FlipBit(OccBB(board, board->xstm), capSq);
    FlipBit(OccBB(board, BOTH), capSq);

    board->squares[capSq] = captured;

    board->piecesCounts += PieceCount(captured);
    board->phase += PHASE_VALUES[PieceType(captured)];
  }
}

void MakeNullMove(Board* board) {
  memcpy(&board->history[board->histPly], board, offsetof(Board, stm));

  board->fmr++;
  board->nullply = 0;

  if (board->epSquare)
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;
  board->pawnZobrist ^= ZOBRIST_SIDE_KEY;
  board->nonPawnZobrist[WHITE] ^= ZOBRIST_SIDE_KEY;
  board->nonPawnZobrist[BLACK] ^= ZOBRIST_SIDE_KEY;

  board->histPly++;
  board->stm = board->xstm;
  board->xstm ^= 1;

  SetSpecialPieces(board);
  SetThreats(board);
}

void UndoNullMove(Board* board) {
  board->stm = board->xstm;
  board->xstm ^= 1;
  board->histPly--;

  // reload historical values
  memcpy(board, &board->history[board->histPly], offsetof(Board, stm));
}

int IsFiftyMoveRule(Board* board) {
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

    int idx = 2 * board->stm + (File(to) != 6); // 0 = G1, 1 = C1, 2 = G8, 3 = C8
    if (!CanCastle(board, CASTLE_MAP[idx][0]))
      return 0;
    if (GetBit(board->pinned, board->cr[idx]))
      return 0;

    int rookTo            = CASTLE_MAP[idx][2];
    BitBoard kingCrossing = BetweenSquares(from, to) | Bit(to);
    BitBoard rookCrossing = BetweenSquares(board->cr[idx], rookTo) | Bit(rookTo);
    BitBoard between      = kingCrossing | rookCrossing;

    if ((OccBB(board, BOTH) ^ Bit(from) ^ Bit(board->cr[idx])) & between)
      return 0;
    if (kingCrossing & board->threatened)
      return 0;

    return 1;
  }

  if (IsPromo(move)) {
    if (BitCount(board->checkers) > 1)
      return 0;

    int king       = LSB(PieceBB(board, KING, board->stm));
    BitBoard valid = board->checkers ? (board->checkers | BetweenSquares(king, LSB(board->checkers))) : ALL;
    BitBoard goal  = board->stm == WHITE ? RANK_8 : RANK_1;
    BitBoard opts  = (ShiftPawnDir(Bit(from), board->stm) & ~OccBB(board, BOTH)) |
                    (GetPawnAttacks(from, board->stm) & OccBB(board, board->xstm));

    return !!GetBit(goal & opts & valid, to);
  }

  if (IsCap(move) && !IsEP(move) && board->squares[to] == NO_PIECE)
    return 0;
  if (!IsCap(move) && board->squares[to] != NO_PIECE)
    return 0;
  if (IsEP(move) && to != board->epSquare)
    return 0;
  if (GetBit(OccBB(board, board->stm), to))
    return 0;
  if (pcType == KING && GetBit(board->threatened, to))
    return 0;

  if (pcType == PAWN) {
    if (GetBit(RANK_1 | RANK_8, to))
      return 0;

    if (!IsEP(move)) {
      if (!(GetPawnAttacks(from, board->stm) & OccBB(board, board->xstm) & Bit(to))  // capture
          && !((from + PawnDir(board->stm) == to) && board->squares[to] == NO_PIECE) // single push
          && !((from + 2 * PawnDir(board->stm) == to)                                // double push
               && board->squares[to] == NO_PIECE                                     //
               && board->squares[to - PawnDir(board->stm)] == NO_PIECE               //
               && (Rank(from ^ (56 * board->stm)) == 6)                              //
               ))
        return 0;
    }
  } else if (!GetBit(GetPieceAttacks(from, OccBB(board, BOTH), pcType), to)) {
    return 0;
  }

  if (board->checkers && pcType != KING) {
    if (BitCount(board->checkers) > 1)
      return 0;

    int kingsq                 = LSB(PieceBB(board, KING, board->stm));
    BitBoard blocksAndCaptures = board->checkers | BetweenSquares(kingsq, LSB(board->checkers));
    int to2                    = IsEP(move) ? to - PawnDir(board->stm) : to;
    if (!GetBit(blocksAndCaptures, to2))
      return 0;
  }

  return 1;
}

// this is NOT a legality checker for ALL moves
// it is only used for final verification of EP and pinned moves.
int IsLegal(Move move, Board* board) {
  int from   = From(move);
  int to     = To(move);
  int kingSq = LSB(PieceBB(board, KING, board->stm));

  if (IsEP(move)) {
    // ep is checked by just applying the move
    int captureSq = to - PawnDir(board->stm);
    BitBoard occ  = (OccBB(board, BOTH) ^ Bit(from) ^ Bit(captureSq)) | Bit(to);
    // EP can only be illegal due to crazy discover checks
    return !(GetBishopAttacks(kingSq, occ) &
             (PieceBB(board, BISHOP, board->xstm) | PieceBB(board, QUEEN, board->xstm))) &&
           !(GetRookAttacks(kingSq, occ) & (PieceBB(board, ROOK, board->xstm) | PieceBB(board, QUEEN, board->xstm)));
  }

  return !GetBit(board->pinned, from) || GetBit(PinnedMoves(from, kingSq), to);
}

static uint64_t cuckoo[8192];
static Move cuckooMove[8192];

INLINE uint64_t Hash1(uint64_t hash) {
  return hash & 0x1fff;
}

INLINE uint64_t Hash2(uint64_t hash) {
  return (hash >> 16) & 0x1fff;
}

void InitCuckoo(void) {
  int validate = 0;

  for (int pc = WHITE_PAWN; pc <= BLACK_KING; pc++) {
    int pcType = PieceType(pc);
    if (pcType == PAWN)
      continue;

    for (int s1 = 0; s1 < 64; s1++) {
      for (int s2 = s1 + 1; s2 < 64; s2++) {
        if (!GetBit(GetPieceAttacks(s1, 0, pcType), s2))
          continue;

        Move move     = BuildMove(s1, s2, pc, QUIET_FLAG);
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
      if (between & OccBB(board, BOTH))
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