#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "types.h"

const int charToPieceIdx[] = {['P'] = 0, ['N'] = 2, ['B'] = 4, ['R'] = 6, ['Q'] = 8, ['K'] = 10, ['p'] = 1, ['n'] = 3, ['b'] = 5, ['r'] = 7, ['q'] = 9, ['k'] = 11};

const char *idxToCord[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a6", "b6", "c6", "d6", "e6", "f6",
    "g6", "h6", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a3", "b3", "c3", "d3",
    "e3", "f3", "g3", "h3", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

const char *pieceChars = "PpNnBbRrQqKk";

const int castlingRights[] = {14, 15, 15, 15, 12, 15, 15, 13, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                              15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 11, 15, 15, 15, 3,  15, 15, 7};

void clear() {
  memset(pieces, 0UL, sizeof(pieces));
  memset(occupancies, 0UL, sizeof(occupancies));

  side = 0;
  xside = 1;

  epSquare = 0;
  castling = 0;
}

inline void setOccupancies() {
  memset(occupancies, 0UL, sizeof(occupancies));
  for (int i = 0; i < 12; i++)
    occupancies[i & 1] |= pieces[i];
  occupancies[2] = occupancies[0] | occupancies[1];
}

inline void setSpecialPieces() {
  int kingSq = lsb(pieces[10 + side]);

  pinned = 0ULL;
  discoverers = 0ULL;

  // start looking at knights/pawns
  checkers = (getKnightAttacks(kingSq) & pieces[2 + xside]) | (getPawnAttacks(kingSq, side) & pieces[xside]);
  for (int kingC = 0; kingC <= 1; kingC++) {
    int enemyC = 1 - kingC;
    kingSq = lsb(pieces[10 + kingC]);

    bb_t enemyPiece = ((pieces[4 + enemyC] | pieces[8 + enemyC]) & getBishopAttacks(kingSq, 0)) | ((pieces[6 + enemyC] | pieces[8 + enemyC]) & getRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      bb_t blockers = getInBetween(kingSq, sq) & occupancies[2];

      if (!blockers) {
        checkers |= (enemyPiece & -enemyPiece);
      } else if (bits(blockers) == 1) { // pinned
        pinned |= (blockers & occupancies[kingC]);
        discoverers |= (blockers & occupancies[enemyC]);
      }

      popLsb(enemyPiece);
    }
  }
}

void parseFen(char *fen) {
  clear();

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = charToPieceIdx[*fen];
      setBit(pieces[piece], i);
    } else if (*fen >= '0' && *fen <= '9') {
      int offset = *fen - '1';
      i += offset;
    } else if (*fen == '/') {
      i--;
    }

    fen++;
  }

  fen++;

  side = (*fen++ == 'w' ? 0 : 1);
  xside = side ^ 1;

  fen++;

  while (*fen != ' ') {
    if (*fen == 'K') {
      castling |= 8;
    } else if (*fen == 'Q') {
      castling |= 4;
    } else if (*fen == 'k') {
      castling |= 2;
    } else if (*fen == 'q') {
      castling |= 1;
    }

    fen++;
  }

  fen++;

  if (*fen != '-') {
    int f = fen[0] - 'a';
    int r = 8 - (fen[1] - '0');

    epSquare = r * 8 + f;
  } else {
    epSquare = 0;
  }

  setOccupancies();
  setSpecialPieces();
}

