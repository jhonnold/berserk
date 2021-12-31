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

#include "search.h"

#include <assert.h>
#include <inttypes.h>
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
#include "nn.h"
#include "noobprobe/noobprobe.h"
#include "pyrrhic/tbprobe.h"
#include "see.h"
#include "tb.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

// arrays to store these pruning cutoffs at specific depths
int LMR[MAX_SEARCH_PLY][64];
int LMP[2][MAX_SEARCH_PLY];
int STATIC_PRUNE[2][MAX_SEARCH_PLY];
extern volatile int PONDERING;

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

    STATIC_PRUNE[0][depth] = -SEE_PRUNE_CUTOFF * depth * depth;  // quiet move cutoff
    STATIC_PRUNE[1][depth] = -SEE_PRUNE_CAPTURE_CUTOFF * depth;  // capture cutoff
  }
}

INLINE int StopSearch(SearchParams* params) {
  if (!params->timeset || PONDERING) return 0;

  long elapsed = GetTimeMS() - params->start;
  return elapsed > params->max;
}

void* UCISearch(void* arg) {
  SearchArgs* args = (SearchArgs*)arg;

  Board* board = args->board;
  SearchParams* params = args->params;
  ThreadData* threads = args->threads;

  SearchResults results = {0};
  BestMove(board, params, threads, &results);

  free(args);
  return NULL;
}

void BestMove(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results) {
  Move bestMove;
  if ((bestMove = TBRootProbe(board))) {
    while (PONDERING)
      ;

    printf("bestmove %s\n", MoveToStr(bestMove, board));
  } else if ((bestMove = ProbeNoob(board))) {
    while (PONDERING)
      ;

    printf("bestmove %s\n", MoveToStr(bestMove, board));
  } else {
    pthread_t pthreads[threads->count];
    InitPool(board, params, threads, results);

    params->stopped = 0;
    TTUpdate();

    // start at 1, we will resuse main-thread
    for (int i = 1; i < threads->count; i++) pthread_create(&pthreads[i], NULL, &Search, &threads[i]);
    Search(&threads[0]);

    // if main thread stopped, then stop all and wait till complete
    params->stopped = 1;
    for (int i = 1; i < threads->count; i++) pthread_join(pthreads[i], NULL);

    while (PONDERING)
      ;

    printf("bestmove %s", MoveToStr(results->bestMoves[results->depth], board));
    if (results->ponderMoves[results->depth])
      printf(" ponder %s", MoveToStr(results->ponderMoves[results->depth], board));

    printf("\n");
  }
}

