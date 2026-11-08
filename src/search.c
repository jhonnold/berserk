// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "nn/accumulator.h"
#include "options.h"
#include "pyrrhic/tbprobe.h"
#include "see.h"
#include "tb.h"
#include "thread.h"
#include "transposition.h"
#include "tune.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

// arrays to store these pruning cutoffs at specific depths
SearchParams Limits;

static int LMR[MAX_SEARCH_PLY][64];
static int LMP[2][MAX_SEARCH_PLY];
static int STATIC_PRUNE[2][MAX_SEARCH_PLY];

static Score Negamax(Score alpha, Score beta, Depth depth, Bool cutnode, ThreadData* thread, PV* pv, SearchStack* ss);
static Score Quiesce(Score alpha, Score beta, Depth depth, ThreadData* thread, SearchStack* ss);
static void SortRootMoves(ThreadData* thread, int offset);
static Bool ValidRootMove(ThreadData* thread, Move move);

void ResetLimits(void) {
  memset(&Limits, 0, sizeof(Limits));

  Limits.start   = GetTimeMS();
  Limits.max     = INT_MAX;
  Limits.depth   = MAX_SEARCH_PLY - 1;
  Limits.multiPV = 1;
  Limits.hitrate = 1000;
}

void InitPruningAndReductionTables(void) {
  for (Depth depth = 1; depth < MAX_SEARCH_PLY; depth++)
    for (int moves = 1; moves < 64; moves++)
      LMR[depth][moves] = (log(depth) * log(moves) / LMR_DIV + LMR_BASE) * 1024; // in 1/1024 plies

  LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;

  for (Depth depth = 0; depth < MAX_SEARCH_PLY; depth++) {
    // LMP has both a improving (more strict) and non-improving evalution
    // parameter for lmp. If the evaluation is getting better we want to check
    // more
    LMP[0][depth] = LMP_BASE + LMP_SCALE * depth * depth;
    LMP[1][depth] = LMP_IMP_BASE + LMP_IMP_SCALE * depth * depth;

    STATIC_PRUNE[0][depth] = -SEE_QUIET * depth * depth; // quiet move cutoff
    STATIC_PRUNE[1][depth] = -SEE_NOISY * depth;         // capture cutoff
  }
}

INLINE Bool CheckLimits(ThreadData* thread) {
  if (--thread->calls > 0)
    return 0;
  thread->calls = Limits.hitrate;

  if (Threads.ponder)
    return 0;

  long elapsed = GetTimeMS() - Limits.start;
  if ((Limits.timeset && elapsed >= Limits.max) || //
      (Limits.nodes && NodesSearched() >= Limits.nodes)) {
    Threads.stop = 1;
    return 1;
  }

  return 0;
}

// Whole plies from a reduction kept in 1/1024 plies, rounding down (also for negatives)
INLINE int ReductionPlies(int r) {
  return r >> 10;
}

INLINE Score AdjustEvalOnFMR(Board* board, Score eval) {
  return (EVAL_FMR_DIV - board->fmr) * eval / EVAL_FMR_DIV;
}

INLINE int ThreadValue(ThreadData* thread, const Score worstScore) {
  return (thread->rootMoves[0].score - worstScore) * thread->depth;
}

// Static eval of a node not in check. Sets *rawEval to the uncorrected eval (what the TT keeps) and
// ss->staticEval to the corrected one, and returns the eval search prunes with: scaled toward 0 by the
// fifty move counter and, on a TT hit, replaced by a TT score whose bound says it is tighter. Without a
// TT hit the fresh eval is stored in the TT entry.
INLINE Score StaticEval(ThreadData* thread,
                        SearchStack* ss,
                        TTEntry* tt,
                        Bool ttHit,
                        Score ttEval,
                        Score ttScore,
                        HashBound ttBound,
                        Bool ttPv,
                        Score* rawEval) {
  Board* board = &thread->board;

  *rawEval       = ttHit && ttEval != EVAL_UNKNOWN ? ttEval : Evaluate(board, thread);
  ss->staticEval = ClampEval(*rawEval + GetCorrectionScore(board, ss));

  Score eval = AdjustEvalOnFMR(board, ss->staticEval);

  if (!ttHit)
    TTPut(tt, board->zobrist, -1, UNKNOWN, BOUND_UNKNOWN, NULL_MOVE, ss->ply, *rawEval, ttPv);
  else if (ttScore != UNKNOWN && (ttBound & (ttScore > eval ? BOUND_LOWER : BOUND_UPPER)))
    eval = ttScore;

  return eval;
}

// Record move on the search stack, count the node and play it
INLINE void PlayMove(ThreadData* thread, SearchStack* ss, Move move) {
  Board* board = &thread->board;

  TTPrefetch(KeyAfter(board, move));
  ss->move = move;
  ss->ch   = &thread->ch[IsCap(move)][Moving(move)][To(move)];
  ss->cont = &thread->contCorrection[Moving(move)][To(move)];
  IncRlx(thread->nodes);
  MakeMove(move, board);
}