void printBoard() {
  for (int i = 0; i < 64; i++) {
    if ((i & 7) == 0)
      printf(" %d ", 8 - (i >> 3));

    if (getBit(occupancies[2], i)) {
      for (int p = 0; p < 12; p++) {
        if (getBit(pieces[p], i)) {
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

int isSquareAttacked(int sq, int attacker) {
  if (getPawnAttacks(sq, attacker ^ 1) & pieces[attacker])
    return 1;
  if (getKnightAttacks(sq) & pieces[2 + attacker])
    return 1;
  if (getBishopAttacks(sq, occupancies[2]) & pieces[4 + attacker])
    return 1;
  if (getRookAttacks(sq, occupancies[2]) & pieces[6 + attacker])
    return 1;
  if (getQueenAttacks(sq, occupancies[2]) & pieces[8 + attacker])
    return 1;
  if (getKingAttacks(sq) & pieces[10 + attacker])
    return 1;

  return 0;
}

int inCheck() {
  return checkers ? 1 : 0;
}

void makeMove(move_t m) {
  int start = moveStart(m);
  int end = moveEnd(m);
  int piece = movePiece(m);
  int promoted = movePromo(m);
  int capture = moveCapture(m);
  int dub = moveDouble(m);
  int ep = moveEP(m);
  int castle = moveCastle(m);

  castlingHistory[move] = castling;
  epSquareHistory[move] = epSquare;
  captureHistory[move] = -1;

  popBit(pieces[piece], start);
  setBit(pieces[piece], end);

  if (capture && !ep) {
    for (int i = xside; i < 12; i += 2) {
      if (getBit(pieces[i], end)) {
        captureHistory[move] = i;
        popBit(pieces[i], end);
        break;
      }
    }
  }

  if (promoted) {
    popBit(pieces[piece], end);
    setBit(pieces[promoted], end);
  }

  if (ep)
    popBit(pieces[xside], end - pawnDirections[side]);

  epSquare = 0;
  if (dub)
    epSquare = end - pawnDirections[side];

  if (castle) {
    if (end == 62) {
      popBit(pieces[6], 63);
      setBit(pieces[6], 61);
    } else if (end == 58) {
      popBit(pieces[6], 56);
      setBit(pieces[6], 59);
    } else if (end == 6) {
      popBit(pieces[7], 7);
      setBit(pieces[7], 5);
    } else if (end == 2) {
      popBit(pieces[7], 0);
      setBit(pieces[7], 3);
    }
  }

  castling &= castlingRights[start];
  castling &= castlingRights[end];

  setOccupancies();

  move++;
  xside = side;
  side ^= 1;

  setSpecialPieces();
}

void undoMove(move_t m) {
  int start = moveStart(m);
  int end = moveEnd(m);
  int piece = movePiece(m);
  int promoted = movePromo(m);
  int capture = moveCapture(m);
  int ep = moveEP(m);
  int castle = moveCastle(m);

  side = xside;
  xside ^= 1;
  move--;

  epSquare = epSquareHistory[move];
  castling = castlingHistory[move];

  popBit(pieces[piece], end);
  setBit(pieces[piece], start);

  if (capture)
    setBit(pieces[captureHistory[move]], end);

  if (promoted)
    popBit(pieces[promoted], end);

  if (ep)
    setBit(pieces[xside], end - pawnDirections[side]);

  if (castle) {
    if (end == 62) {
      popBit(pieces[6], 61);
      setBit(pieces[6], 63);
    } else if (end == 58) {
      popBit(pieces[6], 59);
      setBit(pieces[6], 56);
    } else if (end == 6) {
      popBit(pieces[7], 5);
      setBit(pieces[7], 7);
    } else if (end == 2) {
      popBit(pieces[7], 3);
      setBit(pieces[7], 0);
    }
  }

  setOccupancies();
  setSpecialPieces();
}

int isLegal(move_t m) {
  int start = moveStart(m);
  int end = moveEnd(m);

  if (moveEP(m)) {
    // TODO: Speed this up
    makeMove(m);
    int valid = !isSquareAttacked(lsb(pieces[10 + xside]), side); 
    undoMove(m);

    return valid;
  } else if (moveCastle(m)) {
    int step = (end > start) ? 1 : -1;

    for (int i = start + step; i != end; i += step)
      if (isSquareAttacked(i, xside))
        return 0;
    
    return 1;
  } else if (movePiece(m) > 9) {
    popBit(occupancies[2], start); // take king off the board and see if that square is attacked
    int valid = !isSquareAttacked(end, xside);
    setBit(occupancies[2], start);
    return valid;
  }

  return 1;
}