#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "types.h"
#include "util.h"

const int CHECKMATE = 32767;
const int MATE_BOUND = 30000;

void Search(Board* board, SearchParams* params) {
  params->nodes = 0;

  Move bestMove = 0;
  int alpha = -CHECKMATE;

  MoveList moveList[1];
  generateMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];
    params->nodes++;

    makeMove(move, board);
    int score = -negamax(-CHECKMATE, -alpha, 4, 1, board, params);

    undoMove(move, board);

    if (score > alpha) {
      alpha = score;
      bestMove = move;

      if (score > MATE_BOUND) {
        printf("info depth 4 score mate %d pv %s\n", CHECKMATE - score, moveStr(bestMove));
      } else if (score < -MATE_BOUND) {
        printf("info depth 4 score mate -%d pv %s\n", score + CHECKMATE, moveStr(bestMove));
      } else {
        printf("info depth 4 score cp %d pv %s\n", score, moveStr(bestMove));
      }
    }
  }

  if (!bestMove)
    printf("bestmove %s\n", moveStr(moveList->moves[0]));
  else
    printf("bestmove %s\n", moveStr(bestMove));
}

int negamax(int alpha, int beta, int depth, int ply, Board* board, SearchParams* params) {
  if (depth == 0)
    return quiesce(alpha, beta, board, params);

  if ((params->nodes & 2047) == 0)
    communicate(params);

  params->nodes++;

  MoveList moveList[1];
  generateMoves(moveList, board);

  if (!moveList->count)
    return inCheck(board) ? -CHECKMATE + ply : 0;

  for (int i = 0; i < moveList->count; i++) {
    Move move = moveList->moves[i];

    makeMove(move, board);
    int score = -negamax(-beta, -alpha, depth - 1, ply + 1, board, params);
    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}

int quiesce(int alpha, int beta, Board* board, SearchParams* params) {
  if ((params->nodes & 2047) == 0)
    communicate(params);

  params->nodes++;

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
    int score = -quiesce(-beta, -alpha, board, params);
    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}