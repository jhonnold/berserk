// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "pyrrhic/tbprobe.h"
#include "search.h"
#include "see.h"
#include "tb.h"
#include "thread.h"
#include "timeman.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

// arrays to store these pruning cutoffs at specific depths
int LMR[MAX_SEARCH_PLY][64];
int LMP[2][MAX_SEARCH_PLY];
int STATIC_PRUNE[2][MAX_SEARCH_PLY];
int RFP[MAX_SEARCH_PLY];

void InitPruningAndReductionTables() {
  for (int depth = 1; depth < MAX_SEARCH_PLY; depth++)
    for (int moves = 1; moves < 64; moves++)
      // Credit to Ethereal for this LMR
      LMR[depth][moves] = (int)(0.8f + log(depth) * log(1.2f * moves) / 2.5f);

  LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;

  for (int depth = 0; depth < MAX_SEARCH_PLY; depth++) {
    // LMP has both a improving (more strict) and non-improving evalution parameter
    // for lmp. If the evaluation is getting better we want to check more
    LMP[0][depth] = (3 + depth * depth) / 2;
    LMP[1][depth] = 3 + depth * depth;

    STATIC_PRUNE[0][depth] = -SEE_PRUNE_CUTOFF * depth * depth; // quiet move cutoff
    STATIC_PRUNE[1][depth] = -SEE_PRUNE_CAPTURE_CUTOFF * depth; // capture cutoff

    RFP[depth] = RFP_STEP_RISE * depth * depth / 2 - RFP_STEP_RISE * depth / 2 + RFP_BASE * depth;
  }
}

void* UCISearch(void* arg) {
  SearchArgs* args = (SearchArgs*)arg;

  Board* board = args->board;
  SearchParams* params = args->params;
  ThreadData* threads = args->threads;

  BestMove(board, params, threads);

  free(args);
  return NULL;
}

int BestMove(Board* board, SearchParams* params, ThreadData* threads) {
  Move bestMove;
  if ((bestMove = TBRootProbe(board))) {
    printf("bestmove %s\n", MoveToStr(bestMove));
    return 0;
  }

  pthread_t pthreads[threads->count];
  InitPool(board, params, threads);

  params->stopped = 0;

  // start at 1, we will resuse main-thread
  for (int i = 1; i < threads->count; i++)
    pthread_create(&pthreads[i], NULL, &Search, &threads[i]);
  Search(&threads[0]);

  // if main thread stopped, then stop all and wait till complete
  params->stopped = 1;
  for (int i = 1; i < threads->count; i++)
    pthread_join(pthreads[i], NULL);

  // we accept the best move from the mainthread
  bestMove = threads[0].data.bestMove;
  int bestScore = threads[0].data.score;

  printf("bestmove %s\n", MoveToStr(bestMove));
  return bestScore;
}

void* Search(void* arg) {
  ThreadData* thread = (ThreadData*)arg;
  SearchParams* params = thread->params;
  SearchData* data = &thread->data;
  PV* pv = &thread->pv;
  int mainThread = !thread->idx;

  int alpha = -CHECKMATE;
  int beta = CHECKMATE;
  int score = 0;
  int expands = 0;

  // set a hot exit point for this thread
  if (!setjmp(thread->exit)) {

    // Iterative deepening
    for (int depth = 1; depth <= params->depth; depth++) {
      // delta is our window for search. early depths get full searches
      // as we don't know what score to expect. Otherwise we start with a window of 16 (8x2), but
      // vary this slightly based on the previous depths window expansion count
      int delta = depth >= 5 && abs(score) <= 1000 ? WINDOW : CHECKMATE;

      expands = 0;
      alpha = max(score - delta, -CHECKMATE);
      beta = min(score + delta, CHECKMATE);

      while (!params->stopped) {
        // search!
        score = Negamax(alpha, beta, depth, thread, pv);

        if (mainThread && ((GetTimeMS() - 2500 >= params->startTime) || (score > alpha && score < beta)))
          PrintInfo(pv, score, depth, thread);

        if (score <= alpha) {
          // adjust beta downward when failing low
          beta = (alpha + beta) / 2;
          alpha = max(alpha - delta, -CHECKMATE);
        } else if (score >= beta) {
          beta = min(beta + delta, CHECKMATE);
        } else
          break;

        // delta x 1.5
        delta += delta / 2;
        expands++;
      }

      if (mainThread)
        UpdateTimeParams(params, data->score, score, expands, depth);

      data->bestMove = pv->moves[0];
      data->score = score;
    }
  }

  return NULL;
}

