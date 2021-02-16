#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "types.h"

const int CHECKMATE = 32767;
const int MATE_BOUND = 30000;

int Search(Board* board, Move* bestMove) {
  int best = -CHECKMATE;

  MoveList moveList[1];
  generateMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    makeMove(move, board);
    int score = -negamax(-CHECKMATE, -best, 3, 1, board);

    undoMove(move, board);

    if (score > best) {
      best = score;
      *bestMove = move;

      if (score > MATE_BOUND) {
        printf("info depth 3 score mate %d pv %s\n", CHECKMATE - score, moveStr(*bestMove));
      } else if (score < -MATE_BOUND) {
        printf("info depth 3 score mate -%d pv %s\n", score + CHECKMATE, moveStr(*bestMove));
      } else {
        printf("info depth 3 score cp %d pv %s\n", score, moveStr(*bestMove));
      }
    }
  }

  return best;
}

int negamax(int alpha, int beta, int depth, int ply, Board* board) {
  if (depth == 0)
    return quiesce(alpha, beta, board);

  MoveList moveList[1];
  generateMoves(moveList, board);

  if (!moveList->count)
    return inCheck(board) ? -CHECKMATE + ply : 0;

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    makeMove(move, board);
    int score = -negamax(-beta, -alpha, depth - 1, ply + 1, board);
    undoMove(move, board);

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}

int quiesce(int alpha, int beta, Board* board) {
  int eval = Evaluate(board);

  if (eval >= beta)
    return beta;

  if (eval > alpha)
    alpha = eval;

  MoveList moveList[1];
  generateMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];
    if (!moveCapture(move))
      continue;

    makeMove(move, board);
    int score = -quiesce(-beta, -alpha, board);
    undoMove(move, board);

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}