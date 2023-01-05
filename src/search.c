// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "nn.h"
#include "pyrrhic/tbprobe.h"
#include "see.h"
#include "tb.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"

// arrays to store these pruning cutoffs at specific depths
int LMR[MAX_SEARCH_PLY][64];
int LMP[2][MAX_SEARCH_PLY];
int STATIC_PRUNE[2][MAX_SEARCH_PLY];

void InitPruningAndReductionTables() {
  for (int depth = 1; depth < MAX_SEARCH_PLY; depth++)
    for (int moves = 1; moves < 64; moves++) LMR[depth][moves] = log(depth) * log(moves) / 2.25 + 0.25;

  LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;

  for (int depth = 0; depth < MAX_SEARCH_PLY; depth++) {
    // LMP has both a improving (more strict) and non-improving evalution parameter
    // for lmp. If the evaluation is getting better we want to check more
    LMP[0][depth] = (3 + depth * depth) / 2;
    LMP[1][depth] = 3 + depth * depth;

    STATIC_PRUNE[0][depth] = -SEE_PRUNE_CUTOFF * depth * depth; // quiet move cutoff
    STATIC_PRUNE[1][depth] = -SEE_PRUNE_CAPTURE_CUTOFF * depth; // capture cutoff
  }
}

INLINE void CheckLimits(ThreadData* thread) {
  if (thread->nodes % limits.hitrate != 0 || threadPool.ponder) return;

  long elapsed = GetTimeMS() - limits.start;
  if ((limits.timeset && elapsed >= limits.max) || (limits.nodes && NodesSearched() >= limits.nodes))
    threadPool.stop = 1;
}

void StartSearch(Board* board, uint8_t ponder) {
  if (threadPool.searching) ThreadWaitUntilSleep(threadPool.threads[0]);

  threadPool.stopOnPonderHit = 0;
  threadPool.stop            = 0;
  threadPool.ponder          = ponder;

  for (int i = 0; i < 64 * 64; i++) threadPool.threads[0]->nodeCounts[i] = 0;

  for (int i = 0; i < threadPool.count; i++) {
    ThreadData* thread = threadPool.threads[i];
    thread->nodes      = 0;
    thread->tbhits     = 0;
    thread->seldepth   = 1;

    SearchResults* results = &thread->results;
    results->prevScore     = results->depth > 0 ? results->scores[results->depth] : UNKNOWN;
    results->depth         = 0;

    memcpy(&thread->board, board, sizeof(Board));
    thread->board.accumulators[WHITE] = thread->accumulators[WHITE];
    thread->board.accumulators[BLACK] = thread->accumulators[BLACK];
    thread->board.refreshTable[WHITE] = thread->refreshTable[WHITE];
    thread->board.refreshTable[BLACK] = thread->refreshTable[BLACK];
  }

  threadPool.searching = 1;
  ThreadWake(threadPool.threads[0], THREAD_SEARCH);
}

void MainSearch() {
  ThreadData* thread     = threadPool.threads[0];
  Board* board           = &thread->board;
  SearchResults* results = &thread->results;

  TTUpdate();

  Move bestMove   = TBRootProbe(board);
  Move ponderMove = NULL_MOVE;
  if (!bestMove) {
    for (int i = 1; i < threadPool.count; i++) ThreadWake(threadPool.threads[i], THREAD_SEARCH);
    Search(thread);
  }

  pthread_mutex_lock(&threadPool.lock);
  if (!threadPool.stop && (threadPool.ponder || limits.infinite)) {
    threadPool.sleeping = 1;
    pthread_mutex_unlock(&threadPool.lock);
    ThreadWait(thread, &threadPool.stop);
  } else {
    pthread_mutex_unlock(&threadPool.lock);
  }

  threadPool.stop = 1;

  if (!bestMove) {
    for (int i = 1; i < threadPool.count; i++) ThreadWaitUntilSleep(threadPool.threads[i]);
    bestMove   = results->bestMoves[results->depth];
    ponderMove = results->ponderMoves[results->depth];
  }

  printf("bestmove %s", MoveToStr(bestMove, board));
  if (ponderMove) printf(" ponder %s", MoveToStr(ponderMove, board));
  printf("\n");
}

