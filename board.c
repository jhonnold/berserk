#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "types.h"

const int charToPieceIdx[] = {['P'] = 0, ['N'] = 2, ['B'] = 4, ['R'] = 6, ['Q'] = 8, ['K'] = 10,
                              ['p'] = 1, ['n'] = 3, ['b'] = 5, ['r'] = 7, ['q'] = 9, ['k'] = 11};
const char* idxToCord[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};
const char* pieceChars = "PpNnBbRrQqKk";
const int castlingRights[] = {14, 15, 15, 15, 12, 15, 15, 13, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                              15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                              15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 11, 15, 15, 15, 3,  15, 15, 7};

void clear(Board* board) {
  memset(board->pieces, EMPTY, sizeof(board->pieces));
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));

  board->side = WHITE;
  board->xside = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
}

void setOccupancies(Board* board) {
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));

  for (int i = 0; i < 12; i++)
    board->occupancies[i & 1] |= board->pieces[i];
  board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

void setSpecialPieces(Board* board) {
  int kingSq = lsb(board->pieces[KING[board->side]]);

  board->pinners = EMPTY;
  board->checkers = (getKnightAttacks(kingSq) & board->pieces[KNIGHT[board->xside]]) |
                    (getPawnAttacks(kingSq, board->side) & board->pieces[PAWN[board->xside]]);

  for (int kingColor = WHITE; kingColor <= BLACK; kingColor++) {
    int enemyColor = 1 - kingColor;
    kingSq = lsb(board->pieces[KING[kingColor]]);

    BitBoard enemyPiece =
        ((board->pieces[BISHOP[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & getBishopAttacks(kingSq, 0)) |
        ((board->pieces[ROOK[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & getRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      BitBoard blockers = getInBetween(kingSq, sq) & board->occupancies[BOTH];

      if (!blockers)
        board->checkers |= (enemyPiece & -enemyPiece);
      else if (bits(blockers) == 1)
        board->pinners |= (blockers & board->occupancies[kingColor]);

      popLsb(enemyPiece);
    }
  }
}

void parseFen(char* fen, Board* board) {
  clear(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = charToPieceIdx[(int)*fen];
      setBit(board->pieces[piece], i);
    } else if (*fen >= '0' && *fen <= '9') {
      int offset = *fen - '1';
      i += offset;
    } else if (*fen == '/') {
      i--;
    }

    fen++;
  }

  fen++;

  board->side = (*fen++ == 'w' ? 0 : 1);
  board->xside = board->side ^ 1;

  fen++;

  while (*fen != ' ') {
    if (*fen == 'K') {
      board->castling |= 8;
    } else if (*fen == 'Q') {
      board->castling |= 4;
    } else if (*fen == 'k') {
      board->castling |= 2;
    } else if (*fen == 'q') {
      board->castling |= 1;
    }

    fen++;
  }

  fen++;

  if (*fen != '-') {
    int f = fen[0] - 'a';
    int r = 8 - (fen[1] - '0');

    board->epSquare = r * 8 + f;
  } else {
    board->epSquare = 0;
  }

  setOccupancies(board);
  setSpecialPieces(board);
}

void printBoard(Board* board) {
  for (int i = 0; i < 64; i++) {
    if ((i & 7) == 0)
      printf(" %d ", 8 - (i >> 3));

    if (getBit(board->occupancies[BOTH], i)) {
      for (int p = 0; p < 12; p++) {
        if (getBit(board->pieces[p], i)) {
          printf(" %c", pieceChars[p]);
          break;
        }
      }
    } else {
      printf(" .");
    }

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
}

inline int isSquareAttacked(int sq, int attackColor, BitBoard occupancy, Board* board) {
  if (getPawnAttacks(sq, attackColor ^ 1) & board->pieces[attackColor])
    return 1;
  if (getKnightAttacks(sq) & board->pieces[2 + attackColor])
    return 1;
  if (getBishopAttacks(sq, occupancy) & board->pieces[4 + attackColor])
    return 1;
  if (getRookAttacks(sq, occupancy) & board->pieces[6 + attackColor])
    return 1;
  if (getQueenAttacks(sq, occupancy) & board->pieces[8 + attackColor])
    return 1;
  if (getKingAttacks(sq) & board->pieces[10 + attackColor])
    return 1;

  return 0;
}

inline int inCheck(Board* board) { return board->checkers ? 1 : 0; }

void makeMove(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);
  int piece = movePiece(move);
  int promoted = movePromo(move);
  int capture = moveCapture(move);
  int dub = moveDouble(move);
  int ep = moveEP(move);
  int castle = moveCastle(move);

  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = -1;

  popBit(board->pieces[piece], start);
  setBit(board->pieces[piece], end);

  if (capture && !ep) {
    for (int i = board->xside; i < 12; i += 2) {
      if (getBit(board->pieces[i], end)) {
        board->captureHistory[board->moveNo] = i;
        popBit(board->pieces[i], end);
        break;
      }
    }
  }

  if (promoted) {
    popBit(board->pieces[piece], end);
    setBit(board->pieces[promoted], end);
  }

  if (ep)
    popBit(board->pieces[board->xside], end - pawnDirections[board->side]);

  board->epSquare = 0;
  if (dub)
    board->epSquare = end - pawnDirections[board->side];

  if (castle) {
    // TODO: Remove these "magic" constants
    if (end == 62) {
      popBit(board->pieces[6], 63);
      setBit(board->pieces[6], 61);
    } else if (end == 58) {
      popBit(board->pieces[6], 56);
      setBit(board->pieces[6], 59);
    } else if (end == 6) {
      popBit(board->pieces[7], 7);
      setBit(board->pieces[7], 5);
    } else if (end == 2) {
      popBit(board->pieces[7], 0);
      setBit(board->pieces[7], 3);
    }
  }

  board->castling &= castlingRights[start];
  board->castling &= castlingRights[end];

  setOccupancies(board);

  board->moveNo++;
  board->xside = board->side;
  board->side ^= 1;

  setSpecialPieces(board);
}

void undoMove(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);
  int piece = movePiece(move);
  int promoted = movePromo(move);
  int capture = moveCapture(move);
  int ep = moveEP(move);
  int castle = moveCastle(move);

  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;

  board->epSquare = board->epSquareHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];

  popBit(board->pieces[piece], end);
  setBit(board->pieces[piece], start);

  if (capture)
    setBit(board->pieces[board->captureHistory[board->moveNo]], end);

  if (promoted)
    popBit(board->pieces[promoted], end);

  if (ep)
    setBit(board->pieces[board->xside], end - pawnDirections[board->side]);

  if (castle) {
    if (end == 62) {
      popBit(board->pieces[6], 61);
      setBit(board->pieces[6], 63);
    } else if (end == 58) {
      popBit(board->pieces[6], 59);
      setBit(board->pieces[6], 56);
    } else if (end == 6) {
      popBit(board->pieces[7], 5);
      setBit(board->pieces[7], 7);
    } else if (end == 2) {
      popBit(board->pieces[7], 3);
      setBit(board->pieces[7], 0);
    }
  }

  setOccupancies(board);
  setSpecialPieces(board);
}

int isLegal(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);

  if (moveEP(move)) {
    int kingSq = lsb(board->pieces[KING[board->side]]);
    int captureSq = end - pawnDirections[board->side];
    BitBoard newOccupancy = board->occupancies[BOTH];
    popBit(newOccupancy, start);
    popBit(newOccupancy, captureSq);
    setBit(newOccupancy, end);

    // EP can only be illegal due to crazy discover checks
    return !(getBishopAttacks(kingSq, newOccupancy) &
             (board->pieces[BISHOP[board->xside]] | board->pieces[QUEEN[board->xside]])) &&
           !(getRookAttacks(kingSq, newOccupancy) &
             (board->pieces[ROOK[board->xside]] | board->pieces[QUEEN[board->xside]]));
  } else if (moveCastle(move)) {
    int step = (end > start) ? -1 : 1;

    for (int i = end; i != start; i += step) {
      if (isSquareAttacked(i, board->xside, board->occupancies[BOTH], board))
        return 0;
    }

    return 1;
  } else if (movePiece(move) > 9) {
    BitBoard kingOff = board->occupancies[BOTH];
    popBit(kingOff, start);
    // check king attacks with it off board, because it may move along the checking line
    return !isSquareAttacked(end, board->xside, kingOff, board);
  }

  return 1;
}