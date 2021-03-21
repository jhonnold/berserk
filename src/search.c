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

void Search(SearchParams* params, SearchData* data) {
  data->nodes = 0;
  data->seldepth = 0;
  data->ply = 0;
  memset(data->evals, 0, sizeof(data->evals));
  memset(data->moves, 0, sizeof(data->moves));
  memset(data->killers, 0, sizeof(data->killers));
  memset(data->counters, 0, sizeof(data->counters));
  memset(data->hh, 0, sizeof(data->hh));
  memset(data->bf, 0, sizeof(data->bf));

  ttClear();

  PV pv[1];

  int alpha = -CHECKMATE;
  int beta = CHECKMATE;

  int score = negamax(alpha, beta, 1, params, data, pv);
  printInfo(pv, score, 1, params, data);

  for (int depth = 2; depth <= params->depth && !params->stopped; depth++) {
    int delta = depth >= 5 ? 10 : CHECKMATE;
    alpha = max(score - delta, -CHECKMATE);
    beta = min(score + delta, CHECKMATE);

    while (!params->stopped) {
      score = negamax(alpha, beta, depth, params, data, pv);

      if (score <= alpha) {
        beta = (alpha + beta) / 2;
        alpha = max(alpha - delta, -CHECKMATE);
      } else if (score >= beta) {
        beta = min(beta + delta, CHECKMATE);
      } else {
        printInfo(pv, score, depth, params, data);
        break;
      }

      delta += delta / 2;
    }
  }

  data->bestMove = ttMove(ttProbe(data->board->zobrist));
  data->score = score;

  printf("bestmove %s\n", moveStr(data->bestMove));
}

int negamax(int alpha, int beta, int depth, SearchParams* params, SearchData* data, PV* pv) {
  PV childPv[1];
  pv->count = 0;

  int isPV = beta - alpha != 1;
  int isRoot = !data->ply;
  int bestScore = -CHECKMATE, a0 = alpha;
  Move bestMove = 0;

  if (depth == 0)
    return quiesce(alpha, beta, params, data);

  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  if (!isRoot) {
    // draw
    if (isRepetition(data->board) || isMaterialDraw(data->board) || (data->board->halfMove > 99))
      return 0;

    if (data->ply > MAX_DEPTH - 1)
      return Evaluate(data->board);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + data->ply);
    beta = min(beta, CHECKMATE - data->ply - 1);
    if (alpha >= beta)
      return alpha;
  }

  if ((data->nodes & 2047) == 0)
    communicate(params);

  TTValue ttValue = ttProbe(data->board->zobrist);
  if (ttValue) {
    if (ttDepth(ttValue) >= depth) {
      int score = ttScore(ttValue, data->ply);
      int flag = ttFlag(ttValue);

      if (flag == TT_EXACT)
        return score;
      if (flag == TT_LOWER && score >= beta)
        return score;
      if (flag == TT_UPPER && score <= alpha)
        return score;
    }
  }

  int eval = data->evals[data->ply] = (ttValue ? ttEval(ttValue) : Evaluate(data->board));
  int improving = data->ply >= 2 && (data->evals[data->ply] > data->evals[data->ply - 2]);

  assert(eval == Evaluate(data->board));

  data->killers[data->ply + 1][0] = NULL_MOVE;
  data->killers[data->ply + 1][1] = NULL_MOVE;

  if (!isPV && !data->board->checkers) {
    if (ttValue && ttDepth(ttValue) >= depth) {
      int ttEvalFromScore = ttScore(ttValue, data->ply);
      if (ttFlag(ttValue) == (ttEvalFromScore > eval ? TT_LOWER : TT_UPPER))
        eval = ttEvalFromScore;
    }

    // Reverse Futility Pruning
    if (depth <= 6 && eval - FUTILITY[depth] >= beta && eval < MATE_BOUND)
      return eval;

    // Null move pruning
    if (depth >= 3 && data->moves[data->ply - 1] != NULL_MOVE && eval >= beta && hasNonPawn(data->board)) {
      int R = 3 + depth / 6 + min((eval - beta) / 200, 3);

      if (R > depth)
        R = depth;

      data->moves[data->ply++] = NULL_MOVE;
      nullMove(data->board);

      int score = -negamax(-beta, -beta + 1, depth - R, params, data, childPv);

      undoNullMove(data->board);
      data->ply--;

      if (params->stopped)
        return 0;

      if (score >= beta)
        return beta;
    }
  }

  MoveList moveList[1];
  generateMoves(moveList, data);

  int numMoves = 0;
  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    int tactical = movePromo(move) || moveCapture(move);

    int score = alpha + 1;

    if (!isPV && bestScore > -MATE_BOUND) {
      if (depth <= 8 && !tactical && numMoves >= LMP[improving][depth])
        continue;

      if (see(data->board, move) < SEE[tactical][depth])
        continue;
    }

    numMoves++; // TODO: should this be in an array on SearchData?
    data->moves[data->ply++] = move;
    makeMove(move, data->board);

    int newDepth = depth;
    if (data->board->checkers)
      newDepth++; // check extension

    int R = 1;
    if (depth >= 2 && numMoves > 1 && !tactical) {
      R = LMR[min(depth, 63)][min(numMoves, 63)];

      R += !isPV + !improving - !!(moveList->scores[i] >= COUNTER);

      R = min(depth - 1, max(R, 1));
    }

    if (R != 1)
      score = -negamax(-alpha - 1, -alpha, newDepth - R, params, data, childPv);

    if ((R != 1 && score > alpha) || (R == 1 && (!isPV || numMoves > 1)))
      score = -negamax(-alpha - 1, -alpha, newDepth - 1, params, data, childPv);

    if (isPV && (numMoves == 1 || (score > alpha && (isRoot || score < beta))))
      score = -negamax(-beta, -alpha, newDepth - 1, params, data, childPv);

    undoMove(move, data->board);
    data->ply--;

    if (params->stopped)
      return 0;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha) {
        alpha = score;

        pv->count = childPv->count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv->moves, childPv->count * sizeof(Move));
      }

      if (alpha >= beta) {
        if (!tactical) {
          addKiller(data, move);
          addCounter(data, move, data->moves[data->ply - 1]);
          addHistoryHeuristic(data, move, depth);
        }

        for (int j = 0; j < i; j++) {
          if (moveCapture(moveList->moves[j]) || movePromo(moveList->moves[j]))
            continue;

          addBFHeuristic(data, moveList->moves[j], depth);
        }

        break;
      }
    }
  }

  // Checkmate detection
  if (!moveList->count)
    return data->board->checkers ? -CHECKMATE + data->ply : 0;

  int ttFlag = bestScore >= beta ? TT_LOWER : bestScore <= a0 ? TT_UPPER : TT_EXACT;
  ttPut(data->board->zobrist, depth, bestScore, ttFlag, bestMove, data->ply, data->evals[data->ply]);

  assert(bestScore >= -CHECKMATE);
  assert(bestScore <= CHECKMATE);

  return bestScore;
}