// As PlayMove, with the null move's history pointing at the unused [0][WHITE_PAWN][A1] slots
INLINE void PlayNullMove(ThreadData* thread, SearchStack* ss) {
  Board* board = &thread->board;

  TTPrefetch(KeyAfter(board, NULL_MOVE));
  ss->move = NULL_MOVE;
  ss->ch   = &thread->ch[0][WHITE_PAWN][A1];
  ss->cont = &thread->contCorrection[WHITE_PAWN][A1];
  IncRlx(thread->nodes);
  MakeNullMove(board);
}

INLINE void InitRootMove(RootMove* rm, Move move) {
  rm->move = move;

  rm->previousScore = rm->score = rm->avgScore = -CHECKMATE;

  rm->pv.moves[0] = move;
  rm->pv.count    = 1;

  rm->nodes = 0;
}

static void SetupMainThread(Board* board) {
  ThreadData* mainThread = Threads.threads[0];
  mainThread->calls      = Limits.hitrate;
  mainThread->nodes      = 0;
  mainThread->tbhits     = 0;
  mainThread->nmpMinPly  = 0;

  memcpy(&mainThread->board, board, offsetof(Board, accumulators));

  if (Limits.searchMoves) {
    for (int i = 0; i < Limits.searchable.count; i++)
      InitRootMove(&mainThread->rootMoves[i], Limits.searchable.moves[i]);

    mainThread->numRootMoves = Limits.searchable.count;
  } else {
    SimpleMoveList ml[1];
    TBRootMoves(ml, board);

    if (!ml->count)
      RootMoves(ml, board);

    for (int i = 0; i < ml->count; i++)
      InitRootMove(&mainThread->rootMoves[i], ml->moves[i]);

    mainThread->numRootMoves = ml->count;
  }
}

static void SetupOtherThreads(Board* board) {
  ThreadData* mainThread = Threads.threads[0];

  for (int i = 1; i < Threads.count; i++) {
    ThreadData* thread = Threads.threads[i];
    thread->calls      = Limits.hitrate;
    thread->nodes      = 0;
    thread->tbhits     = 0;
    thread->nmpMinPly  = 0;

    for (int j = 0; j < mainThread->numRootMoves; j++)
      InitRootMove(&thread->rootMoves[j], mainThread->rootMoves[j].move);

    thread->numRootMoves = mainThread->numRootMoves;

    memcpy(&thread->board, board, offsetof(Board, accumulators));
  }
}

void StartSearch(Board* board, uint8_t ponder) {
  if (Threads.searching)
    ThreadWaitUntilSleep(Threads.threads[0]);

  Threads.stopOnPonderHit = 0;
  Threads.stop            = 0;
  Threads.ponder          = ponder;

  // Setup Threads
  SetupMainThread(board);
  SetupOtherThreads(board);

  Threads.searching = 1;
  ThreadWake(Threads.threads[0], THREAD_SEARCH);
}

void MainSearch(void) {
  ThreadData* mainThread = Threads.threads[0];
  Board* board           = &mainThread->board;

  TTUpdate();

  for (int i = 1; i < Threads.count; i++)
    ThreadWake(Threads.threads[i], THREAD_SEARCH);
  Search(mainThread);

  pthread_mutex_lock(&Threads.lock);
  if (!Threads.stop && (Threads.ponder || Limits.infinite)) {
    Threads.sleeping = 1;
    pthread_mutex_unlock(&Threads.lock);
    ThreadWait(mainThread, &Threads.stop);
  } else {
    pthread_mutex_unlock(&Threads.lock);
  }

  Threads.stop = 1;

  for (int i = 1; i < Threads.count; i++)
    ThreadWaitUntilSleep(Threads.threads[i]);

  int voteMap[64 * 64];
  Score worstScore = UNKNOWN;

  for (int i = 0; i < Threads.count; i++) {
    ThreadData* thread                         = Threads.threads[i];
    worstScore                                 = Min(worstScore, thread->rootMoves[0].score);
    voteMap[FromTo(thread->rootMoves[0].move)] = 0;
  }

  for (int i = 0; i < Threads.count; i++) {
    ThreadData* thread = Threads.threads[i];
    voteMap[FromTo(thread->rootMoves[0].move)] += ThreadValue(thread, worstScore);
  }

  ThreadData* bestThread = mainThread;
  Score bestScore        = bestThread->rootMoves[0].score;
  int bestVoteScore      = voteMap[FromTo(bestThread->rootMoves[0].move)];

  for (int i = 1; i < Threads.count; i++) {
    ThreadData* curr  = Threads.threads[i];
    Score currScore   = curr->rootMoves[0].score;
    int currVoteScore = voteMap[FromTo(curr->rootMoves[0].move)];

    // Always choose the fastest mate or longest avoidance of mate
    if (abs(bestScore) >= TB_WIN_BOUND) {
      if (currScore > bestScore)
        bestThread = curr, bestScore = currScore, bestVoteScore = currVoteScore;
    }

    // If this move avoids mate, choose it if:
    //   It's strictly better in terms of voting
    //   or if it's equal in voting and it's individual score is stronger
    //   when considering if this move was a FH or FL in the window
    //   This logic originates verbatim in SF ThreadPool::get_best_thread()
    else if (currScore > -TB_WIN_BOUND &&
             (currVoteScore > bestVoteScore ||
              (currVoteScore == bestVoteScore &&
               (ThreadValue(curr, worstScore) * (curr->rootMoves[0].pv.count > 2)) >
                 (ThreadValue(bestThread, worstScore) * (bestThread->rootMoves[0].pv.count > 2)))))
      bestThread = curr, bestScore = currScore, bestVoteScore = currVoteScore;
  }

  if (bestThread != mainThread || MINIMAL)
    PrintUCI(bestThread, bestThread->completedDepth, -CHECKMATE, CHECKMATE, board);

  if (bestThread != mainThread) {
    // / If a new thread is best, make it the main thread
    mainThread->idx                  = bestThread->idx;
    Threads.threads[mainThread->idx] = mainThread;

    bestThread->idx    = 0;
    Threads.threads[0] = bestThread;
  }

  bestThread->previousScore = bestThread->rootMoves[0].score;

  Move bestMove   = bestThread->rootMoves[0].move;
  Move ponderMove = NULL_MOVE;
  if (bestThread->rootMoves[0].pv.count > 1)
    ponderMove = bestThread->rootMoves[0].pv.moves[1];
  else {
    // Pull ponder move from the TT if PV doesn't have one.
    MakeMove(bestMove, board);
    Bool ttHit = 0, ttPv = 0;
    Score ttScore, ttEval;
    Depth ttDepth;
    HashBound ttBound;
    TTProbe(board->zobrist, 0, &ttHit, &ponderMove, &ttScore, &ttEval, &ttDepth, &ttBound, &ttPv);

    // Set to NULL if not legal
    if (!(ttHit && IsPseudoLegal(ponderMove, board) && IsLegal(ponderMove, board)))
      ponderMove = NULL_MOVE;

    UndoMove(bestMove, board);
  }

  PrintBestMove(bestMove, ponderMove, board);
}