void* Search(void* arg) {
  ThreadData* thread = (ThreadData*)arg;
  SearchParams* params = thread->params;
  SearchResults* results = thread->results;
  SearchData* data = &thread->data;
  Board* board = &thread->board;

  int mainThread = !thread->idx;
  int alpha = -CHECKMATE;
  int beta = CHECKMATE;
  int cfh = 0;
  int searchStability = 0;

  board->ply = 0;
  RefreshAccumulator(board->accumulators[WHITE][board->ply], board, WHITE);
  RefreshAccumulator(board->accumulators[BLACK][board->ply], board, BLACK);

  data->contempt[WHITE] = data->contempt[BLACK] = 0;

  // set a hot exit point for this thread
  if (!setjmp(thread->exit)) {
    // Iterative deepening
    for (int depth = 1; depth <= params->depth; depth++) {
      for (thread->multiPV = 0; thread->multiPV < params->multiPV; thread->multiPV++) {
        PV* pv = &thread->pvs[thread->multiPV];

        // delta is our window for search. early depths get full searches
        // as we don't know what score to expect. Otherwise we start with a window of 16 (8x2), but
        // vary this slightly based on the previous depths window expansion count
        int delta = WINDOW;
        int score = thread->scores[thread->multiPV];
        int searchDepth = thread->depth = depth;

        alpha = -CHECKMATE;
        beta = CHECKMATE;
        delta = CHECKMATE;

        if (depth >= 5) {
          alpha = max(score - WINDOW, -CHECKMATE);
          beta = min(score + WINDOW, CHECKMATE);
          delta = WINDOW;
        }

        SetContempt(data->contempt, board->stm, score);

        while (!params->stopped) {
          data->window = beta - alpha;

          // search!
          score = Negamax(alpha, beta, searchDepth, 0, thread, pv);

          if (mainThread && (score <= alpha || score >= beta) && params->multiPV == 1 &&
              GetTimeMS() - params->start >= 2500)
            PrintInfo(pv, score, thread, alpha, beta, 1, board);

          if (score <= alpha) {
            // adjust beta downward when failing low
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, -CHECKMATE);

            searchDepth = depth;
            cfh = 0;
          } else if (score >= beta) {
            beta = min(beta + delta, CHECKMATE);

            if (abs(score) < TB_WIN_BOUND) {
              cfh += 2;
              searchDepth -= cfh;
              searchDepth = max(1, searchDepth);
            }
          } else {
            thread->scores[thread->multiPV] = score;
            thread->bestMoves[thread->multiPV] = pv->moves[0];

            cfh = 0;
            break;
          }

          // delta x 1.25
          delta += delta / 4;
        }
      }

      // sort multi pv
      for (int i = 0; i < params->multiPV; i++) {
        int best = i;

        for (int j = i + 1; j < params->multiPV; j++)
          if (thread->scores[j] > thread->scores[best]) best = j;

        if (best != i) {
          Score tempS = thread->scores[best];
          Move tempM = thread->bestMoves[best];

          thread->scores[best] = thread->scores[i];
          thread->bestMoves[best] = thread->bestMoves[i];

          thread->scores[i] = tempS;
          thread->bestMoves[i] = tempM;
        }
      }

      if (mainThread) {
        results->depth = depth;
        results->scores[depth] = thread->scores[0];
        results->bestMoves[depth] = thread->bestMoves[0];
        results->ponderMoves[depth] = thread->pvs[0].count > 1 ? thread->pvs[0].moves[1] : NULL_MOVE;

        for (int i = 0; i < params->multiPV; i++)
          PrintInfo(&thread->pvs[i], thread->scores[i], thread, -CHECKMATE, CHECKMATE, i + 1, board);
      }

      if (!mainThread || depth < 5 || !params->timeset) continue;

      int sameBestMove = results->bestMoves[depth] == results->bestMoves[depth - 1];  // same move?
      searchStability = sameBestMove ? min(10, searchStability + 1) : 0;  // increase how stable our best move is
      double stabilityFactor = 1.25 - 0.05 * searchStability;

      double scoreDiff = results->scores[depth - 3] - results->scores[depth];
      double scoreChangeFactor = 0.1 + 0.05 * scoreDiff;
      scoreChangeFactor = max(0.5, min(1.5, scoreChangeFactor));

      int64_t bestMoveNodes = data->tm[FromTo(results->bestMoves[depth])];
      double pctNodesNotBest = 1.0 - (double)bestMoveNodes / data->nodes;
      double nodeCountFactor = max(0.5, pctNodesNotBest * 2 + 0.4);

      if (results->scores[depth] > MATE_BOUND) nodeCountFactor = 0.5;

      long elapsed = GetTimeMS() - params->start;

      if (elapsed > params->alloc * stabilityFactor * scoreChangeFactor * nodeCountFactor) {
        params->stopped = 1;
        break;
      }
    }
  }

  return NULL;
}