int quiesce(int alpha, int beta, SearchParams* params, SearchData* data) {
  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  if (isMaterialDraw(data->board) || isRepetition(data->board) || (data->board->halfMove > 99))
    return 0;

  if (data->ply > MAX_DEPTH - 1)
    return Evaluate(data->board);

  if ((data->nodes & 2047) == 0)
    communicate(params);

  TTValue ttValue = ttProbe(data->board->zobrist);
  if (ttValue) {
    int score = ttScore(ttValue, data->ply);
    int flag = ttFlag(ttValue);

    if (flag == TT_EXACT)
      return score;
    if (flag == TT_LOWER && score >= beta)
      return score;
    if (flag == TT_UPPER && score <= alpha)
      return score;
  }

  int eval = data->evals[data->ply] = (ttValue ? ttEval(ttValue) : Evaluate(data->board));
  if (ttValue) {
    int ttEval = ttScore(ttValue, data->ply);
    if (ttFlag(ttValue) == (ttEval > eval ? TT_LOWER : TT_UPPER))
      eval = ttEval;
  }

  if (eval >= beta)
    return eval;

  if (eval > alpha)
    alpha = eval;

  int bestScore = eval;

  MoveList moveList[1];
  generateQuiesceMoves(moveList, data);

  for (int i = 0; i < moveList->count; i++) {
    bubbleTopMove(moveList, i);
    Move move = moveList->moves[i];

    if (movePromo(move)) {
      if (movePromo(move) < QUEEN[WHITE])
        continue;
    } else {
      int captured = moveEP(move) ? PAWN[data->board->xside] : data->board->squares[moveEnd(move)];

      assert(captured != NO_PIECE);

      if (eval + DELTA_CUTOFF + scoreMG(MATERIAL_VALUES[captured]) < alpha)
        continue;
    }

    if (moveList->scores[i] < 0)
      break;

    data->moves[data->ply++] = move;
    makeMove(move, data->board);

    int score = -quiesce(-beta, -alpha, params, data);

    undoMove(move, data->board);
    data->ply--;

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

inline void printInfo(PV* pv, int score, int depth, SearchParams* params, SearchData* data) {
  if (score > MATE_BOUND) {
    int movesToMate = (CHECKMATE - score) / 2 + ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d nodes %d time %ld score mate %d pv ", depth, data->seldepth, data->nodes,
           getTimeMs() - params->startTime, movesToMate);
  } else if (score < -MATE_BOUND) {
    int movesToMate = (CHECKMATE + score) / 2 - ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d  nodes %d time %ld score mate -%d pv ", depth, data->seldepth, data->nodes,
           getTimeMs() - params->startTime, movesToMate);
  } else {
    printf("info depth %d seldepth %d nodes %d time %ld score cp %d pv ", depth, data->seldepth, data->nodes,
           getTimeMs() - params->startTime, score);
  }
  printPv(pv);
}

void printPv(PV* pv) {
  for (int i = 0; i < pv->count; i++)
    printf("%s ", moveStr(pv->moves[i]));
  printf("\n");
}