void Search(ThreadData* thread) {
  Board* board    = &thread->board;
  Bool mainThread = !thread->idx;

  thread->depth          = 0;
  thread->completedDepth = 0;
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);
  SetContempt(thread->contempt, board->stm);

  PV nullPv;
  Score scores[MAX_SEARCH_PLY];
  int searchStability   = 0;
  Move previousBestMove = NULL_MOVE;

  enum {
    searchOffset = 6
  };
  SearchStack searchStack[MAX_SEARCH_PLY + searchOffset];
  SearchStack* ss = searchStack + searchOffset;
  memset(searchStack, 0, (searchOffset + 1) * sizeof(SearchStack));
  for (size_t i = 0; i < MAX_SEARCH_PLY; i++)
    (ss + i)->ply = i, (ss + i)->reduction = 0;
  for (size_t i = 1; i <= searchOffset; i++) {
    (ss - i)->ch        = &thread->ch[0][WHITE_PAWN][A1];
    (ss - i)->cont      = &thread->contCorrection[WHITE_PAWN][A1];
    (ss - i)->reduction = 0;
  }

  while (++thread->depth < MAX_SEARCH_PLY) {
    if (Limits.depth && mainThread && thread->depth > Limits.depth)
      break;

    for (int i = 0; i < thread->numRootMoves; i++)
      thread->rootMoves[i].previousScore = thread->rootMoves[i].score;

    for (thread->multiPV = 0; thread->multiPV < Limits.multiPV; thread->multiPV++) {
      Score alpha       = -CHECKMATE;
      Score beta        = CHECKMATE;
      Score delta       = CHECKMATE;
      Depth searchDepth = thread->depth;
      Score score       = thread->rootMoves[thread->multiPV].avgScore;

      thread->seldepth = 0;

      // One at depth 5 or later, start search at a reduced window
      if (thread->depth >= 5) {
        delta = ASP_DELTA;
        alpha = Max(score - delta, -CHECKMATE);
        beta  = Min(score + delta, CHECKMATE);
      }

      while (1) {
        // Go checkmate hunting once our eval is at the edge of it's bounds (-2045, 2045)
        if (alpha < -2000)
          alpha = -CHECKMATE;

        if (beta > 2000)
          beta = CHECKMATE;

        // search!
        score = Negamax(alpha, beta, Max(1, searchDepth), 0, thread, &nullPv, ss);

        // the score cannot be trusted, leave the root moves as the last completed iteration
        if (LoadRlx(Threads.stop))
          break;

        SortRootMoves(thread, thread->multiPV);

        if (mainThread && !MINIMAL && (score <= alpha || score >= beta) && Limits.multiPV == 1 &&
            GetTimeMS() - Limits.start >= 2500)
          PrintUCI(thread, thread->depth, alpha, beta, board);

        if (score <= alpha) {
          // adjust beta downward when failing low
          beta  = (alpha + beta) / 2;
          alpha = Max(alpha - delta, -CHECKMATE);

          searchDepth = thread->depth;
          if (mainThread)
            Threads.stopOnPonderHit = 0;
        } else if (score >= beta) {
          beta = Min(beta + delta, CHECKMATE);

          if (abs(score) < TB_WIN_BOUND)
            searchDepth--;
        } else
          break;

        // delta x 1.25
        delta += ASP_GROWTH * delta / 64;
      }

      if (LoadRlx(Threads.stop))
        break;

      SortRootMoves(thread, 0);

      // Print if final multipv or time elapsed
      if (mainThread && !MINIMAL && (thread->multiPV + 1 == Limits.multiPV || GetTimeMS() - Limits.start >= 2500))
        PrintUCI(thread, thread->depth, -CHECKMATE, CHECKMATE, board);
    }

    if (LoadRlx(Threads.stop))
      break;

    thread->completedDepth = thread->depth;

    if (!mainThread)
      continue;

    Move bestMove   = thread->rootMoves[0].move;
    Score bestScore = thread->rootMoves[0].score;

    // Found mate?
    if (Limits.mate && CHECKMATE - abs(bestScore) <= 2 * abs(Limits.mate))
      break;

    if (Limits.softNodes && NodesSearched() >= Limits.softNodes)
      break;

    // Time Management stuff
    long elapsed = GetTimeMS() - Limits.start;

    // Soft TM checks
    if (Limits.timeset && thread->depth >= 5 && !Threads.stopOnPonderHit) {
      Bool sameBestMove      = bestMove == previousBestMove;                    // same move?
      searchStability        = sameBestMove ? Min(10, searchStability + 1) : 0; // increase how stable our best move is
      double stabilityFactor = TM_STABILITY_BASE - TM_STABILITY_SCALE * searchStability;

      Score searchScoreDiff = scores[thread->depth - 3] - bestScore;
      Score prevScoreDiff   = thread->previousScore - bestScore;

      // if we don't know the previous score, work only on the searchscore
      if (thread->previousScore == UNKNOWN)
        searchScoreDiff *= 2, prevScoreDiff = 0;

      double scoreChangeFactor = TM_SCORE_BASE +                                             //
                                 TM_SCORE_SEARCH * searchScoreDiff * (searchScoreDiff > 0) + //
                                 TM_SCORE_PREV * prevScoreDiff * (prevScoreDiff > 0);
      scoreChangeFactor = Max(TM_SCORE_MIN, Min(TM_SCORE_MAX, scoreChangeFactor));

      uint64_t bestMoveNodes = thread->rootMoves[0].nodes;
      double pctNodesNotBest = 1.0 - (double) bestMoveNodes / thread->nodes;
      double nodeCountFactor = Max(TM_NODES_MIN, pctNodesNotBest * TM_NODES_SCALE + TM_NODES_BASE);
      if (bestScore >= TB_WIN_BOUND)
        nodeCountFactor = 0.5;

      if (elapsed > Limits.alloc * stabilityFactor * scoreChangeFactor * nodeCountFactor) {
        if (Threads.ponder)
          Threads.stopOnPonderHit = 1;
        else
          break;
      }
    }

    previousBestMove      = bestMove;
    scores[thread->depth] = bestScore;
  }
}