int Negamax(int alpha, int beta, int depth, ThreadData* thread, PV* pv) {
  SearchParams* params = thread->params;
  SearchData* data = &thread->data;
  Board* board = &thread->board;

  PV childPv;
  pv->count = 0;

  int isPV = beta - alpha != 1; // pv node when doing a full window
  int isRoot = !data->ply;      //
  int score = -CHECKMATE;       // initially assume the worst case
  int bestScore = -CHECKMATE;   //
  int maxScore = CHECKMATE;     // best possible
  int origAlpha = alpha;        // remember first alpha for tt storage

  Move bestMove = NULL_MOVE;
  Move skipMove = data->skipMove[data->ply]; // skip used in SE (concept from SF)
  Move nullThreat = NULL_MOVE;
  Move hashMove = NULL_MOVE;

  Move move;
  MoveList moves;

  // drop into tactical moves only
  if (depth <= 0)
    return Quiesce(alpha, beta, thread, pv);

  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  // Either mainthread has ended us OR we've run out of time
  // this second check is more expensive and done only every 1024 nodes
  // 1Mnps ~1ms
  if (params->stopped || (!(data->nodes & 1023) && params->timeset && GetTimeMS() > params->endTime))
    longjmp(thread->exit, 1);

  if (!isRoot) {
    // draw
    if (IsRepetition(board, data->ply) || IsMaterialDraw(board) || (board->halfMove > 99))
      return 2 - (data->nodes & 0x3);

    // Prevent overflows
    if (data->ply > MAX_SEARCH_PLY - 1)
      return Evaluate(board, thread);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + data->ply);
    beta = min(beta, CHECKMATE - data->ply - 1);
    if (alpha >= beta)
      return alpha;
  }

  // check the transposition table for previous info
  // we ignore the tt on singular extension searches
  int ttHit = 0;
  TTEntry* tt = skipMove ? NULL : TTProbe(&ttHit, board->zobrist);
  if (ttHit)
    hashMove = tt->move;

  // if the TT has a value that fits our position and has been searched to an equal or greater depth, then we accept
  // this score and prune
  if (!isPV && ttHit && tt->depth >= depth) {
    int ttScore = TTScore(tt, data->ply);

    if ((tt->flags & TT_EXACT) || ((tt->flags & TT_LOWER) && ttScore >= beta) ||
        ((tt->flags & TT_UPPER) && ttScore <= alpha))
      return ttScore;
  }

  // tablebase - we do not do this at root
  if (!isRoot) {
    unsigned tbResult = TBProbe(board);

    if (tbResult != TB_RESULT_FAILED) {
      data->tbhits++;

      int flag;
      switch (tbResult) {
      case TB_WIN:
        score = TB_WIN_BOUND - data->ply;
        flag = TT_LOWER;
        break;
      case TB_LOSS:
        score = -TB_WIN_BOUND + data->ply;
        flag = TT_UPPER;
        break;
      default:
        score = 0;
        flag = TT_EXACT;
        break;
      }

      // if the tablebase gives us what we want, then we accept it's score and return
      if ((flag & TT_EXACT) || ((flag & TT_LOWER) && score >= beta) || ((flag & TT_UPPER) && score <= alpha)) {
        TTPut(board->zobrist, depth, score, flag, 0, data->ply, 0);
        return score;
      }

      // for pv node searches we adjust our a/b search accordingly
      if (isPV) {
        if (flag & TT_LOWER) {
          bestScore = score;
          alpha = max(alpha, score);
        } else
          maxScore = score;
      }
    }
  }

  // IIR by Ed Schroder
  // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
  if (depth >= 4 && !ttHit && !skipMove)
    depth--;

  // pull previous static eval from tt - this is depth independent
  int eval = data->evals[data->ply] = (ttHit ? tt->eval : Evaluate(board, thread));
  // getting better if eval has gone up
  int improving = data->ply >= 2 && (data->evals[data->ply] > data->evals[data->ply - 2]);

  // reset moves to moves related to 1 additional ply
  data->skipMove[data->ply + 1] = NULL_MOVE;
  data->killers[data->ply + 1][0] = NULL_MOVE;
  data->killers[data->ply + 1][1] = NULL_MOVE;

  if (!isPV && !board->checkers) {
    // Our TT might have a more accurate evaluation score, use this
    if (ttHit && tt->depth >= depth) {
      int ttScore = TTScore(tt, data->ply);
      if (tt->flags & (ttScore > eval ? TT_LOWER : TT_UPPER))
        eval = ttScore;
    }

    // Reverse Futility Pruning
    // i.e. the static eval is so far above beta we prune
    if (depth <= 6 && eval - RFP[depth] >= beta && eval < MATE_BOUND)
      return eval;

    // Null move pruning
    // i.e. Our position is so good we can give our opponnent a free move and
    // they still can't catch up (this is usually countered by captures or mate threats)
    if (depth >= 3 && data->moves[data->ply - 1] != NULL_MOVE && !skipMove && eval >= beta && HasNonPawn(board)) {
      int R = 3 + depth / 6 + min((eval - beta) / 200, 3);
      R = min(depth, R); // don't go too low

      data->moves[data->ply++] = NULL_MOVE;
      MakeNullMove(board);

      score = -Negamax(-beta, -beta + 1, depth - R, thread, &childPv);

      UndoNullMove(board);
      data->ply--;

      if (score >= beta)
        return beta;

      nullThreat = childPv.count ? childPv.moves[0] : NULL_MOVE;
    }

    // Prob cut
    // If a relatively deep search from our TT doesn't say this node is
    // less than beta + margin, then we run a shallow search to look
    int probBeta = beta + 100;
    if (depth > 4 && abs(beta) < MATE_BOUND &&
        !(ttHit && tt->depth >= depth - 3 && TTScore(tt, data->ply) < probBeta)) {

      InitTacticalMoves(&moves, data);
      while ((move = NextMove(&moves, board, 1))) {
        data->moves[data->ply++] = move;
        MakeMove(move, board);

        // qsearch to quickly check
        score = -Quiesce(-probBeta, -probBeta + 1, thread, pv);

        // if it's still above our cutoff, revalidate
        if (score >= probBeta)
          score = -Negamax(-probBeta, -probBeta + 1, depth - 4, thread, pv);

        UndoMove(move, board);
        data->ply--;

        if (score >= probBeta)
          return score;
      }
    }
  }

  Move quiets[64] = {0};
  int totalMoves = 0, nonPrunedMoves = 0, numQuiets = 0;
  InitAllMoves(&moves, hashMove, data);

  while ((move = NextMove(&moves, board, 0))) {
    // don't search this during singular
    if (skipMove == move)
      continue;

    int tactical = !!Tactical(move);
    int specialQuiet = !tactical && (move == moves.killer1 || move == moves.killer2 || move == moves.counter);
    int hist = !tactical ? data->hh[board->side][MoveStartEnd(move)] : 0;

    if (bestScore > -MATE_BOUND && depth <= 8 && !tactical && totalMoves > LMP[improving][depth])
      continue;

    totalMoves++;

    if (bestScore > -MATE_BOUND && !tactical && depth <= 2 && !specialQuiet && hist <= -2048 * depth * depth)
      continue;

    // Static evaluation pruning, this applies for both quiet and tactical moves
    // quiet moves use a quadratic scale upwards
    if (bestScore > -MATE_BOUND && SEE(board, move) < STATIC_PRUNE[tactical][depth])
      continue;

    nonPrunedMoves++;

    if (isRoot && !thread->idx && GetTimeMS() - 2500 >= params->startTime)
      printf("info depth %d currmove %s currmovenumber %d\n", depth, MoveToStr(move), nonPrunedMoves);

    if (!tactical)
      quiets[numQuiets++] = move;

    // singular extension
    // if one move is better than all the rest, then we consider this singular
    // and look at it more (extend). Singular is determined by checking all other
    // moves at a shallow depth on a nullwindow that is somewhere below the tt evaluation
    // implemented using "skip move" recursion like in SF (allows for reductions when doing singular search)
    int extension = 0;
    if (depth >= 8 && !skipMove && !isRoot && ttHit && move == tt->move && tt->depth >= depth - 3 &&
        abs(TTScore(tt, data->ply)) < MATE_BOUND && (tt->flags & TT_LOWER)) {
      int sBeta = max(TTScore(tt, data->ply) - depth * 2, -CHECKMATE);
      int sDepth = depth / 2 - 1;

      data->skipMove[data->ply] = move;
      score = Negamax(sBeta - 1, sBeta, sDepth, thread, pv);
      data->skipMove[data->ply] = NULL_MOVE;

      // no score failed above sBeta, so this is singular
      if (score < sBeta)
        extension = 1 + (!isPV && score < sBeta - 100);
    }

    // re-capture extension - looks for a follow up capture on the same square
    // as the previous capture
    if (isPV && !isRoot && !extension) {
      Move parentMove = data->moves[data->ply - 1];

      if (!(MoveCapture(parentMove) ^ MoveCapture(move)) && MoveEnd(parentMove) == MoveEnd(move))
        extension = 1;
    }

    data->moves[data->ply++] = move;
    MakeMove(move, board);

    // apply extensions
    int newDepth = depth + max(extension, !!board->checkers);

    // Late move reductions
    int R = 1;
    if (depth > 2 && nonPrunedMoves > 1 && !specialQuiet) {
      R = LMR[min(depth, 63)][min(nonPrunedMoves, 63)];

      if (!tactical) {
        // increase reduction on non-pv
        if (!isPV)
          R++;

        // increase reduction if our eval is declining
        if (!improving)
          R++;

        if (MoveCapture(nullThreat) && MoveStart(move) != MoveEnd(nullThreat) && !board->checkers)
          R++;

        // adjust reduction based on historical score
        R -= hist / 16384;
      } else {
        R--;
      }

      // prevent dropping into QS, extending, or reducing all extensions
      R = min(depth - 1, max(R, 1));
    }

    // First move of a PV node
    if (isPV && nonPrunedMoves == 1) {
      score = -Negamax(-beta, -alpha, newDepth - 1, thread, &childPv);
    } else {
      // potentially reduced search
      score = -Negamax(-alpha - 1, -alpha, newDepth - R, thread, &childPv);

      if (score > alpha && R != 1) // failed high on a reducede search, try again
        score = -Negamax(-alpha - 1, -alpha, newDepth - 1, thread, &childPv);

      if (score > alpha && (isRoot || score < beta)) // failed high again, do full window
        score = -Negamax(-beta, -alpha, newDepth - 1, thread, &childPv);
    }

    UndoMove(move, board);
    data->ply--;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha) {
        alpha = score;

        // copy pv when alpha is raised
        pv->count = childPv.count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv.moves, childPv.count * sizeof(Move));
      }

      // we're failing high
      if (alpha >= beta) {
        UpdateHistories(data, move, depth, board->side, &quiets, numQuiets);
        break;
      }
    }
  }

  // Checkmate detection using movecount
  if (!totalMoves)
    return board->checkers ? -CHECKMATE + data->ply : 0;

  // don't let our score inflate too high (tb)
  bestScore = min(bestScore, maxScore);

  // prevent saving when in singular search
  if (!skipMove) {
    // save to the TT
    // TT_LOWER = we failed high, TT_UPPER = we didnt raise alpha, TT_EXACT = in
    int TTFlag = bestScore >= beta ? TT_LOWER : bestScore <= origAlpha ? TT_UPPER : TT_EXACT;
    TTPut(board->zobrist, depth, bestScore, TTFlag, bestMove, data->ply, data->evals[data->ply]);
  }

  return bestScore;
}