int Negamax(int alpha, int beta, int depth, int cutnode, ThreadData* thread, PV* pv) {
  SearchParams* params = thread->params;
  SearchData* data = &thread->data;
  Board* board = &thread->board;

  PV childPv;
  pv->count = 0;

  int isPV = beta - alpha != 1;  // pv node when doing a full window
  int isRoot = !data->ply;       //
  int score = -CHECKMATE;        // initially assume the worst case
  int bestScore = -CHECKMATE;    //
  int maxScore = CHECKMATE;      // best possible
  int origAlpha = alpha;         // remember first alpha for tt storage
  int ttScore = UNKNOWN;

  Move bestMove = NULL_MOVE;
  Move skipMove = data->skipMove[data->ply];  // skip used in SE (concept from SF)
  Move hashMove = NULL_MOVE;

  Move move;
  MoveList moves;

  // drop into tactical moves only
  if (depth <= 0) {
    if (board->checkers)
      depth = 1;
    else
      return Quiesce(alpha, beta, thread);
  }

  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  // Either mainthread has ended us OR we've run out of time
  // this second check is more expensive and done only every 1024 nodes
  // 1Mnps ~1ms
  if (params->stopped || (!(data->nodes & 1023) && StopSearch(params))) longjmp(thread->exit, 1);

  if (!isRoot) {
    // draw
    if (IsRepetition(board, data->ply) || IsMaterialDraw(board) || (board->halfMove > 99))
      return 2 - (data->nodes & 0x3);

    // Prevent overflows
    if (data->ply >= MAX_SEARCH_PLY - 1) return board->checkers ? 0 : Evaluate(board, thread);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + data->ply);
    beta = min(beta, CHECKMATE - data->ply - 1);
    if (alpha >= beta) return alpha;
  }

  // check the transposition table for previous info
  // we ignore the tt on singular extension searches
  int ttHit = 0;
  TTEntry* tt = skipMove ? NULL : TTProbe(&ttHit, board->zobrist);
  ttScore = ttHit ? TTScore(tt, data->ply) : UNKNOWN;
  hashMove = isRoot ? thread->pvs[thread->multiPV].moves[0] : ttHit ? tt->move : NULL_MOVE;

  // if the TT has a value that fits our position and has been searched to an equal or greater depth, then we accept
  // this score and prune
  if (!isPV && ttHit && tt->depth >= depth && ttScore != UNKNOWN) {
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
          score = TB_WIN_SCORE - data->ply;
          flag = TT_LOWER;
          break;
        case TB_LOSS:
          score = -TB_WIN_SCORE + data->ply;
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
  if (depth >= 4 && !hashMove && !skipMove) depth--;

  // pull previous static eval from tt - this is depth independent
  int eval;
  if (!skipMove) {
    eval = data->evals[data->ply] = board->checkers ? UNKNOWN : (ttHit ? tt->eval : Evaluate(board, thread));
  } else {
    // after se, just used already determined eval
    eval = data->evals[data->ply];
  }

  if (!ttHit) TTPut(board->zobrist, INT8_MIN, UNKNOWN, TT_UNKNOWN, NULL_MOVE, data->ply, eval);

  // getting better if eval has gone up
  int improving = !board->checkers && data->ply >= 2 &&
                  (data->evals[data->ply] > data->evals[data->ply - 2] || data->evals[data->ply - 2] == UNKNOWN);

  // reset moves to moves related to 1 additional ply
  data->skipMove[data->ply + 1] = NULL_MOVE;
  data->killers[data->ply + 1][0] = NULL_MOVE;
  data->killers[data->ply + 1][1] = NULL_MOVE;

  BitBoard oppThreats = Threats(board, board->xstm);

  if (!isPV && !board->checkers) {
    // Our TT might have a more accurate evaluation score, use this
    if (ttHit && tt->depth >= depth && ttScore != UNKNOWN) {
      if (tt->flags & (ttScore > eval ? TT_LOWER : TT_UPPER)) eval = ttScore;
    }

    // Reverse Futility Pruning
    // i.e. the static eval is so far above beta we prune
    if (depth <= 6 && !skipMove && eval - 50 * (depth - (improving && !oppThreats)) > beta && eval < TB_WIN_BOUND)
      return eval;

    // Null move pruning
    // i.e. Our position is so good we can give our opponnent a free move and
    // they still can't catch up (this is usually countered by captures or mate threats)
    if (depth >= 3 && data->moves[data->ply - 1] != NULL_MOVE && !skipMove && eval >= beta &&
        // weiss conditional
        HasNonPawn(board) > (depth > 12)) {
      int R = 4 + depth / 6 + min((eval - beta) / 256, 3);
      R = min(depth, R);  // don't go too low

      data->moves[data->ply++] = NULL_MOVE;
      MakeNullMove(board);

      score = -Negamax(-beta, -beta + 1, depth - R, !cutnode, thread, &childPv);

      UndoNullMove(board);
      data->ply--;

      if (score >= beta) return score < TB_WIN_BOUND ? score : beta;
    }

    // Prob cut
    // If a relatively deep search from our TT doesn't say this node is
    // less than beta + margin, then we run a shallow search to look
    BitBoard ownThreats = Threats(board, board->stm);
    int probBeta = beta + 110;
    if (depth > 4 && abs(beta) < TB_WIN_BOUND && ownThreats &&
        !(ttHit && tt->depth >= depth - 3 && ttScore < probBeta)) {
      InitTacticalMoves(&moves, data, 0);
      while ((move = NextMove(&moves, board, 1))) {
        if (skipMove == move) continue;

        data->moves[data->ply++] = move;
        MakeMove(move, board);

        // qsearch to quickly check
        score = -Quiesce(-probBeta, -probBeta + 1, thread);

        // if it's still above our cutoff, revalidate
        if (score >= probBeta) score = -Negamax(-probBeta, -probBeta + 1, depth - 4, !cutnode, thread, pv);

        UndoMove(move, board);
        data->ply--;

        if (score >= probBeta) return score;
      }
    }
  }

  Move quiets[64];
  Move tacticals[64];

  int legalMoves = 0, playedMoves = 0, numQuiets = 0, numTacticals = 0, skipQuiets = 0;
  InitAllMoves(&moves, hashMove, data, oppThreats);

  while ((move = NextMove(&moves, board, skipQuiets))) {
    int64_t startingNodeCount = data->nodes;

    if (isRoot && MoveSearchedByMultiPV(thread, move)) continue;
    if (isRoot && !MoveSearchable(params, move)) continue;

    // don't search this during singular
    if (skipMove == move) continue;

    legalMoves++;

    int extension = 0;
    int tactical = IsTactical(move);
    int killerOrCounter = move == moves.killer1 || move == moves.killer2 || move == moves.counter;
    int history = GetQuietHistory(data, move, board->stm, oppThreats);
    int counterHistory = GetCounterHistory(data, move);

    if (bestScore > -MATE_BOUND) {
      if (legalMoves >= LMP[improving][depth]) skipQuiets = 1;

      if (!tactical) {
        if (depth < 3 && !killerOrCounter && (history < -4096 * depth || counterHistory < -2048 * depth)) continue;

        if (depth < 9 && eval + 100 + 50 * depth + history / 512 <= alpha) skipQuiets = 1;

        if (SEE(board, move) < STATIC_PRUNE[0][depth]) continue;
      } else {
        if (moves.phase > PLAY_GOOD_TACTICAL && SEE(board, move) < STATIC_PRUNE[1][depth]) continue;
      }
    }

    playedMoves++;

    if (isRoot && !thread->idx && GetTimeMS() - params->start > 2500)
      printf("info depth %d multipv %d currmove %s currmovenumber %d\n", thread->depth, thread->multiPV + 1,
             MoveToStr(move, board), playedMoves + thread->multiPV);

    if (!tactical)
      quiets[numQuiets++] = move;
    else
      tacticals[numTacticals++] = move;

    // singular extension
    // if one move is better than all the rest, then we consider this singular
    // and look at it more (extend). Singular is determined by checking all other
    // moves at a shallow depth on a nullwindow that is somewhere below the tt evaluation
    // implemented using "skip move" recursion like in SF (allows for reductions when doing singular search)
    if (!isRoot && depth >= 7 && ttHit && move == tt->move && tt->depth >= depth - 3 && abs(ttScore) < TB_WIN_BOUND &&
        (tt->flags & TT_LOWER)) {
      int sBeta = max(ttScore - 3 * depth / 2, -CHECKMATE);
      int sDepth = depth / 2 - 1;

      data->skipMove[data->ply] = move;
      score = Negamax(sBeta - 1, sBeta, sDepth, cutnode, thread, pv);
      data->skipMove[data->ply] = NULL_MOVE;

      // no score failed above sBeta, so this is singular
      if (score < sBeta)
        extension = 1 + (!isPV && score < sBeta - 50);
      else if (sBeta >= beta)
        return sBeta;
    }

    // history extension - if the tt move has a really good history score, extend.
    // thank you to Connor, author of Seer for this idea
    else if (!isRoot && depth >= 7 && ttHit && move == tt->move && abs(ttScore) < TB_WIN_BOUND && history >= 98304)
      extension = 1;

    // re-capture extension - looks for a follow up capture on the same square
    // as the previous capture
    else if (!isRoot && isPV && IsRecapture(data, move))
      extension = 1;

    data->moves[data->ply++] = move;
    MakeMove(move, board);

    // apply extensions
    int newDepth = depth + extension;

    // Late move reductions
    int R = 1;
    if (depth > 2 && playedMoves > 1) {
      R = LMR[min(depth, 63)][min(playedMoves, 63)];

      if (!tactical) {
        // increase reduction on non-pv
        if (!isPV) R++;

        if (isPV && (beta - alpha) * 2 < data->window) R++;

        // increase reduction if our eval is declining
        if (!improving) R++;

        if (killerOrCounter) R -= 2;

        // move GAVE check
        if (board->checkers) R--;

        // adjust reduction based on historical score
        R -= history / 20480;
      } else {
        int th = GetTacticalHistory(data, board, move);
        R = 1 - 4 * th / (abs(th) + 24576);
      }

      // Reduce more on expected cut nodes
      // idea from komodo/sf, explained by Don Daily here
      // https://talkchess.com/forum3/viewtopic.php?f=7&t=47577&start=10#p519741
      // and https://www.chessprogramming.org/Node_Types
      if (cutnode) R++;

      // prevent dropping into QS, extending, or reducing all extensions
      R = min(depth - 1, max(R, 1));
    }

    // First move of a PV node
    if (isPV && playedMoves == 1) {
      score = -Negamax(-beta, -alpha, newDepth - 1, 0, thread, &childPv);
    } else {
      // potentially reduced search
      score = -Negamax(-alpha - 1, -alpha, newDepth - R, 1, thread, &childPv);

      if (score > alpha && R != 1)  // failed high on a reducede search, try again
        score = -Negamax(-alpha - 1, -alpha, newDepth - 1, !cutnode, thread, &childPv);

      if (score > alpha && (isRoot || score < beta))  // failed high again, do full window
        score = -Negamax(-beta, -alpha, newDepth - 1, 0, thread, &childPv);
    }

    UndoMove(move, board);
    data->ply--;

    if (isRoot) data->tm[FromTo(move)] += data->nodes - startingNodeCount;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if ((isPV && score > alpha) || (isRoot && playedMoves == 1)) {
        pv->count = childPv.count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv.moves, childPv.count * sizeof(Move));
      }

      if (score > alpha) alpha = score;

      // we're failing high
      if (alpha >= beta) {
        UpdateHistories(board, data, move, depth + (bestScore > beta + 100), board->stm, quiets, numQuiets, tacticals,
                        numTacticals, oppThreats);
        break;
      }
    }
  }

  // Checkmate detection using movecount
  if (!legalMoves) return board->checkers ? -CHECKMATE + data->ply : 0;

  // don't let our score inflate too high (tb)
  bestScore = min(bestScore, maxScore);

  // prevent saving when in singular search
  if (!skipMove && !(isRoot && thread->multiPV > 0)) {
    // save to the TT
    // TT_LOWER = we failed high, TT_UPPER = we didnt raise alpha, TT_EXACT = in
    int TTFlag = bestScore >= beta ? TT_LOWER : bestScore <= origAlpha ? TT_UPPER : TT_EXACT;
    Move moveToStore = ttHit && TTFlag == TT_UPPER && (tt->flags & TT_LOWER) ? hashMove : bestMove;
    TTPut(board->zobrist, depth, bestScore, TTFlag, moveToStore, data->ply, data->evals[data->ply]);
  }

  return bestScore;
}