static Score Negamax(Score alpha, Score beta, Depth depth, Bool cutnode, ThreadData* thread, PV* pv, SearchStack* ss) {
  Board* board = &thread->board;

  PV childPv;
  pv->count = 0;

  Bool isPV       = beta - alpha != 1; // pv node when doing a full window
  Bool isRoot     = !ss->ply;          //
  Score score     = -CHECKMATE;        // initially assume the worst case
  Score bestScore = -CHECKMATE;        //
  Score maxScore  = CHECKMATE;         // best possible
  Score origAlpha = alpha;             // remember first alpha for tt storage
  Bool inCheck    = !!board->checkers;
  Bool improving  = 0;
  Score eval      = ss->staticEval;
  Score rawEval   = eval;

  Move bestMove = NULL_MOVE;
  Move hashMove = NULL_MOVE;
  Move move     = NULL_MOVE;

  if (!isRoot && board->fmr >= 3 && alpha < 0 && HasCycle(board, ss->ply)) {
    alpha = 2 - (thread->nodes & 0x3);
    if (alpha >= beta)
      return alpha;
  }

  // drop into noisy moves only
  if (depth <= 0)
    return Quiesce(alpha, beta, 0, thread, ss);

  if (LoadRlx(Threads.stop) || (!thread->idx && CheckLimits(thread)))
    return 0;

  if (isPV && thread->seldepth < ss->ply + 1)
    thread->seldepth = ss->ply + 1;

  if (!isRoot) {
    // draw
    if (IsDraw(board, ss->ply))
      return 2 - (thread->nodes & 0x3);

    // Prevent overflows
    if (ss->ply >= MAX_SEARCH_PLY - 1)
      return inCheck ? 0 : Evaluate(board, thread);

    // Mate distance pruning
    alpha = Max(alpha, -CHECKMATE + ss->ply);
    beta  = Min(beta, CHECKMATE - ss->ply - 1);
    if (alpha >= beta)
      return alpha;
  }

  // check the transposition table for previous info
  // we ignore the tt on singular extension searches
  Bool ttHit        = 0;
  Score ttScore     = UNKNOWN;
  Score ttEval      = EVAL_UNKNOWN;
  Depth ttDepth     = DEPTH_OFFSET;
  HashBound ttBound = BOUND_UNKNOWN;
  Bool ttPv         = isPV;

  TTEntry* tt =
    ss->skip ? NULL : TTProbe(board->zobrist, ss->ply, &ttHit, &hashMove, &ttScore, &ttEval, &ttDepth, &ttBound, &ttPv);
  hashMove = isRoot ? thread->rootMoves[thread->multiPV].move : hashMove;

  // if the TT has a value that fits our position and has been searched to an
  // equal or greater depth, then we accept this score and prune
  if (!isPV && ttScore != UNKNOWN && ttDepth >= depth && (cutnode || ttScore <= alpha) &&
      (ttBound & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
    return ttScore;

  // tablebase - we do not do this at root
  if (!isRoot && !ss->skip) {
    unsigned tbResult = TBProbe(board);

    if (tbResult != TB_RESULT_FAILED) {
      IncRlx(thread->tbhits);

      score           = tbResult == TB_WIN ? TB_WIN_SCORE - ss->ply : tbResult == TB_LOSS ? -TB_WIN_SCORE + ss->ply : 0;
      HashBound bound = tbResult == TB_WIN ? BOUND_LOWER : tbResult == TB_LOSS ? BOUND_UPPER : BOUND_EXACT;

      // if the tablebase gives us what we want, then we accept it's score and
      // return
      if ((bound == BOUND_EXACT) || (bound == BOUND_LOWER ? score >= beta : score <= alpha)) {
        TTPut(tt, board->zobrist, depth, score, bound, NULL_MOVE, ss->ply, ttEval, ttPv);
        return score;
      }

      // for pv node searches we adjust our a/b search accordingly
      if (isPV) {
        if (bound == BOUND_LOWER) {
          bestScore = score;
          alpha     = Max(alpha, score);
        } else
          maxScore = score;
      }
    }
  }

  if (inCheck) {
    rawEval = eval = ss->staticEval = EVAL_UNKNOWN;
  } else {
    // a singular search (ss->skip) has no TT entry and keeps the eval of the node it re-searches
    if (ttHit || !ss->skip)
      eval = StaticEval(thread, ss, tt, ttHit, ttEval, ttScore, ttBound, ttPv, &rawEval);

    // Improving
    improving = ss->ply >= 2 && (ss->staticEval > (ss - 2)->staticEval || (ss - 2)->staticEval == EVAL_UNKNOWN);
  }

  // reset moves to moves related to 1 additional ply
  (ss + 1)->skip       = NULL_MOVE;
  (ss + 1)->killers[0] = NULL_MOVE;
  (ss + 1)->killers[1] = NULL_MOVE;
  ss->de               = (ss - 1)->de;

  // IIR by Ed Schroder
  // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
  if (!(ss->skip || inCheck)) {
    if ((isPV || cutnode) && depth >= IIR_DEPTH && !hashMove)
      depth--;
  }

  MovePicker mp;
  if (!isPV && !inCheck) {
    const Bool opponentHasEasyCapture = !!OpponentsEasyCaptures(board);
    const Bool opponentDeclining      = ss->staticEval + (ss - 1)->staticEval > 1;

    if ((ss - 1)->reduction >= 3 && !opponentDeclining)
      depth++;

    if ((ss - 1)->reduction >= 1 && depth >= 2 && ss->staticEval + (ss - 1)->staticEval > DECLINE_MARGIN)
      depth--;

    // Reverse Futility Pruning
    // i.e. the static eval is so far above beta we prune
    if (depth <= RFP_DEPTH && !ss->skip && eval < TB_WIN_BOUND && eval >= beta &&
        eval - RFP_MARGIN * depth + RFP_IMPROVING * (improving && !opponentHasEasyCapture) +
            RFP_DECLINING * opponentDeclining >=
          beta &&
        (!hashMove || GetHistory(ss, thread, hashMove) > RFP_HISTORY))
      return (eval + beta) / 2;

    // Razoring
    if (depth <= RAZOR_DEPTH && eval + RAZOR_MARGIN * depth <= alpha) {
      score = Quiesce(alpha, beta, 0, thread, ss);

      if (LoadRlx(Threads.stop))
        return 0;

      if (score <= alpha)
        return score;
    }

    // Null move pruning
    // i.e. Our position is so good we can give our opponnent a free move and
    // they still can't catch up (this is usually countered by captures or mate
    // threats)
    if (depth >= NMP_DEPTH && (ss - 1)->move != NULL_MOVE && !ss->skip && !opponentHasEasyCapture && eval >= beta &&
        HasNonPawn(board, board->stm) && (ss->ply >= thread->nmpMinPly || board->stm != thread->npmColor)) {
      Depth R = NMP_BASE + NMP_DEPTH_SCALE * depth / 1024 + Min(NMP_EVAL_SCALE * (eval - beta) / 1024, NMP_EVAL_MAX);

      PlayNullMove(thread, ss);

      score = -Negamax(-beta, -beta + 1, depth - R, !cutnode, thread, &childPv, ss + 1);

      UndoNullMove(board);

      if (LoadRlx(Threads.stop))
        return 0;

      if (score >= beta) {
        if (score >= TB_WIN_BOUND)
          score = beta;

        if (thread->nmpMinPly || (abs(beta) < TB_WIN_BOUND && depth < NMP_VERIFY_DEPTH))
          return score;

        thread->nmpMinPly = ss->ply + 3 * (depth - R) / 4;
        thread->npmColor  = board->stm;

        Score verify = Negamax(beta - 1, beta, depth - R, 0, thread, pv, ss);

        thread->nmpMinPly = 0;

        if (LoadRlx(Threads.stop))
          return 0;

        if (verify >= beta)
          return score;
      }
    }

    // Prob cut
    // If a relatively deep search from our TT doesn't say this node is
    // less than beta + margin, then we run a shallow search to look
    Score probBeta = beta + PROBCUT_MARGIN;
    if (depth >= PROBCUT_DEPTH && !ss->skip && abs(beta) < TB_WIN_BOUND &&
        !(ttHit && ttDepth >= depth - 3 && ttScore < probBeta)) {
      InitPCMovePicker(&mp, thread, probBeta > eval);
      while ((move = NextMove(&mp, board, 1))) {
        if (!IsLegal(move, board))
          continue;

        PlayMove(thread, ss, move);

        // qsearch to quickly check
        score = -Quiesce(-probBeta, -probBeta + 1, 0, thread, ss + 1);

        // if it's still above our cutoff, revalidate
        if (score >= probBeta)
          score = -Negamax(-probBeta, -probBeta + 1, depth - 4, !cutnode, thread, pv, ss + 1);

        UndoMove(move, board);

        if (LoadRlx(Threads.stop))
          return 0;

        if (score >= probBeta)
          return score;
      }
    }
  }

  int numQuiets = 0, numCaptures = 0;
  Move quiets[64], captures[32];

  int legalMoves = 0, playedMoves = 0, skipQuiets = 0;
  InitNormalMovePicker(&mp, hashMove, thread, ss);

  while ((move = NextMove(&mp, board, skipQuiets))) {
    if (ss->skip == move)
      continue;
    if (isRoot && !ValidRootMove(thread, move))
      continue;
    if (!isRoot && !IsLegal(move, board))
      continue;

    uint64_t startingNodeCount = thread->nodes;

    legalMoves++;

    Depth extension      = 0;
    Bool killerOrCounter = move == mp.killer1 || move == mp.killer2 || move == mp.counter;
    int history          = GetHistory(ss, thread, move);

    // reduction in 1/1024 plies
    int R = LMR[Min(depth, 63)][Min(legalMoves, 63)];
    R -= LMR_HISTORY * history / 65536;                           // adjust reduction based on historical score
    R += LMR_NOISY_HASH * (IsCap(hashMove) || IsPromo(hashMove)); // increase reduction if hash move is noisy

    if (bestScore > -TB_WIN_BOUND) {
      if (!isRoot && legalMoves >= LMP[improving][depth])
        skipQuiets = 1;

      if (!IsCap(move) && PromoPT(move) != QUEEN) {
        Depth lmrDepth = Max(1, depth - ReductionPlies(R));

        if (!killerOrCounter && lmrDepth < HP_DEPTH && history < -HP_MARGIN * (depth - 1)) {
          skipQuiets = 1;
          continue;
        }

        if (!inCheck && lmrDepth < FP_DEPTH && eval + FP_BASE + FP_SCALE * lmrDepth <= alpha)
          skipQuiets = 1;

        if (!SEE(board, move, STATIC_PRUNE[0][lmrDepth]))
          continue;
      } else {
        if (!SEE(board, move, STATIC_PRUNE[1][depth]))
          continue;
      }
    }

    playedMoves++;

    if (isRoot && !thread->idx && GetTimeMS() - Limits.start > 2500)
      PrintCurrMove(thread->depth, move, playedMoves + thread->multiPV, board);

    if (!IsCap(move) && numQuiets < 64)
      quiets[numQuiets++] = move;
    else if (IsCap(move) && numCaptures < 32)
      captures[numCaptures++] = move;

    // singular extension
    // if one move is better than all the rest, then we consider this singular
    // and look at it more (extend). Singular is determined by checking all
    // other moves at a shallow depth on a nullwindow that is somewhere below
    // the tt evaluation implemented using "skip move" recursion like in SF
    // (allows for reductions when doing singular search)
    if (!isRoot && ss->ply < thread->depth * 2) {
      // ttHit is implied for move == hashMove to ever be true
      if (depth >= SE_DEPTH && move == hashMove && ttDepth >= depth - 3 && (ttBound & BOUND_LOWER) &&
          abs(ttScore) < TB_WIN_BOUND) {
        Score sBeta  = Max(ttScore - SE_MARGIN * depth / 8, -CHECKMATE);
        Depth sDepth = (depth - 1) / 2;

        ss->skip = move;
        score    = Negamax(sBeta - 1, sBeta, sDepth, cutnode, thread, pv, ss);
        ss->skip = NULL_MOVE;

        if (LoadRlx(Threads.stop))
          return 0;

        // no score failed above sBeta, so this is singular
        if (score < sBeta) {
          if (!isPV && score < sBeta - SE_TRIPLE_MARGIN && ss->de <= SE_TRIPLE_LIMIT && !IsCap(move)) {
            extension = 3;
            ss->de    = (ss - 1)->de + 1;
          } else if (!isPV && score < sBeta - SE_DOUBLE_MARGIN && ss->de <= SE_DOUBLE_LIMIT) {
            extension = 2;
            ss->de    = (ss - 1)->de + 1;
          } else {
            extension = 1;
          }
        } else if (sBeta >= beta)
          return sBeta;
        else if (ttScore >= beta)
          extension = -2 + isPV;
        else if (cutnode)
          extension = -2;
        else if (ttScore <= alpha)
          extension = -1;
      }
    }

    PlayMove(thread, ss, move);

    // apply extensions
    Depth newDepth = depth + extension;

    // Late move reductions
    if (depth > 1 && legalMoves > 1 && !(isPV && IsCap(move))) {
      // increase reduction on non-pv
      if (!ttPv)
        R += LMR_NOT_PV;

      // increase reduction if our eval is declining
      if (!improving)
        R += LMR_NOT_IMPROVING;

      // reduce these special quiets less
      if (killerOrCounter)
        R -= LMR_KILLER;

      // move GAVE check
      if (board->checkers)
        R -= LMR_CHECK;

      // Reduce more on expected cut nodes
      // idea from komodo/sf, explained by Don Daily here
      // https://talkchess.com/forum3/viewtopic.php?f=7&t=47577&start=10#p519741
      // and https://www.chessprogramming.org/Node_Types
      if (cutnode)
        R += LMR_CUTNODE + LMR_CUTNODE_QUIET * !IsCap(move);

      if (ttDepth >= depth)
        R -= LMR_TT_DEPTH;

      // prevent dropping into QS, extending, or reducing all extensions
      R             = Min(newDepth, Max(ReductionPlies(R), 1));
      ss->reduction = R;

      Depth lmrDepth = newDepth - R;
      score          = -Negamax(-alpha - 1, -alpha, lmrDepth, 1, thread, &childPv, ss + 1);

      ss->reduction = 0;

      if (score > alpha && R > 1) {
        // Credit to Viz (and lonfom) for the following modification of the zws
        // re-search depth. They can be found in SF as doDeeperSearch + doShallowerSearch
        newDepth += (score > bestScore + DEEPER_MARGIN);
        newDepth -= (score < bestScore + newDepth);

        if (newDepth - 1 > lmrDepth)
          score = -Negamax(-alpha - 1, -alpha, newDepth - 1, !cutnode, thread, &childPv, ss + 1);

        int bonus = score <= alpha ? -HistoryBonus(newDepth - 1) : score >= beta ? HistoryBonus(newDepth - 1) : 0;
        UpdateCH(ss, move, bonus);
      }
    } else if (!isPV || playedMoves > 1) {
      score = -Negamax(-alpha - 1, -alpha, newDepth - 1, !cutnode, thread, &childPv, ss + 1);
    }

    if (isPV && (playedMoves == 1 || (score > alpha && (isRoot || score < beta))))
      score = -Negamax(-beta, -alpha, newDepth - 1, 0, thread, &childPv, ss + 1);

    UndoMove(move, board);

    // if a stop occurred the score cannot be trusted, so return immediately without
    // updating the best move, pv, root moves, histories or tt
    if (LoadRlx(Threads.stop))
      return 0;

    if (isRoot) {
      RootMove* rm = thread->rootMoves;
      for (int i = 1; i < thread->numRootMoves; i++)
        if (thread->rootMoves[i].move == move) {
          rm = &thread->rootMoves[i];
          break;
        }

      rm->nodes += thread->nodes - startingNodeCount;
      rm->avgScore = rm->avgScore == -CHECKMATE ? score : (rm->avgScore + score) / 2;

      if (playedMoves == 1 || score > alpha) {
        rm->score    = score;
        rm->seldepth = thread->seldepth;

        rm->pv.count    = childPv.count + 1;
        rm->pv.moves[0] = move;
        memcpy(rm->pv.moves + 1, childPv.moves, childPv.count * sizeof(Move));
      } else {
        rm->score = -CHECKMATE;
      }
    }

    if (score > bestScore) {
      bestScore = score;

      if (isPV && !isRoot && score > alpha) {
        pv->count    = childPv.count + 1;
        pv->moves[0] = move;
        memcpy(pv->moves + 1, childPv.moves, childPv.count * sizeof(Move));
      }

      if (score > alpha) {
        bestMove = move;
        alpha    = score;

        if (alpha < beta && score > -TB_WIN_BOUND)
          depth -= (depth >= 2 && depth <= 11);
      }

      // we're failing high
      if (alpha >= beta) {
        UpdateHistories(ss,
                        thread,
                        move,
                        depth + (bestScore > beta + HIST_BETA_MARGIN),
                        quiets,
                        numQuiets,
                        captures,
                        numCaptures);
        break;
      }
    }
  }

  // Checkmate detection using movecount
  if (!legalMoves)
    return ss->skip ? alpha : inCheck ? -CHECKMATE + ss->ply : 0;

  // don't let our score inflate too high (tb)
  bestScore = Min(bestScore, maxScore);

  // prevent saving when in singular search
  HashBound bound = bestScore >= beta ? BOUND_LOWER : bestScore <= origAlpha ? BOUND_UPPER : BOUND_EXACT;
  if (!ss->skip && !(isRoot && thread->multiPV > 0))
    TTPut(tt, board->zobrist, depth, bestScore, bound, bestMove, ss->ply, rawEval, ttPv);

  if (!inCheck && !IsCap(bestMove) && (bound & (bestScore >= ss->staticEval ? BOUND_LOWER : BOUND_UPPER))) {
    UpdateCorrection(ss->staticEval, bestScore, depth, board);
    UpdateContCorrection(ss->staticEval, bestScore, depth, ss);
  }

  return bestScore;
}

static Score Quiesce(Score alpha, Score beta, Depth depth, ThreadData* thread, SearchStack* ss) {
  Board* board = &thread->board;

  Score score     = -CHECKMATE;
  Score futility  = -CHECKMATE;
  Score bestScore = -CHECKMATE + ss->ply;
  Bool isPV       = beta - alpha != 1;
  Bool inCheck    = !!board->checkers;
  Score eval      = ss->staticEval;
  Score rawEval   = eval;

  Move hashMove = NULL_MOVE;
  Move bestMove = NULL_MOVE;
  Move move     = NULL_MOVE;

  if (LoadRlx(Threads.stop) || (!thread->idx && CheckLimits(thread)))
    return 0;

  // draw check
  if (IsDraw(board, ss->ply))
    return 0;

  // prevent overflows
  if (ss->ply >= MAX_SEARCH_PLY - 1)
    return inCheck ? 0 : Evaluate(board, thread);

  // check the transposition table for previous info
  Bool ttHit        = 0;
  Score ttScore     = UNKNOWN;
  Score ttEval      = EVAL_UNKNOWN;
  Depth ttDepth     = DEPTH_OFFSET;
  HashBound ttBound = BOUND_UNKNOWN;
  Bool ttPv         = isPV;

  TTEntry* tt = TTProbe(board->zobrist, ss->ply, &ttHit, &hashMove, &ttScore, &ttEval, &ttDepth, &ttBound, &ttPv);

  // TT score pruning, ttHit implied with adjusted score
  if (!isPV && ttScore != UNKNOWN && (ttBound & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
    return ttScore;

  if (inCheck) {
    rawEval = eval = ss->staticEval = EVAL_UNKNOWN;
  } else {
    eval = StaticEval(thread, ss, tt, ttHit, ttEval, ttScore, ttBound, ttPv, &rawEval);

    // stand pat
    if (eval >= beta)
      return eval;

    if (eval > alpha)
      alpha = eval;

    bestScore = eval;

    futility = bestScore + QS_FUTILITY;
  }

  (ss + 1)->killers[0] = NULL_MOVE;
  (ss + 1)->killers[1] = NULL_MOVE;

  int numQuiets = 0, numCaptures = 0;
  Move quiets[64], captures[32];

  MovePicker mp;
  if (!inCheck)
    InitQSMovePicker(&mp, thread, depth >= -1);
  else
    InitQSEvasionsPicker(&mp, hashMove, thread, ss);

  int legalMoves = 0, skipQuiets = !inCheck;

  while ((move = NextMove(&mp, board, skipQuiets))) {
    if (!IsLegal(move, board))
      continue;

    legalMoves++;

    if (bestScore > -TB_WIN_BOUND) {
      if (!inCheck && mp.phase != QS_PLAY_QUIET_CHECKS && futility <= alpha && !SEE(board, move, 1)) {
        bestScore = Max(bestScore, futility);
        continue;
      }

      if (!SEE(board, move, 0))
        continue;
    }

    if (!IsCap(move) && numQuiets < 64)
      quiets[numQuiets++] = move;
    else if (IsCap(move) && numCaptures < 32)
      captures[numCaptures++] = move;

    PlayMove(thread, ss, move);

    score = -Quiesce(-beta, -alpha, depth - 1, thread, ss + 1);

    UndoMove(move, board);

    if (LoadRlx(Threads.stop))
      return 0;

    if (score > -TB_WIN_BOUND)
      skipQuiets = 1;

    if (score > bestScore) {
      bestScore = score;

      if (score > alpha) {
        bestMove = move;
        alpha    = score;
      }

      // failed high
      if (alpha >= beta) {
        UpdateHistories(ss, thread, move, 1, quiets, numQuiets, captures, numCaptures);
        break;
      }
    }
  }

  if (!legalMoves && inCheck)
    return -CHECKMATE + ss->ply;

  if (bestScore >= beta && abs(bestScore) < TB_WIN_BOUND)
    bestScore = (bestScore + beta) / 2;

  HashBound bound = bestScore >= beta ? BOUND_LOWER : BOUND_UPPER;
  TTPut(tt, board->zobrist, 0, bestScore, bound, bestMove, ss->ply, rawEval, ttPv);

  return bestScore;
}

static void SortRootMoves(ThreadData* thread, int offset) {
  for (int i = offset; i < thread->numRootMoves; i++) {
    int best = i;

    for (int j = i + 1; j < thread->numRootMoves; j++)
      if (thread->rootMoves[j].score > thread->rootMoves[best].score)
        best = j;

    if (best != i) {
      RootMove temp           = thread->rootMoves[best];
      thread->rootMoves[best] = thread->rootMoves[i];
      thread->rootMoves[i]    = temp;
    }
  }
}

static Bool ValidRootMove(ThreadData* thread, Move move) {
  for (int i = thread->multiPV; i < thread->numRootMoves; i++)
    if (move == thread->rootMoves[i].move)
      return 1;

  return 0;
}

void SearchClearThread(ThreadData* thread) {
  memset(&thread->counters, 0, sizeof(thread->counters));
  memset(&thread->hh, 0, sizeof(thread->hh));
  memset(&thread->ch, 0, sizeof(thread->ch));
  memset(&thread->caph, 0, sizeof(thread->caph));
  CorrectionClearPart(thread->idx, Threads.count);
  memset(&thread->contCorrection, 0, sizeof(thread->contCorrection));

  thread->board.accumulators = thread->accumulators;
  thread->previousScore      = UNKNOWN;
}