int Quiesce(int alpha, int beta, ThreadData* thread, PV* pv) {
  SearchParams* params = thread->params;
  SearchData* data = &thread->data;
  Board* board = &thread->board;

  PV childPv;
  pv->count = 0;

  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  // Either mainthread has ended us OR we've run out of time
  // this second check is more expensive and done only every 1024 nodes
  // 1Mnps ~1ms
  if (params->stopped || ((data->nodes & 1023) == 0 && params->timeset && GetTimeMS() > params->endTime))
    longjmp(thread->exit, 1);

  // draw check
  if (IsMaterialDraw(board) || IsRepetition(board, data->ply) || (board->halfMove > 99))
    return 0;

  // prevent overflows
  if (data->ply > MAX_SEARCH_PLY - 1)
    return Evaluate(board, thread);

  // check the transposition table for previous info
  int ttHit = 0;
  TTEntry* tt = TTProbe(&ttHit, board->zobrist);
  // TT score pruning - no depth check required since everything in QS is depth 0
  if (ttHit) {
    int ttScore = TTScore(tt, data->ply);
    if ((tt->flags & TT_EXACT) || ((tt->flags & TT_LOWER) && ttScore >= beta) ||
        ((tt->flags & TT_UPPER) && ttScore <= alpha))
      return ttScore;
  }

  // pull cached eval if it exists
  int eval = data->evals[data->ply] = (ttHit ? tt->eval : Evaluate(board, thread));

  // can we use an improved evaluation from the tt?
  if (ttHit) {
    int ttEval = TTScore(tt, data->ply);
    if (tt->flags & (ttEval > eval ? TT_LOWER : TT_UPPER))
      eval = ttEval;
  }

  // stand pat
  if (eval >= beta)
    return eval;

  if (eval > alpha)
    alpha = eval;

  int bestScore = eval;
  Move bestMove = NULL_MOVE;
  int origAlpha = alpha;

  Move move;
  MoveList moves;
  InitTacticalMoves(&moves, data);

  while ((move = NextMove(&moves, board, 1))) {
    int see = SEE(board, move);
    // a delta prune look-a-like by Halogen
    // prune based on SEE scores rather than flat mat val
    if (eval + see + DELTA_CUTOFF < alpha || see < 0)
      continue;

    data->moves[data->ply++] = move;
    MakeMove(move, board);

    int score = -Quiesce(-beta, -alpha, thread, &childPv);

    UndoMove(move, board);
    data->ply--;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha) {
        alpha = score;

        // copy pv
        pv->count = childPv.count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv.moves, childPv.count * sizeof(Move));
      }

      // failed high
      if (alpha >= beta)
        break;
    }
  }

  int TTFlag = bestScore >= beta ? TT_LOWER : bestScore <= origAlpha ? TT_UPPER : TT_EXACT;
  TTPut(board->zobrist, 0, bestScore, TTFlag, bestMove, data->ply, data->evals[data->ply]);

  return bestScore;
}

