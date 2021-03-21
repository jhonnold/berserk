#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

const int CHECKMATE = 32767;
const int MATE_BOUND = 30000;

const int FUTILITY_MARGIN = 85;
const int SEE_PRUNE_CAPTURE_CUTOFF = -70;
const int SEE_PRUNE_CUTOFF = -20;
const int DELTA_CUTOFF = 200;

int LMR[MAX_DEPTH][64];
int LMP[2][MAX_DEPTH];
int SEE[2][MAX_DEPTH];
int FUTILITY[MAX_DEPTH];

void initLMR() {
  for (int depth = 0; depth < MAX_DEPTH; depth++)
    for (int moves = 0; moves < 64; moves++)
      LMR[depth][moves] = (int)(0.6f + log(depth) * log(1.2f * moves) / 2.5f);

  for (int depth = 0; depth < MAX_DEPTH; depth++) {
    LMP[0][depth] = (3 + depth * depth) / 2; // not improving
    LMP[1][depth] = 3 + depth * depth;

    SEE[0][depth] = SEE_PRUNE_CUTOFF * depth * depth; // quiet
    SEE[1][depth] = SEE_PRUNE_CAPTURE_CUTOFF * depth; // capture

    FUTILITY[depth] = FUTILITY_MARGIN * depth;
  }
}

void Search(Board* board, SearchParams* params, SearchData* data) {
  data->nodes = 0;
  data->seldepth = 1;
  memset(data->evals, 0, sizeof(data->evals));

  ttClear();
  memset(board->killers, 0, sizeof(board->killers));
  memset(board->counters, 0, sizeof(board->counters));

  int alpha = -CHECKMATE;
  int beta = CHECKMATE;

  int score = negamax(alpha, beta, 1, 0, 1, board, params, data);

  for (int depth = 2; depth <= params->depth && !params->stopped; depth++) {
    int delta = depth >= 5 ? 10 : CHECKMATE;
    alpha = max(score - delta, -CHECKMATE);
    beta = min(score + delta, CHECKMATE);

    while (!params->stopped) {
      score = negamax(alpha, beta, depth, 0, 1, board, params, data);

      if (score <= alpha) {
        beta = (alpha + beta) / 2;
        alpha = max(alpha - delta, -CHECKMATE);
      } else if (score >= beta) {
        beta = min(beta + delta, CHECKMATE);
      } else {
        break;
      }

      delta += delta / 2;
    }
  }

  printf("bestmove %s\n", moveStr(data->bestMove));
}

void printPv(Move move, Board* board, int maxDepth) {
  int depth = 0;
  Board temp[1];
  memcpy(temp, board, sizeof(Board));

  do {
    printf("%s ", moveStr(move));

    makeMove(move, temp);
    TTValue ttValue = ttProbe(temp->zobrist);
    move = ttMove(ttValue);
  } while (++depth < maxDepth && move);

  printf("\n");
}

