#include <stdio.h>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

const int CHECKMATE = 32767;
const int MATE_BOUND = 30000;

void Search(Board* board, SearchParams* params) {
  params->nodes = 0;
  ttClear();

  int alpha = -CHECKMATE;
  int beta = CHECKMATE;

  int score = negamax(alpha, beta, 1, 0, 1, board, params);

  for (int depth = 2; depth <= params->depth && !params->stopped; depth++) {
    int delta = depth >= 4 ? 50 : CHECKMATE;
    alpha = max(score - delta, -CHECKMATE);
    beta = min(score + delta, CHECKMATE);

    while (!params->stopped) {
      score = negamax(alpha, beta, depth, 0, 1, board, params);

      if (score <= alpha && score > -MATE_BOUND) {
        alpha = max(alpha - delta, -CHECKMATE);
        delta *= 2;
      } else if (score >= beta && score < MATE_BOUND) {
        beta = min(beta + delta, CHECKMATE);
        delta *= 2;
      } else {
        break;
      }
    }
  }

  printf("bestmove %s\n", moveStr(ttMove(ttProbe(board->zobrist))));
}

int negamax(int alpha, int beta, int depth, int ply, int canNull, Board* board, SearchParams* params) {
  if (ply && isRepetition(board))
    return 0;

  if (depth == 0)
    return quiesce(alpha, beta, board, params);

  if ((params->nodes & 2047) == 0)
    communicate(params);

  TTValue ttValue = ttProbe(board->zobrist);
  if (ttValue) {
    if (ttDepth(ttValue) >= depth) {
      int score = ttScore(ttValue, ply);
      int flag = ttFlag(ttValue);

      if (flag == TT_EXACT)
        return score;
      if (flag == TT_LOWER && score >= beta)
        return score;
      if (flag == TT_UPPER && score <= alpha)
        return score;
    }
  }

  params->nodes++;

  if (depth > 3 && canNull && !inCheck(board)) {
    nullMove(board);
    int score = -negamax(-beta, -beta + 1, depth - 3, ply + 1, 0, board, params);
    undoNullMove(board);

    if (params->stopped)
      return 0;

    if (score >= beta)
      return beta;
  }

  int bestScore = -CHECKMATE, a0 = alpha;
  Move bestMove = 0;

  MoveList moveList[1];
  generateMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    makeMove(move, board);
    int score = -negamax(-beta, -alpha, depth - 1, ply + 1, 1, board, params);
    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha)
        alpha = score;

      if (alpha >= beta)
        break;
    }
  }

  // Checkmate detection
  if (!moveList->count)
    return inCheck(board) ? -CHECKMATE + ply : 0;

  if (!ply && !params->stopped) {
    if (bestScore > MATE_BOUND) {
      int movesToMate = (CHECKMATE - bestScore) / 2 + ((CHECKMATE - bestScore) & 1);

      printf("info depth %d nodes %ld time %ld score mate %d pv %s\n", depth, params->nodes,
             getTimeMs() - params->startTime, movesToMate, moveStr(bestMove));
    } else if (bestScore < -MATE_BOUND) {
      int movesToMate = (CHECKMATE + bestScore) / 2 - ((CHECKMATE - bestScore) & 1);

      printf("info depth %d nodes %ld time %ld score mate -%d pv %s\n", depth, params->nodes,
             getTimeMs() - params->startTime, movesToMate, moveStr(bestMove));
    } else {
      printf("info depth %d nodes %ld time %ld score cp %d pv %s\n", depth, params->nodes,
             getTimeMs() - params->startTime, bestScore, moveStr(bestMove));
    }
  }

  int ttFlag = bestScore >= beta ? TT_LOWER : bestScore <= a0 ? TT_UPPER : TT_EXACT;
  ttPut(board->zobrist, depth, bestScore, ttFlag, bestMove);

  return bestScore;
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
  generateQuiesceMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    if (movePromo(move) && movePromo(move) < QUEEN[WHITE])
      continue;

    int captured = capturedPiece(move, board);
    if (eval + 200 + scoreMG(materialValues[captured]) < alpha)
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