inline void PrintInfo(PV* pv, int score, int depth, ThreadData* thread) {
  uint64_t nodes = NodesSearched(thread->threads);
  uint64_t tbhits = TBHits(thread->threads);
  uint64_t time = GetTimeMS() - thread->params->startTime;
  uint64_t nps = 1000 * nodes / max(time, 1);
  int hashfull = TTFull();

  if (score > MATE_BOUND) {
    int movesToMate = (CHECKMATE - score) / 2 + ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d nodes %lld nps %lld tbhits %lld hashfull %d time %lld score mate %d pv ", depth,
           thread->data.seldepth, nodes, nps, tbhits, hashfull, time, movesToMate);
  } else if (score < -MATE_BOUND) {
    int movesToMate = (CHECKMATE + score) / 2 - ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d  nodes %lld nps %lld tbhits %lld hashfull %d time %lld score mate -%d pv ", depth,
           thread->data.seldepth, nodes, nps, tbhits, hashfull, time, movesToMate);
  } else {
    printf("info depth %d seldepth %d nodes %lld nps %lld tbhits %lld hashfull %d time %lld score cp %d pv ", depth,
           thread->data.seldepth, nodes, nps, tbhits, hashfull, time, score);
  }

  if (pv->count)
    PrintPV(pv);
  else
    printf("%s\n", MoveToStr(pv->moves[0]));
}

void PrintPV(PV* pv) {
  for (int i = 0; i < pv->count; i++)
    printf("%s ", MoveToStr(pv->moves[i]));
  printf("\n");
}