int negamax(int alpha, int beta, int depth, int ply, int canNull, Board* board, SearchParams* params,
            SearchData* data) {
  if (depth == 0)
    return quiesce(alpha, beta, ply, board, params, data);

  data->nodes++;
  data->seldepth = max(ply, data->seldepth);

  if (ply) {
    // draw
    if (isRepetition(board) || isMaterialDraw(board) || (board->halfMove > 99))
      return 0;

    if (ply > MAX_DEPTH - 1)
      return Evaluate(board);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + ply);
    beta = min(beta, CHECKMATE - ply - 1);
    if (alpha >= beta)
      return alpha;
  }

  if ((data->nodes & 2047) == 0)
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

  int isPV = beta - alpha != 1;
  int bestScore = -CHECKMATE, a0 = alpha;
  Move bestMove = 0;

  int staticEval = Evaluate(board);
  if (!isPV && !board->checkers) {
    if (ttValue && ttDepth(ttValue) >= depth) {
      int ttEval = ttScore(ttValue, ply);
      if (ttFlag(ttValue) == (ttEval > staticEval ? TT_LOWER : TT_UPPER))
        staticEval = ttEval;
    }

    // Reverse Futility Pruning
    if (depth <= 6 && staticEval - FUTILITY[depth] >= beta && staticEval < MATE_BOUND)
      return staticEval;

    // Null move pruning
    if (depth >= 3 && canNull && staticEval >= beta && hasNonPawn(board)) {
      int R = 3 + depth / 6 + min((staticEval - beta) / 200, 3);

      if (R > depth)
        R = depth;

      nullMove(board);
      int score = -negamax(-beta, -beta + 1, depth - R, ply + 1, 0, board, params, data);
      undoNullMove(board);

      if (params->stopped)
        return 0;

      if (score >= beta)
        return beta;
    }
  }

  data->evals[ply] = staticEval;
  int improving = ply >= 2 && staticEval > data->evals[ply - 2];

  MoveList moveList[1];
  generateMoves(moveList, board, ply);

  int numMoves = 0;
  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    int tactical = movePromo(move) || moveCapture(move);

    int score = alpha + 1;

    if (!isPV && bestScore > -MATE_BOUND) {
      if (depth <= 8 && !tactical && numMoves >= LMP[improving][depth])
        continue;

      if (see(board, move) < SEE[tactical][depth])
        continue;
    }

    numMoves++;
    makeMove(move, board);

    int newDepth = depth;
    if (board->checkers)
      newDepth++; // check extension

    int R = 1;
    if (depth >= 2 && numMoves > 1 && !tactical) {
      R = LMR[min(depth, 63)][min(numMoves, 63)];

      R += !isPV + !improving - !!(moveList->scores[i] >= COUNTER);

      R = min(depth - 1, max(R, 1));
    }

    if (R != 1)
      score = -negamax(-alpha - 1, -alpha, newDepth - R, ply + 1, 1, board, params, data);

    if ((R != 1 && score > alpha) || (R == 1 && (!isPV || numMoves > 1)))
      score = -negamax(-alpha - 1, -alpha, newDepth - 1, ply + 1, 1, board, params, data);

    if (isPV && (numMoves == 1 || (score > alpha && (!ply || score < beta))))
      score = -negamax(-beta, -alpha, newDepth - 1, ply + 1, 1, board, params, data);

    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (!ply) {
        data->bestMove = move;
        data->score = score;
      }

      if (score > alpha)
        alpha = score;

      if (alpha >= beta) {
        if (!tactical) {
          addKiller(board, move, ply);
          addCounter(board, move, board->gameMoves[board->moveNo - 1]);
          addHistoryHeuristic(board, move, depth);
        }

        for (int j = 0; j < i; j++) {
          if (moveCapture(moveList->moves[j]) || movePromo(moveList->moves[j]))
            continue;

          addBFHeuristic(board, moveList->moves[j], depth);
        }

        break;
      }
    }
  }

  // Checkmate detection
  if (!moveList->count)
    return board->checkers ? -CHECKMATE + ply : 0;

  if (!ply && !params->stopped) {
    if (bestScore > MATE_BOUND) {
      int movesToMate = (CHECKMATE - bestScore) / 2 + ((CHECKMATE - bestScore) & 1);

      printf("info depth %d seldepth %d nodes %d time %ld score mate %d pv ", depth, data->seldepth, data->nodes,
             getTimeMs() - params->startTime, movesToMate);
    } else if (bestScore < -MATE_BOUND) {
      int movesToMate = (CHECKMATE + bestScore) / 2 - ((CHECKMATE - bestScore) & 1);

      printf("info depth %d seldepth %d  nodes %d time %ld score mate -%d pv ", depth, data->seldepth, data->nodes,
             getTimeMs() - params->startTime, movesToMate);
    } else {
      printf("info depth %d seldepth %d nodes %d time %ld score cp %d pv ", depth, data->seldepth, data->nodes,
             getTimeMs() - params->startTime, bestScore);
    }
    printPv(bestMove, board, depth);
  }

  int ttFlag = bestScore >= beta ? TT_LOWER : bestScore <= a0 ? TT_UPPER : TT_EXACT;
  ttPut(board->zobrist, depth, bestScore, ttFlag, bestMove, ply);

  return bestScore;
}

int quiesce(int alpha, int beta, int ply, Board* board, SearchParams* params, SearchData* data) {
  data->nodes++;
  data->seldepth = max(ply, data->seldepth);

  if (isMaterialDraw(board) || isRepetition(board) || (board->halfMove > 99))
    return 0;

  if (ply > MAX_DEPTH - 1)
    return Evaluate(board);

  if ((data->nodes & 2047) == 0)
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

  int eval = Evaluate(board);
  if (ttValue) {
    int ttEval = ttScore(ttValue, ply);
    if (ttFlag(ttValue) == (ttEval > eval ? TT_LOWER : TT_UPPER))
      eval = ttEval;
  }

  if (eval >= beta)
    return eval;

  if (eval > alpha)
    alpha = eval;

  int bestScore = eval;

  MoveList moveList[1];
  generateQuiesceMoves(moveList, board);

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    if (movePromo(move)) {
      if (movePromo(move) < QUEEN[WHITE])
        continue;
    } else {
      int captured = moveEP(move) ? PAWN[board->xside] : board->squares[moveEnd(move)];

      assert(captured != NO_PIECE);

      if (eval + DELTA_CUTOFF + scoreMG(MATERIAL_VALUES[captured]) < alpha)
        continue;
    }

    if (moveList->scores[i] < 0)
      break;

    makeMove(move, board);
    int score = -quiesce(-beta, -alpha, ply + 1, board, params, data);
    undoMove(move, board);

    if (params->stopped)
      return 0;

    if (score > bestScore) {
      bestScore = score;
      alpha = max(alpha, score);

      if (alpha >= beta)
        break;
    }
  }

  return bestScore;
}