#include <stdio.h>
#include <string.h>

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

const int FUTILITY_MARGINS[] = {0, 90, 180, 280, 380, 490, 600};

void Search(Board* board, SearchParams* params) {
  params->nodes = 0;
  ttClear();
  memset(board->killers, 0, sizeof(board->history));
  memset(board->history, 0, sizeof(board->history));
  memset(board->bf, 0, sizeof(board->history));
  memset(board->counters, 0, sizeof(board->counters));

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
  // Repetition detection
  if (ply && isRepetition(board))
    return 0;

  // Mate distance pruning
  alpha = max(alpha, -CHECKMATE + ply);
  beta = min(beta, CHECKMATE - ply - 1);
  if (alpha >= beta)
    return alpha;

  // Check extension
  if (inCheck(board))
    depth++;
  if (depth == 0)
    return quiesce(alpha, beta, ply, board, params);

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

  int isPV = beta - alpha != 1 ? 1 : 0;
  int bestScore = -CHECKMATE, a0 = alpha;
  Move bestMove = 0;

  int staticEval = Evaluate(board);
  if (!isPV && !inCheck(board)) {
    // Reverse Futility Pruning
    if (depth <= 6 && staticEval - FUTILITY_MARGINS[depth] >= beta && staticEval < MATE_BOUND)
      return staticEval;

    // Null move pruning
    if (depth > 3 && canNull) {
      int R = 3;
      if (depth >= 6)
        R++;

      nullMove(board);
      int score = -negamax(-beta, -beta + 1, depth - R, ply + 1, 0, board, params);
      undoNullMove(board);

      if (params->stopped)
        return 0;

      if (score >= beta)
        return beta;
    }
  }

  MoveList moveList[1];
  generateMoves(moveList, board, ply);

  int numMoves = 0;
  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    numMoves++;
    makeMove(move, board);

    // LMR
    int score = alpha + 1;
    int newDepth = depth - 1;

    // Start LMR
    int doZws = (!isPV || numMoves > 1) ? 1 : 0;

    if (depth >= 3 && numMoves > (!ply ? 3 : 1) && !moveCapture(move) && !movePromo(move)) {
      // Senpai logic
      if (numMoves <= 6)
        newDepth--;
      else
        newDepth -= depth / 2;

      score = -negamax(-alpha - 1, -alpha, newDepth, ply + 1, 1, board, params);

      doZws = (score > alpha) ? 1 : 0;
    }

    if (doZws)
      score = -negamax(-alpha - 1, -alpha, depth - 1, ply + 1, 1, board, params);

    if (isPV && (numMoves == 1 || (score > alpha && (!ply || score < beta))))
      score = -negamax(-beta, -alpha, depth - 1, ply + 1, 1, board, params);

    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha)
        alpha = score;

      if (alpha >= beta) {
        if (!moveCapture(move) && !movePromo(move)) {
          addKiller(board, move, ply);
          addHistory(board, move, depth);
          addCounter(board, move, board->gameMoves[board->moveNo - 1]);
        }

        break;
      }

      if (!moveCapture(move) && !movePromo(move))
        addBf(board, move, depth);
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

int quiesce(int alpha, int beta, int ply, Board* board, SearchParams* params) {
  if ((params->nodes & 2047) == 0)
    communicate(params);

  TTValue ttValue = ttProbe(board->zobrist);
  if (ttValue) {
    int score = ttScore(ttValue, ply);
    int flag = ttFlag(ttValue);

    if (flag == TT_EXACT)
      return score;
    if (flag == TT_LOWER && score >= beta)
      return score;
    if (flag == TT_UPPER && score <= alpha)
      return score;
  }

  params->nodes++;

  int eval = Evaluate(board);

  if (eval >= beta)
    return beta;

  if (eval > alpha)
    alpha = eval;

  MoveList moveList[1];
  generateQuiesceMoves(moveList, board, ply);

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    if (movePromo(move) && movePromo(move) < QUEEN[WHITE])
      continue;

    int captured = capturedPiece(move, board);
    if (eval + 200 + scoreMG(materialValues[captured]) < alpha)
      continue;

    makeMove(move, board);
    int score = -quiesce(-beta, -alpha, ply + 1, board, params);
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