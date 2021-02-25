#include "see.h"
#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

inline int see(Board* board, Move move) {
  BitBoard occupied = board->occupancies[2];
  int side = board->side;

  int gain[32];
  int captureCount = 1;

  int start = moveStart(move);
  int end = moveEnd(move);

  BitBoard attackers = attacksTo(board, end);
  int captured = capturedPiece(move, board);
  // We can run see against a non-capture
  int attackedPieceVal = captured >= 0 ? scoreMG(materialValues[captured]) : 0;

  side ^= 1;
  gain[0] = attackedPieceVal;

  int piece = movePiece(move);
  attackedPieceVal = scoreMG(materialValues[piece]);
  popBit(occupied, start);

  // If pawn/bishop/queen captures
  if (piece == 0 || piece == 1 || piece == 4 || piece == 5 || piece == 8 || piece == 9)
    attackers |= getBishopAttacks(end, occupied) & (board->pieces[BISHOP[WHITE]] | board->pieces[BISHOP[BLACK]] |
                                                    board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

  // If pawn/rook/queen captures (ep)
  if (piece >= 6 && piece <= 9)
    attackers |= getRookAttacks(end, occupied) & (board->pieces[ROOK[WHITE]] | board->pieces[ROOK[BLACK]] |
                                                  board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

  BitBoard attackee = 0;
  attackers &= occupied;

  while (attackers) {
    for (piece = side; piece <= KING[side]; piece += 2)
      if ((attackee = board->pieces[piece] & attackers))
        break;

    if (piece >= 12)
      break;

    occupied ^= (attackee & -attackee);

    // If pawn/bishop/queen captures
    if (piece == 0 || piece == 1 || piece == 4 || piece == 5 || piece == 8 || piece == 9)
      attackers |= getBishopAttacks(end, occupied) & (board->pieces[BISHOP[WHITE]] | board->pieces[BISHOP[BLACK]] |
                                                      board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

    // If pawn/rook/queen captures (ep)
    if (piece >= 6 && piece <= 9)
      attackers |= getRookAttacks(end, occupied) & (board->pieces[ROOK[WHITE]] | board->pieces[ROOK[BLACK]] |
                                                    board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

    gain[captureCount] = -gain[captureCount - 1] + attackedPieceVal;
    attackedPieceVal = scoreMG(materialValues[piece]);
    if (gain[captureCount++] - attackedPieceVal > 0)
      break;

    side ^= 1;
    attackers &= occupied;
  }

  while (--captureCount)
    gain[captureCount - 1] = -max(-gain[captureCount - 1], gain[captureCount]);

  return gain[0];
}