void Search(ThreadData* thread) {
  SearchResults* results = &thread->results;
  Board* board           = &thread->board;
  int mainThread         = !thread->idx;

  SearchStack searchStack[MAX_SEARCH_PLY + 4];
  SearchStack* ss = searchStack + 4;
  memset(searchStack, 0, 5 * sizeof(SearchStack));
  for (size_t i = 0; i < MAX_SEARCH_PLY; i++) (ss + i)->ply = i;
  for (size_t i = 1; i <= 4; i++) (ss - i)->ch = &thread->ch[WHITE_PAWN][A1];

  int alpha = -CHECKMATE;
  int beta  = CHECKMATE;

  board->acc = 0;
  ResetAccumulator(board->accumulators[WHITE][0], board, WHITE);
  ResetAccumulator(board->accumulators[BLACK][0], board, BLACK);

  SetContempt(thread->contempt, board->stm);

  int searchStability = 0;
  thread->depth       = 0;

  while (++thread->depth < MAX_SEARCH_PLY && !threadPool.stop) {
    if (limits.depth && mainThread && thread->depth > limits.depth) break;

    for (thread->multiPV = 0; thread->multiPV < limits.multiPV && !threadPool.stop; thread->multiPV++) {
      PV* pv = &thread->pvs[thread->multiPV];

      // delta is our window for search. early depths get full searches
      // as we don't know what score to expect. Otherwise we start with a window of 16 (8x2), but
      // vary this slightly based on the previous depths window expansion count
      int delta       = WINDOW;
      int score       = thread->scores[thread->multiPV];
      int searchDepth = thread->depth;

      alpha = -CHECKMATE;
      beta  = CHECKMATE;
      delta = CHECKMATE;

      if (thread->depth >= 5) {
        alpha = max(score - WINDOW, -CHECKMATE);
        beta  = min(score + WINDOW, CHECKMATE);
        delta = WINDOW;
      }

      while (1) {
        // search!
        score = Negamax(alpha, beta, searchDepth, 0, thread, pv, ss);

        if (threadPool.stop) break;

        if (mainThread && (score <= alpha || score >= beta) && limits.multiPV == 1 &&
            GetTimeMS() - limits.start >= 2500)
          PrintInfo(pv, score, thread, alpha, beta, 1, board);

        if (score <= alpha) {
          // adjust beta downward when failing low
          beta  = (alpha + beta) / 2;
          alpha = max(alpha - delta, -CHECKMATE);

          searchDepth = thread->depth;
          if (mainThread) threadPool.stopOnPonderHit = 0;
        } else if (score >= beta) {
          beta = min(beta + delta, CHECKMATE);

          if (abs(score) < TB_WIN_BOUND) searchDepth--;
        } else {
          thread->scores[thread->multiPV]    = score;
          thread->bestMoves[thread->multiPV] = pv->moves[0];

          break;
        }

        // delta x 1.25
        delta += delta / 4;
      }
    }

    // sort multi pv
    for (int i = 0; i < limits.multiPV; i++) {
      int best = i;

      for (int j = i + 1; j < limits.multiPV; j++)
        if (thread->scores[j] > thread->scores[best]) best = j;

      if (best != i) {
        Score tempS = thread->scores[best];
        Move tempM  = thread->bestMoves[best];

        thread->scores[best]    = thread->scores[i];
        thread->bestMoves[best] = thread->bestMoves[i];

        thread->scores[i]    = tempS;
        thread->bestMoves[i] = tempM;
      }
    }

    if (mainThread && !threadPool.stop) {
      results->depth                      = thread->depth;
      results->scores[thread->depth]      = thread->scores[0];
      results->bestMoves[thread->depth]   = thread->bestMoves[0];
      results->ponderMoves[thread->depth] = thread->pvs[0].count > 1 ? thread->pvs[0].moves[1] : NULL_MOVE;

      for (int i = 0; i < limits.multiPV; i++)
        PrintInfo(&thread->pvs[i], thread->scores[i], thread, -CHECKMATE, CHECKMATE, i + 1, board);
    }

    if (!mainThread) continue;

    if (limits.timeset && !threadPool.stop && !threadPool.stopOnPonderHit) {
      int sameBestMove       = results->bestMoves[thread->depth] == results->bestMoves[thread->depth - 1]; // same move?
      searchStability        = sameBestMove ? min(10, searchStability + 1) : 0; // increase how stable our best move is
      double stabilityFactor = 1.25 - 0.05 * searchStability;

      Score searchScoreDiff    = results->scores[thread->depth - 3] - results->scores[thread->depth];
      Score prevScoreDiff      = results->prevScore - results->scores[thread->depth];
      double scoreChangeFactor = 0.1 + 0.0275 * searchScoreDiff + 0.0275 * prevScoreDiff;
      scoreChangeFactor        = max(0.5, min(1.5, scoreChangeFactor));

      uint64_t bestMoveNodes = thread->nodeCounts[FromTo(results->bestMoves[thread->depth])];
      double pctNodesNotBest = 1.0 - (double) bestMoveNodes / thread->nodes;
      double nodeCountFactor = max(0.5, pctNodesNotBest * 2 + 0.4);
      if (results->scores[thread->depth] >= TB_WIN_BOUND) nodeCountFactor = 0.5;

      long elapsed = GetTimeMS() - limits.start;
      if (elapsed > limits.alloc * stabilityFactor * scoreChangeFactor * nodeCountFactor) {
        if (threadPool.ponder)
          threadPool.stopOnPonderHit = 1;
        else
          threadPool.stop = 1;
      }
    }
  }
}