int Quiesce(int alpha, int beta, ThreadData* thread) {
  SearchParams* params = thread->params;
  SearchData* data = &thread->data;
  Board* board = &thread->board;

  data->nodes++;

  // Either mainthread has ended us OR we've run out of time
  // this second check is more expensive and done only every 1024 nodes
  // 1Mnps ~1ms
  if (params->stopped || (!(data->nodes & 1023) && StopSearch(params))) longjmp(thread->exit, 1);

  // draw check
  if (IsMaterialDraw(board) || IsRepetition(board, data->ply) || (board->halfMove > 99)) return 0;

  // prevent overflows
  if (data->ply >= MAX_SEARCH_PLY - 1) return board->checkers ? 0 : Evaluate(board, thread);

  // check the transposition table for previous info
  int ttHit = 0, ttScore = UNKNOWN;
  TTEntry* tt = TTProbe(&ttHit, board->zobrist);
  // TT score pruning - no depth check required since everything in QS is depth 0
  if (ttHit) {
    ttScore = TTScore(tt, data->ply);

    if (ttScore != UNKNOWN && ((tt->flags & TT_EXACT) || ((tt->flags & TT_LOWER) && ttScore >= beta) ||
                               ((tt->flags & TT_UPPER) && ttScore <= alpha)))
      return ttScore;
  }

  Move bestMove = NULL_MOVE;
  int origAlpha = alpha;
  int bestScore = -CHECKMATE + data->ply;

  // pull cached eval if it exists
  int eval = data->evals[data->ply] = board->checkers ? UNKNOWN : (ttHit ? tt->eval : Evaluate(board, thread));
  if (!ttHit) TTPut(board->zobrist, INT8_MIN, UNKNOWN, TT_UNKNOWN, NULL_MOVE, data->ply, eval);

  // can we use an improved evaluation from the tt?
  if (ttHit && ttScore != UNKNOWN) {
    if (tt->flags & (ttScore > eval ? TT_LOWER : TT_UPPER)) eval = ttScore;
  }

  // stand pat
  if (!board->checkers) {
    if (eval >= beta) return eval;

    if (eval > alpha) alpha = eval;

    bestScore = eval;
  }

  Move move;
  MoveList moves;

  int seeThreshold = max(0, alpha - eval - DELTA_CUTOFF);
  InitTacticalMoves(&moves, data, seeThreshold);

  while ((move = NextMove(&moves, board, 1))) {
    if (moves.phase > PLAY_GOOD_TACTICAL) break;

    data->moves[data->ply++] = move;
    MakeMove(move, board);

    int score = -Quiesce(-beta, -alpha, thread);

    UndoMove(move, board);
    data->ply--;

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;

      if (score > alpha) alpha = score;

      // failed high
      if (alpha >= beta) break;
    }
  }

  int TTFlag = bestScore >= beta ? TT_LOWER : bestScore <= origAlpha ? TT_UPPER : TT_EXACT;
  TTPut(board->zobrist, 0, bestScore, TTFlag, bestMove, data->ply, data->evals[data->ply]);

  return bestScore;
}

