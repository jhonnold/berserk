#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "pyrrhic/tbprobe.h"
#include "tb.h"


#define vf(bb) __builtin_bswap64((bb))

Move TBRootProbe(Board* board) {
  if (board->castling || bits(board->occupancies[BOTH]) > TB_LARGEST)
    return NULL_MOVE;

  unsigned results[MAX_MOVES]; // used for native support
  unsigned res = tb_probe_root(vf(board->occupancies[WHITE]), vf(board->occupancies[BLACK]),
                               vf(board->pieces[KING_WHITE] | board->pieces[KING_BLACK]),
                               vf(board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK]),
                               vf(board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]),
                               vf(board->pieces[BISHOP_WHITE] | board->pieces[BISHOP_BLACK]),
                               vf(board->pieces[KNIGHT_WHITE] | board->pieces[KNIGHT_BLACK]),
                               vf(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]), board->halfMove,
                               board->epSquare ? MIRROR[board->epSquare] : 0, board->side == WHITE ? 1 : 0, results);

  if (res == TB_RESULT_FAILED || res == TB_RESULT_STALEMATE || res == TB_RESULT_CHECKMATE)
    return NULL_MOVE;

  unsigned start = MIRROR[TB_GET_FROM(res)];
  unsigned end = MIRROR[TB_GET_TO(res)];
  unsigned ep = TB_GET_EP(res);
  unsigned promo = TB_GET_PROMOTES(res);
  int piece = board->squares[start];
  int capture = !!board->squares[end];

  int promoPieces[5] = {0, QUEEN[board->side], ROOK[board->side], BISHOP[board->side], KNIGHT[board->side]};

  return BuildMove(start, end, piece, promoPieces[promo], capture,
                   PIECE_TYPE[piece] == PAWN_TYPE && abs((int)start - (int)end) == 2, ep, 0);
}

unsigned TBProbe(Board* board) {
  if (board->castling || board->halfMove || bits(board->occupancies[BOTH]) > TB_LARGEST)
    return TB_RESULT_FAILED;

  return tb_probe_wdl(vf(board->occupancies[WHITE]), vf(board->occupancies[BLACK]),
                      vf(board->pieces[KING_WHITE] | board->pieces[KING_BLACK]),
                      vf(board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK]),
                      vf(board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]),
                      vf(board->pieces[BISHOP_WHITE] | board->pieces[BISHOP_BLACK]),
                      vf(board->pieces[KNIGHT_WHITE] | board->pieces[KNIGHT_BLACK]),
                      vf(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]),
                      board->epSquare ? MIRROR[board->epSquare] : 0, board->side == WHITE ? 1 : 0);
}