int Negamax(int alpha, int beta, int depth, int cutnode, ThreadData* thread, PV* pv, SearchStack* ss) {
  Board* board = &thread->board;

  PV childPv;
  pv->count = 0;

  int isPV      = beta - alpha != 1; // pv node when doing a full window
  int isRoot    = !ss->ply;          //
  int score     = -CHECKMATE;        // initially assume the worst case
  int bestScore = -CHECKMATE;        //
  int maxScore  = CHECKMATE;         // best possible
  int origAlpha = alpha;             // remember first alpha for tt storage
  int ttScore   = UNKNOWN;
  int ttPv      = 0;

  Move bestMove = NULL_MOVE, hashMove = NULL_MOVE;

  Move move;
  MovePicker mp;

  // drop into tactical moves only
  if (depth <= 0) {
    if (board->checkers)
      depth = 1;
    else
      return Quiesce(alpha, beta, thread, ss);
  }

  if (!thread->idx) CheckLimits(thread);

  thread->nodes++;
  thread->seldepth = max(ss->ply + 1, thread->seldepth);

  if (!isRoot) {
    if (load_rlx(threadPool.stop)) return 0;

    // draw
    if (IsDraw(board, ss->ply)) return 2 - (thread->nodes & 0x3);

    // Prevent overflows
    if (ss->ply >= MAX_SEARCH_PLY - 1) return board->checkers ? 0 : Evaluate(board, thread);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + ss->ply);
    beta  = min(beta, CHECKMATE - ss->ply - 1);
    if (alpha >= beta) return alpha;
  }

  // check the transposition table for previous info
  // we ignore the tt on singular extension searches
  TTEntry* tt = ss->skip ? NULL : TTProbe(board->zobrist);
  ttScore     = tt ? TTScore(tt, ss->ply) : UNKNOWN;
  ttPv        = isPV || (tt && (tt->flags & TT_PV));
  hashMove    = isRoot ? thread->pvs[thread->multiPV].moves[0] : tt ? tt->move : NULL_MOVE;

  // if the TT has a value that fits our position and has been searched to an equal or greater depth, then we accept
  // this score and prune
  if (!isPV && tt && tt->depth >= depth && ttScore != UNKNOWN) {
    if ((tt->flags & TT_EXACT) || ((tt->flags & TT_LOWER) && ttScore >= beta) ||
        ((tt->flags & TT_UPPER) && ttScore <= alpha))
      return ttScore;
  }

  // tablebase - we do not do this at root
  if (!isRoot) {
    unsigned tbResult = TBProbe(board);

    if (tbResult != TB_RESULT_FAILED) {
      thread->tbhits++;

      int flag;
      switch (tbResult) {
        case TB_WIN:
          score = TB_WIN_SCORE - ss->ply;
          flag  = TT_LOWER;
          break;
        case TB_LOSS:
          score = -TB_WIN_SCORE + ss->ply;
          flag  = TT_UPPER;
          break;
        default:
          score = 0;
          flag  = TT_EXACT;
          break;
      }

      // if the tablebase gives us what we want, then we accept it's score and return
      if ((flag & TT_EXACT) || ((flag & TT_LOWER) && score >= beta) || ((flag & TT_UPPER) && score <= alpha)) {
        TTPut(board->zobrist, depth, score, flag, 0, ss->ply, 0, ttPv);
        return score;
      }

      // for pv node searches we adjust our a/b search accordingly
      if (isPV) {
        if (flag & TT_LOWER) {
          bestScore = score;
          alpha     = max(alpha, score);
        } else
          maxScore = score;
      }
    }
  }

  // IIR by Ed Schroder
  // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
  if (depth >= 4 && !hashMove && !ss->skip) depth--;

  // pull previous static eval from tt - this is depth independent
  int eval;
  if (!ss->skip) {
    eval = ss->staticEval = board->checkers ? UNKNOWN : (tt ? tt->eval : Evaluate(board, thread));
  } else {
    // after se, just used already determined eval
    eval = ss->staticEval;
  }

  if (!tt && !ss->skip && eval != UNKNOWN)
    TTPut(board->zobrist, INT8_MIN, UNKNOWN, TT_UNKNOWN, NULL_MOVE, ss->ply, eval, ttPv);

  // getting better if eval has gone up
  int improving = 0;
  if (!board->checkers && ss->ply >= 2) {
    if (ss->ply >= 4 && (ss - 2)->staticEval == UNKNOWN) {
      improving = ss->staticEval > (ss - 4)->staticEval || (ss - 4)->staticEval == UNKNOWN;
    } else {
      improving = ss->staticEval > (ss - 2)->staticEval || (ss - 2)->staticEval == UNKNOWN;
    }
  }

  // reset moves to moves related to 1 additional ply
  (ss + 1)->skip       = NULL_MOVE;
  (ss + 1)->killers[0] = NULL_MOVE;
  (ss + 1)->killers[1] = NULL_MOVE;
  ss->de               = (ss - 1)->de;

  int inCheck = !!board->checkers;

  Threat oppThreat;
  Threats(&oppThreat, board, board->xstm);

  if (!isPV && !board->checkers) {
    // Our TT might have a more accurate evaluation score, use this
    if (tt && ttScore != UNKNOWN && (tt->flags & (ttScore > eval ? TT_LOWER : TT_UPPER))) eval = ttScore;

    // Reverse Futility Pruning
    // i.e. the static eval is so far above beta we prune
    if (depth <= 9 && !ss->skip && eval - 75 * depth + 100 * (improving && !oppThreat.pcs) >= beta && eval >= beta &&
        eval < WINNING_ENDGAME)
      return eval;

    // Razoring
    if (depth <= 5 && eval + 200 * depth <= alpha) {
      score = Quiesce(alpha, beta, thread, ss);
      if (score <= alpha) return score;
    }

    // Null move pruning
    // i.e. Our position is so good we can give our opponnent a free move and
    // they still can't catch up (this is usually countered by captures or mate threats)
    if (depth >= 3 && (ss - 1)->move != NULL_MOVE && !ss->skip && eval >= beta &&
        // weiss conditional
        HasNonPawn(board) > (depth > 12)) {
      int R = 4 + depth / 6 + min((eval - beta) / 256, 3) + !oppThreat.pcs;
      R     = min(depth, R); // don't go too low

      ss->move = NULL_MOVE;
      ss->ch   = &thread->ch[WHITE_PAWN][A1];
      MakeNullMove(board);

      score = -Negamax(-beta, -beta + 1, depth - R, !cutnode, thread, &childPv, ss + 1);

      UndoNullMove(board);

      if (score >= beta) return score < WINNING_ENDGAME ? score : beta;
    }

    // Prob cut
    // If a relatively deep search from our TT doesn't say this node is
    // less than beta + margin, then we run a shallow search to look
    Threat ownThreat;
    Threats(&ownThreat, board, board->stm);
    int probBeta = beta + 110 - 30 * improving;
    if (depth > 4 && abs(beta) < TB_WIN_BOUND && ownThreat.pcs &&
        !(tt && tt->depth >= depth - 3 && ttScore < probBeta)) {
      InitTacticalMoves(&mp, thread, 0);
      while ((move = NextMove(&mp, board, 1))) {
        if (ss->skip == move) continue;
        if (!IsLegal(move, board)) continue;

        ss->move = move;
        ss->ch   = &thread->ch[Moving(move)][To(move)];
        MakeMove(move, board);

        // qsearch to quickly check
        score = -Quiesce(-probBeta, -probBeta + 1, thread, ss + 1);

        // if it's still above our cutoff, revalidate
        if (score >= probBeta) score = -Negamax(-probBeta, -probBeta + 1, depth - 4, !cutnode, thread, pv, ss + 1);

        UndoMove(move, board);

        if (score >= probBeta) return score;
      }
    }
  }

  Move quiets[64];
  Move tacticals[64];

  int legalMoves = 0, playedMoves = 0, numQuiets = 0, numTacticals = 0, skipQuiets = 0, singularExtension = 0;
  InitAllMoves(&mp, hashMove, thread, ss, oppThreat.sqs);

  while ((move = NextMove(&mp, board, skipQuiets))) {
    uint64_t startingNodeCount = thread->nodes;

    if (isRoot && MoveSearchedByMultiPV(thread, move)) continue;
    if (isRoot && !MoveSearchable(move)) continue;

    // don't search this during singular
    if (ss->skip == move) continue;

    if (!IsLegal(move, board)) continue;

    legalMoves++;

    int extension       = 0;
    int tactical        = IsTactical(move);
    int killerOrCounter = move == mp.killer1 || move == mp.killer2 || move == mp.counter;
    int history         = !tactical ? GetQuietHistory(ss, thread, move, board->stm, oppThreat.sqs) :
                                      GetTacticalHistory(thread, board, move);

    if (bestScore > -MATE_BOUND) {
      if (!isRoot && legalMoves >= LMP[improving][depth]) skipQuiets = 1;

      if (!tactical) {
        if (depth < 9 && eval + 100 + 50 * depth + history / 512 <= alpha) skipQuiets = 1;

        if (!SEE(board, move, STATIC_PRUNE[0][depth])) continue;
      } else {
        if (mp.phase > PLAY_GOOD_TACTICAL && !SEE(board, move, STATIC_PRUNE[1][depth])) continue;
      }
    }

    playedMoves++;

    if (isRoot && !thread->idx && GetTimeMS() - limits.start > 2500)
      printf("info depth %d currmove %s currmovenumber %d\n",
             thread->depth,
             MoveToStr(move, board),
             playedMoves + thread->multiPV);

    if (!tactical)
      quiets[numQuiets++] = move;
    else
      tacticals[numTacticals++] = move;

    // singular extension
    // if one move is better than all the rest, then we consider this singular
    // and look at it more (extend). Singular is determined by checking all other
    // moves at a shallow depth on a nullwindow that is somewhere below the tt evaluation
    // implemented using "skip move" recursion like in SF (allows for reductions when doing singular search)
    if (!isRoot && depth >= 7 && tt && move == hashMove && tt->depth >= depth - 3 && (tt->flags & TT_LOWER) &&
        abs(ttScore) < WINNING_ENDGAME) {
      int sBeta  = max(ttScore - 3 * depth / 2, -CHECKMATE);
      int sDepth = depth / 2 - 1;

      ss->skip = move;
      score    = Negamax(sBeta - 1, sBeta, sDepth, cutnode, thread, pv, ss);
      ss->skip = NULL_MOVE;

      // no score failed above sBeta, so this is singular
      if (score < sBeta) {
        singularExtension = 1;

        if (!isPV && score < sBeta - 35 && ss->de <= 6) {
          extension = 2;
          ss->de    = (ss - 1)->de + 1;
        } else {
          extension = 1;
        }
      } else if (sBeta >= beta)
        return sBeta;
      else if (ttScore >= beta)
        extension = -1;
    }

    // history extension - if the tt move has a really good history score, extend.
    // thank you to Connor, author of Seer for this idea
    else if (!isRoot && depth >= 7 && tt && move == hashMove && history >= 98304)
      extension = 1;

    // re-capture extension - looks for a follow up capture on the same square
    // as the previous capture
    else if (!isRoot && isPV && IsRecapture(ss, move))
      extension = 1;

    ss->move = move;
    ss->ch   = &thread->ch[Moving(move)][To(move)];
    MakeMove(move, board);

    // apply extensions
    int newDepth = depth + extension;

    int doFullSearch = 0;

    // Late move reductions
    if (depth > 2 && legalMoves > 1 && !(ttPv && IsCap(move))) {
      int R = LMR[min(depth, 63)][min(legalMoves, 63)];

      // increase reduction on non-pv
      if (!ttPv) R++;

      // increase reduction if our eval is declining
      if (!improving) R++;

      // reduce these special quiets less
      if (killerOrCounter) R -= 2;

      // less likely a non-capture is best
      if (IsCap(hashMove)) R++;

      // move GAVE check
      if (board->checkers) R--;

      // Ethereal king evasions
      if (inCheck && PieceType(Moving(move)) == KING) R++;

      // Reduce more on expected cut nodes
      // idea from komodo/sf, explained by Don Daily here
      // https://talkchess.com/forum3/viewtopic.php?f=7&t=47577&start=10#p519741
      // and https://www.chessprogramming.org/Node_Types
      if (cutnode) R += 1 + !tactical;

      // adjust reduction based on historical score
      R -= history / 20480;

      // prevent dropping into QS, extending, or reducing all extensions
      R = min(depth - 1, max(R, !singularExtension));

      score = -Negamax(-alpha - 1, -alpha, newDepth - R, 1, thread, &childPv, ss + 1);

      doFullSearch = score > alpha && R > 1;
    } else {
      doFullSearch = !isPV || playedMoves > 1;
    }

    if (doFullSearch) score = -Negamax(-alpha - 1, -alpha, newDepth - 1, !cutnode, thread, &childPv, ss + 1);

    if (isPV && (playedMoves == 1 || (score > alpha && (isRoot || score < beta))))
      score = -Negamax(-beta, -alpha, newDepth - 1, 0, thread, &childPv, ss + 1);

    UndoMove(move, board);

    if (load_rlx(threadPool.stop)) return 0;

    if (isRoot) thread->nodeCounts[FromTo(move)] += thread->nodes - startingNodeCount;

    if (score > bestScore) {
      bestScore = score;
      bestMove  = move;

      if ((isPV && score > alpha) || (isRoot && playedMoves == 1)) {
        pv->count    = childPv.count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv.moves, childPv.count * sizeof(Move));
      }

      if (score > alpha) alpha = score;

      // we're failing high
      if (alpha >= beta) {
        UpdateHistories(board,
                        ss,
                        thread,
                        move,
                        depth + (bestScore > beta + 100),
                        board->stm,
                        quiets,
                        numQuiets,
                        tacticals,
                        numTacticals,
                        oppThreat.sqs);
        break;
      }
    }
  }

  // Checkmate detection using movecount
  if (!legalMoves) return ss->skip ? alpha : board->checkers ? -CHECKMATE + ss->ply : 0;

  // don't let our score inflate too high (tb)
  bestScore = min(bestScore, maxScore);

  // prevent saving when in singular search
  if (!ss->skip && !(isRoot && thread->multiPV > 0)) {
    // save to the TT
    // TT_LOWER = we failed high, TT_UPPER = we didnt raise alpha, TT_EXACT = in
    int TTFlag       = bestScore >= beta ? TT_LOWER : bestScore <= origAlpha ? TT_UPPER : TT_EXACT;
    Move moveToStore = tt && TTFlag == TT_UPPER && (tt->flags & TT_LOWER) ? hashMove : bestMove;
    TTPut(board->zobrist, depth, bestScore, TTFlag, moveToStore, ss->ply, ss->staticEval, ttPv);
  }

  return bestScore;
}