inline void PrintInfo(PV* pv, int score, ThreadData* thread, int alpha, int beta, int multiPV, Board* board) {
  int depth = thread->depth;
  int seldepth = Seldepth(thread);
  uint64_t nodes = NodesSearched(thread->threads);
  uint64_t tbhits = TBHits(thread->threads);
  uint64_t time = GetTimeMS() - thread->params->start;
  uint64_t nps = 1000 * nodes / max(time, 1);
  int hashfull = TTFull();
  int bounded = max(alpha, min(beta, score));

  int printable = bounded > MATE_BOUND    ? (CHECKMATE - bounded + 1) / 2
                  : bounded < -MATE_BOUND ? -(CHECKMATE + bounded) / 2
                                          : bounded;
  char* type = abs(bounded) > MATE_BOUND ? "mate" : "cp";
  char* bound = bounded >= beta ? " lowerbound " : bounded <= alpha ? " upperbound " : " ";

  printf("info depth %d seldepth %d multipv %d score %s %d%snodes %" PRId64 " nps %" PRId64
         " hashfull %d tbhits %" PRId64 " time %" PRId64 " pv ",
         depth, seldepth, multiPV, type, printable, bound, nodes, nps, hashfull, tbhits, time);

  if (pv->count)
    PrintPV(pv, board);
  else
    printf("%s\n", MoveToStr(pv->moves[0], board));
}

void PrintPV(PV* pv, Board* board) {
  for (int i = 0; i < pv->count; i++) printf("%s ", MoveToStr(pv->moves[i], board));
  printf("\n");
}

int MoveSearchedByMultiPV(ThreadData* thread, Move move) {
  for (int i = 0; i < thread->multiPV; i++)
    if (thread->bestMoves[i] == move) return 1;

  return 0;
}

int MoveSearchable(SearchParams* params, Move move) {
  if (!params->searchMoves) return 1;

  for (int i = 0; i < params->searchable.count; i++)
    if (move == params->searchable.moves[i]) return 1;

  return 0;
}