int Quiesce(int alpha, int beta, ThreadData* thread, SearchStack* ss) {
  Board* board = &thread->board;

  int isPV = beta - alpha != 1;
  int ttPv = 0;

  thread->nodes++;

  // draw check
  if (IsDraw(board, ss->ply)) return 0;

  // prevent overflows
  if (ss->ply >= MAX_SEARCH_PLY - 1) return board->checkers ? 0 : Evaluate(board, thread);

  // check the transposition table for previous info
  int ttScore = UNKNOWN;
  TTEntry* tt = TTProbe(board->zobrist);
  ttPv        = isPV || (tt && (tt->flags & TT_PV));
  // TT score pruning - no depth check required since everything in QS is depth 0
  if (!isPV && tt) {
    ttScore = TTScore(tt, ss->ply);

    if (ttScore != UNKNOWN && ((tt->flags & TT_EXACT) || ((tt->flags & TT_LOWER) && ttScore >= beta) ||
                               ((tt->flags & TT_UPPER) && ttScore <= alpha)))
      return ttScore;
  }

  Move bestMove = NULL_MOVE;
  int bestScore = -CHECKMATE + ss->ply;

  // pull cached eval if it exists
  int eval = ss->staticEval = board->checkers ? UNKNOWN : (tt ? tt->eval : Evaluate(board, thread));
  if (!tt && eval != UNKNOWN) TTPut(board->zobrist, INT8_MIN, UNKNOWN, TT_UNKNOWN, NULL_MOVE, ss->ply, eval, ttPv);

  // can we use an improved evaluation from the tt?
  if (tt && ttScore != UNKNOWN)
    if (tt->flags & (ttScore > eval ? TT_LOWER : TT_UPPER)) eval = ttScore;

  // stand pat
  if (!board->checkers) {
    if (eval >= beta) return eval;

    if (eval > alpha) alpha = eval;

    bestScore = eval;
  }

  Move move;
  MovePicker mp;

  InitTacticalMoves(&mp, thread, eval <= alpha - DELTA_CUTOFF);

  while ((move = NextMove(&mp, board, 1))) {
    if (!IsLegal(move, board)) continue;

    if (mp.phase > PLAY_GOOD_TACTICAL) break;

    ss->move = move;
    ss->ch   = &thread->ch[Moving(move)][To(move)];
    MakeMove(move, board);

    int score = -Quiesce(-beta, -alpha, thread, ss + 1);

    UndoMove(move, board);

    if (score > bestScore) {
      bestScore = score;
      bestMove  = move;

      if (score > alpha) alpha = score;

      // failed high
      if (alpha >= beta) break;
    }
  }

  int TTFlag = bestScore >= beta ? TT_LOWER : TT_UPPER;
  TTPut(board->zobrist, 0, bestScore, TTFlag, bestMove, ss->ply, ss->staticEval, ttPv);

  return bestScore;
}

inline void PrintInfo(PV* pv, int score, ThreadData* thread, int alpha, int beta, int multiPV, Board* board) {
  int depth       = thread->depth;
  int seldepth    = thread->seldepth;
  uint64_t nodes  = NodesSearched();
  uint64_t tbhits = TBHits();
  uint64_t time   = GetTimeMS() - limits.start;
  uint64_t nps    = 1000 * nodes / max(time, 1);
  int hashfull    = TTFull();
  int bounded     = max(alpha, min(beta, score));

  int printable = bounded > MATE_BOUND  ? (CHECKMATE - bounded + 1) / 2 :
                  bounded < -MATE_BOUND ? -(CHECKMATE + bounded) / 2 :
                                          bounded;
  char* type    = abs(bounded) > MATE_BOUND ? "mate" : "cp";
  char* bound   = bounded >= beta ? " lowerbound " : bounded <= alpha ? " upperbound " : " ";

  printf("info depth %d seldepth %d multipv %d score %s %d%snodes %" PRId64 " nps %" PRId64
         " hashfull %d tbhits %" PRId64 " time %" PRId64 " pv ",
         depth,
         seldepth,
         multiPV,
         type,
         printable,
         bound,
         nodes,
         nps,
         hashfull,
         tbhits,
         time);

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

int MoveSearchable(Move move) {
  if (!limits.searchMoves) return 1;

  for (int i = 0; i < limits.searchable.count; i++)
    if (move == limits.searchable.moves[i]) return 1;

  return 0;
}

void SearchClearThread(ThreadData* thread) {
  thread->results.depth = 0;

  memset(&thread->counters, 0, sizeof(thread->counters));
  memset(&thread->hh, 0, sizeof(thread->hh));
  memset(&thread->ch, 0, sizeof(thread->ch));
  memset(&thread->th, 0, sizeof(thread->th));
  memset(&thread->scores, 0, sizeof(thread->scores));
  memset(&thread->bestMoves, 0, sizeof(thread->bestMoves));
  memset(&thread->pvs, 0, sizeof(thread->counters));
}

void SearchClear() {
  for (int i = 0; i < threadPool.count; i++) ThreadWake(threadPool.threads[i], THREAD_SEARCH_CLEAR);
  for (int i = 0; i < threadPool.count; i++) ThreadWaitUntilSleep(threadPool.threads[i]);
}
