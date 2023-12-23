// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "nn/accumulator.h"
#include "pyrrhic/tbprobe.h"
#include "see.h"
#include "tb.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

// arrays to store these pruning cutoffs at specific depths
int LMR[MAX_SEARCH_PLY][64];
int LMP[2][MAX_SEARCH_PLY];
int STATIC_PRUNE[2][MAX_SEARCH_PLY];

void InitPruningAndReductionTables() {
  for (int depth = 1; depth < MAX_SEARCH_PLY; depth++)
    for (int moves = 1; moves < 64; moves++)
      LMR[depth][moves] = log(depth) * log(moves) / 2.1872 + 0.2487;

  LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;

  for (int depth = 0; depth < MAX_SEARCH_PLY; depth++) {
    // LMP has both a improving (more strict) and non-improving evalution
    // parameter for lmp. If the evaluation is getting better we want to check
    // more
    LMP[0][depth] = 1.2973 + 0.3772 * depth * depth;
    LMP[1][depth] = 2.7002 + 0.9448 * depth * depth;

    STATIC_PRUNE[0][depth] = -14.9419 * depth * depth; // quiet move cutoff
    STATIC_PRUNE[1][depth] = -103.9379 * depth;        // capture cutoff
  }
}

INLINE int CheckLimits(ThreadData* thread) {
  if (--thread->calls > 0)
    return 0;
  thread->calls = Limits.hitrate;

  if (Threads.ponder)
    return 0;

  long elapsed = GetTimeMS() - Limits.start;
  return (Limits.timeset && elapsed >= Limits.max) || //
         (Limits.nodes && NodesSearched() >= Limits.nodes);
}

INLINE int AdjustEvalOnFMR(Board* board, int eval) {
  return (200 - board->fmr) * eval / 200;
}

INLINE int ThreadValue(ThreadData* thread, const int worstScore) {
  return (thread->rootMoves[0].score - worstScore) * thread->depth;
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

void MainSearch() {
  ThreadData* mainThread = Threads.threads[0];
  Board* board           = &mainThread->board;

  char startFen[128];
  BoardToFen(startFen, board);

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
  int worstScore = UNKNOWN;

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
  Score bestVoteScore    = voteMap[FromTo(bestThread->rootMoves[0].move)];

  for (int i = 1; i < Threads.count; i++) {
    ThreadData* curr    = Threads.threads[i];
    Score currScore     = curr->rootMoves[0].score;
    Score currVoteScore = voteMap[FromTo(curr->rootMoves[0].move)];

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

  if (bestThread != mainThread) {
    PrintUCI(bestThread, -CHECKMATE, CHECKMATE, board);

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
    // We reload the startfen because jmp aborts don't guarantee a rolled back board
    ParseFen(startFen, board);

    MakeMove(bestMove, board);
    int ttHit = 0, ttScore, ttEval, ttDepth, ttBound, ttPv = 0;
    TTProbe(board->zobrist, 0, &ttHit, &ponderMove, &ttScore, &ttEval, &ttDepth, &ttBound, &ttPv);

    // Set to NULL if not legal
    if (!(ttHit && IsPseudoLegal(ponderMove, board) && IsLegal(ponderMove, board)))
      ponderMove = NULL_MOVE;

    UndoMove(bestMove, board);
  }

  printf("bestmove %s", MoveToStr(bestMove, board));
  if (ponderMove)
    printf(" ponder %s", MoveToStr(ponderMove, board));
  printf("\n");
}

void Search(ThreadData* thread) {
  Board* board   = &thread->board;
  int mainThread = !thread->idx;

  thread->depth       = 0;
  board->accumulators = thread->accumulators; // exit jumps can cause this pointer to not be reset
  ResetAccumulator(board->accumulators, board, WHITE);
  ResetAccumulator(board->accumulators, board, BLACK);
  SetContempt(thread->contempt, board->stm);

  PV nullPv;
  int scores[MAX_SEARCH_PLY];
  int searchStability   = 0;
  Move previousBestMove = NULL_MOVE;

  const int searchOffset = 6;
  SearchStack searchStack[MAX_SEARCH_PLY + searchOffset];
  SearchStack* ss = searchStack + searchOffset;
  memset(searchStack, 0, (searchOffset + 1) * sizeof(SearchStack));
  for (size_t i = 0; i < MAX_SEARCH_PLY; i++)
    (ss + i)->ply = i;
  for (size_t i = 1; i <= searchOffset; i++)
    (ss - i)->ch = &thread->ch[0][WHITE_PAWN][A1];

  while (++thread->depth < MAX_SEARCH_PLY) {
#if defined(_WIN32) || defined(_WIN64)
    if (_setjmp(thread->exit, NULL)) {
#else
    if (setjmp(thread->exit)) {
#endif
      break;
    }

    if (Limits.depth && mainThread && thread->depth > Limits.depth)
      break;

    for (int i = 0; i < thread->numRootMoves; i++)
      thread->rootMoves[i].previousScore = thread->rootMoves[i].score;

    for (thread->multiPV = 0; thread->multiPV < Limits.multiPV; thread->multiPV++) {
      int alpha       = -CHECKMATE;
      int beta        = CHECKMATE;
      int delta       = CHECKMATE;
      int searchDepth = thread->depth;
      int score       = thread->rootMoves[thread->multiPV].previousScore;

      thread->seldepth = 0;

      // One at depth 5 or later, start search at a reduced window
      if (thread->depth >= 5) {
        delta = 9;
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

        SortRootMoves(thread, thread->multiPV);

        if (mainThread && (score <= alpha || score >= beta) && Limits.multiPV == 1 &&
            GetTimeMS() - Limits.start >= 2500)
          PrintUCI(thread, alpha, beta, board);

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
        delta += delta / 4;
      }

      SortRootMoves(thread, 0);

      // Print if final multipv or time elapsed
      if (mainThread && (thread->multiPV + 1 == Limits.multiPV || GetTimeMS() - Limits.start >= 2500))
        PrintUCI(thread, -CHECKMATE, CHECKMATE, board);
    }

    if (!mainThread)
      continue;

    Move bestMove = thread->rootMoves[0].move;
    int bestScore = thread->rootMoves[0].score;

    // Found mate?
    if (Limits.mate && CHECKMATE - abs(bestScore) <= 2 * abs(Limits.mate))
      break;

    // Time Management stuff
    long elapsed = GetTimeMS() - Limits.start;

    // Soft TM checks
    if (Limits.timeset && thread->depth >= 5 && !Threads.stopOnPonderHit) {
      int sameBestMove       = bestMove == previousBestMove;                    // same move?
      searchStability        = sameBestMove ? Min(10, searchStability + 1) : 0; // increase how stable our best move is
      double stabilityFactor = 1.3658 - 0.0482 * searchStability;

      Score searchScoreDiff = scores[thread->depth - 3] - bestScore;
      Score prevScoreDiff   = thread->previousScore - bestScore;

      // if we don't know the previous score, work only on the searchscore
      if (thread->previousScore == UNKNOWN)
        searchScoreDiff *= 2, prevScoreDiff = 0;

      double scoreChangeFactor = 0.0995 +                                           //
                                 0.0286 * searchScoreDiff * (searchScoreDiff > 0) + //
                                 0.0261 * prevScoreDiff * (prevScoreDiff > 0);
      scoreChangeFactor = Max(0.4843, Min(1.4498, scoreChangeFactor));

      uint64_t bestMoveNodes = thread->rootMoves[0].nodes;
      double pctNodesNotBest = 1.0 - (double) bestMoveNodes / thread->nodes;
      double nodeCountFactor = Max(0.5464, pctNodesNotBest * 2.1394 + 0.4393);
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
  int inCheck   = !!board->checkers;
  int improving = 0;
  int eval      = ss->staticEval;

  Move bestMove = NULL_MOVE;
  Move hashMove = NULL_MOVE;
  Move move     = NULL_MOVE;
  MovePicker mp;

  if (!isRoot && board->fmr >= 3 && alpha < 0 && HasCycle(board, ss->ply)) {
    alpha = 2 - (thread->nodes & 0x3);
    if (alpha >= beta)
      return alpha;
  }

  // drop into noisy moves only
  if (depth <= 0)
    return Quiesce(alpha, beta, 0, thread, ss);

  if (LoadRlx(Threads.stop) || (!thread->idx && CheckLimits(thread)))
    // hot exit
    longjmp(thread->exit, 1);

  IncRlx(thread->nodes);
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
  int ttHit   = 0;
  int ttScore = UNKNOWN;
  int ttEval  = EVAL_UNKNOWN;
  int ttDepth = DEPTH_OFFSET;
  int ttBound = BOUND_UNKNOWN;
  int ttPv    = isPV;

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

      score     = tbResult == TB_WIN ? TB_WIN_SCORE - ss->ply : tbResult == TB_LOSS ? -TB_WIN_SCORE + ss->ply : 0;
      int bound = tbResult == TB_WIN ? BOUND_LOWER : tbResult == TB_LOSS ? BOUND_UPPER : BOUND_EXACT;

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
    eval = ss->staticEval = EVAL_UNKNOWN;
  } else {
    if (ttHit) {
      eval = ss->staticEval = ttEval;
      if (ss->staticEval == EVAL_UNKNOWN)
        eval = ss->staticEval = Evaluate(board, thread);

      // correct eval on fmr
      eval = AdjustEvalOnFMR(board, eval);

      if (ttScore != UNKNOWN && (ttBound & (ttScore > eval ? BOUND_LOWER : BOUND_UPPER)))
        eval = ttScore;
    } else if (!ss->skip) {
      eval = ss->staticEval = Evaluate(board, thread);

      // correct eval on fmr
      eval = AdjustEvalOnFMR(board, eval);

      TTPut(tt, board->zobrist, -1, UNKNOWN, BOUND_UNKNOWN, NULL_MOVE, ss->ply, ss->staticEval, ttPv);
    }

    // Improving
    if (ss->ply >= 2) {
      if (ss->ply >= 4 && (ss - 2)->staticEval == EVAL_UNKNOWN) {
        improving = ss->staticEval > (ss - 4)->staticEval || (ss - 4)->staticEval == EVAL_UNKNOWN;
      } else {
        improving = ss->staticEval > (ss - 2)->staticEval || (ss - 2)->staticEval == EVAL_UNKNOWN;
      }
    }
  }

  // reset moves to moves related to 1 additional ply
  (ss + 1)->skip       = NULL_MOVE;
  (ss + 1)->killers[0] = NULL_MOVE;
  (ss + 1)->killers[1] = NULL_MOVE;
  ss->de               = (ss - 1)->de;

  // IIR by Ed Schroder
  // http://talkchess.com/forum3/viewtopic.php?f=7&t=74769&sid=64085e3396554f0fba414404445b3120
  if (!(ss->skip || inCheck)) {
    if ((isPV || cutnode) && depth >= 4 && !hashMove)
      depth--;
  }

  if (!isPV && !inCheck) {
    // Reverse Futility Pruning
    // i.e. the static eval is so far above beta we prune
    if (depth <= 8 && !ss->skip && eval < TB_WIN_BOUND && eval >= beta &&
        eval - 67 * depth + 112 * (improving && !board->easyCapture) >= beta &&
        (!hashMove || GetHistory(ss, thread, hashMove) > 12525))
      return (eval + beta) / 2;

    // Razoring
    if (depth <= 6 && eval + 252 * depth <= alpha) {
      score = Quiesce(alpha, beta, 0, thread, ss);
      if (score <= alpha)
        return score;
    }

    // Null move pruning
    // i.e. Our position is so good we can give our opponnent a free move and
    // they still can't catch up (this is usually countered by captures or mate
    // threats)
    if (depth >= 4 && (ss - 1)->move != NULL_MOVE && !ss->skip && eval >= beta && HasNonPawn(board, board->stm) &&
        (ss->ply >= thread->nmpMinPly || board->stm != thread->npmColor)) {
      int R = 5 + 221 * depth / 1024 + Min(5 * (eval - beta) / 1024, 4) + !board->easyCapture;
      R     = Min(depth, R); // don't go too low

      TTPrefetch(KeyAfter(board, NULL_MOVE));
      ss->move = NULL_MOVE;
      ss->ch   = &thread->ch[0][WHITE_PAWN][A1];
      MakeNullMove(board);

      score = -Negamax(-beta, -beta + 1, depth - R, !cutnode, thread, &childPv, ss + 1);

      UndoNullMove(board);

      if (score >= beta) {
        if (score >= TB_WIN_BOUND)
          score = beta;

        if (thread->nmpMinPly || (abs(beta) < TB_WIN_BOUND && depth < 14))
          return score;

        thread->nmpMinPly = ss->ply + 3 * (depth - R) / 4;
        thread->npmColor  = board->stm;

        Score verify = Negamax(beta - 1, beta, depth - R, 0, thread, pv, ss);

        thread->nmpMinPly = 0;

        if (verify >= beta)
          return score;
      }
    }

    // Prob cut
    // If a relatively deep search from our TT doesn't say this node is
    // less than beta + margin, then we run a shallow search to look
    int probBeta = beta + 197;
    if (depth >= 5 && !ss->skip && abs(beta) < TB_WIN_BOUND && !(ttHit && ttDepth >= depth - 3 && ttScore < probBeta)) {
      InitPCMovePicker(&mp, thread, probBeta > eval);
      while ((move = NextMove(&mp, board, 1))) {
        if (!IsLegal(move, board))
          continue;

        TTPrefetch(KeyAfter(board, move));
        ss->move = move;
        ss->ch   = &thread->ch[IsCap(move)][Moving(move)][To(move)];
        MakeMove(move, board);

        // qsearch to quickly check
        score = -Quiesce(-probBeta, -probBeta + 1, 0, thread, ss + 1);

        // if it's still above our cutoff, revalidate
        if (score >= probBeta)
          score = -Negamax(-probBeta, -probBeta + 1, depth - 4, !cutnode, thread, pv, ss + 1);

        UndoMove(move, board);

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

    int extension       = 0;
    int killerOrCounter = move == mp.killer1 || move == mp.killer2 || move == mp.counter;
    int history         = GetHistory(ss, thread, move);

    int R = LMR[Min(depth, 63)][Min(legalMoves, 63)];
    R -= history / 8192;                         // adjust reduction based on historical score
    R += (IsCap(hashMove) || IsPromo(hashMove)); // increase reduction if hash move is noisy

    if (bestScore > -TB_WIN_BOUND) {
      if (!isRoot && legalMoves >= LMP[improving][depth])
        skipQuiets = 1;

      if (!IsCap(move) && PromoPT(move) != QUEEN) {
        int lmrDepth = Max(1, depth - R);

        if (!killerOrCounter && lmrDepth < 7 && history < -2658 * (depth - 1)) {
          skipQuiets = 1;
          continue;
        }

        if (!inCheck && lmrDepth < 10 && eval + 87 + 46 * lmrDepth <= alpha)
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
      printf("info depth %d currmove %s currmovenumber %d\n",
             thread->depth,
             MoveToStr(move, board),
             playedMoves + thread->multiPV);

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
      if (depth >= 6 && move == hashMove && ttDepth >= depth - 3 && (ttBound & BOUND_LOWER) &&
          abs(ttScore) < TB_WIN_BOUND) {
        int sBeta  = Max(ttScore - 5 * depth / 8, -CHECKMATE);
        int sDepth = (depth - 1) / 2;

        ss->skip = move;
        score    = Negamax(sBeta - 1, sBeta, sDepth, cutnode, thread, pv, ss);
        ss->skip = NULL_MOVE;

        // no score failed above sBeta, so this is singular
        if (score < sBeta) {
          if (!isPV && score < sBeta - 17 && ss->de <= 6) {
            extension = 2;
            ss->de    = (ss - 1)->de + 1;
          } else {
            extension = 1;
          }
        } else if (sBeta >= beta)
          return sBeta;
        else if (ttScore >= beta)
          extension = -2 + isPV;
        else if (ttScore <= alpha)
          extension = -1;
      }
    }

    TTPrefetch(KeyAfter(board, move));
    ss->move = move;
    ss->ch   = &thread->ch[IsCap(move)][Moving(move)][To(move)];
    MakeMove(move, board);

    // apply extensions
    int newDepth = depth + extension;

    // Late move reductions
    if (depth > 1 && legalMoves > 1 && !(isPV && IsCap(move))) {
      // increase reduction on non-pv
      if (!ttPv)
        R += 2;

      // increase reduction if our eval is declining
      if (!improving)
        R++;

      // reduce these special quiets less
      if (killerOrCounter)
        R -= 2;

      // Reduce more on expected cut nodes
      // idea from komodo/sf, explained by Don Daily here
      // https://talkchess.com/forum3/viewtopic.php?f=7&t=47577&start=10#p519741
      // and https://www.chessprogramming.org/Node_Types
      if (cutnode)
        R += 1 + !IsCap(move);

      // prevent dropping into QS, extending, or reducing all extensions
      R = Min(newDepth, Max(R, 1));

      int lmrDepth = newDepth - R;
      score        = -Negamax(-alpha - 1, -alpha, lmrDepth, 1, thread, &childPv, ss + 1);

      if (score > alpha && R > 1) {
        // Credit to Viz (and lonfom) for the following modification of the zws
        // re-search depth. They can be found in SF as doDeeperSearch + doShallowerSearch
        newDepth += (score > bestScore + 76);
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

    if (isRoot) {
      RootMove* rm = thread->rootMoves;
      for (int i = 1; i < thread->numRootMoves; i++)
        if (thread->rootMoves[i].move == move) {
          rm = &thread->rootMoves[i];
          break;
        }

      rm->nodes += thread->nodes - startingNodeCount;

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
        UpdateHistories(ss, thread, move, depth + (bestScore > beta + 78), quiets, numQuiets, captures, numCaptures);
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
  if (!ss->skip && !(isRoot && thread->multiPV > 0)) {
    int bound = bestScore >= beta ? BOUND_LOWER : bestScore <= origAlpha ? BOUND_UPPER : BOUND_EXACT;
    TTPut(tt, board->zobrist, depth, bestScore, bound, bestMove, ss->ply, ss->staticEval, ttPv);
  }

  return bestScore;
}

int Quiesce(int alpha, int beta, int depth, ThreadData* thread, SearchStack* ss) {
  Board* board = &thread->board;

  int score     = -CHECKMATE;
  int futility  = -CHECKMATE;
  int bestScore = -CHECKMATE + ss->ply;
  int isPV      = beta - alpha != 1;
  int inCheck   = !!board->checkers;
  int eval      = ss->staticEval;

  Move hashMove = NULL_MOVE;
  Move bestMove = NULL_MOVE;
  Move move     = NULL_MOVE;
  MovePicker mp;

  if (LoadRlx(Threads.stop) || (!thread->idx && CheckLimits(thread)))
    // hot exit
    longjmp(thread->exit, 1);

  IncRlx(thread->nodes);

  // draw check
  if (IsDraw(board, ss->ply))
    return 0;

  // prevent overflows
  if (ss->ply >= MAX_SEARCH_PLY - 1)
    return inCheck ? 0 : Evaluate(board, thread);

  // check the transposition table for previous info
  int ttHit   = 0;
  int ttScore = UNKNOWN;
  int ttEval  = EVAL_UNKNOWN;
  int ttDepth = DEPTH_OFFSET;
  int ttBound = BOUND_UNKNOWN;
  int ttPv    = isPV;

  TTEntry* tt = TTProbe(board->zobrist, ss->ply, &ttHit, &hashMove, &ttScore, &ttEval, &ttDepth, &ttBound, &ttPv);

  // TT score pruning, ttHit implied with adjusted score
  if (!isPV && ttScore != UNKNOWN && (ttBound & (ttScore >= beta ? BOUND_LOWER : BOUND_UPPER)))
    return ttScore;

  if (inCheck) {
    eval = ss->staticEval = EVAL_UNKNOWN;
  } else {
    if (ttHit) {
      eval = ss->staticEval = ttEval;
      if (ss->staticEval == EVAL_UNKNOWN)
        eval = ss->staticEval = Evaluate(board, thread);

      // correct eval on fmr
      eval = AdjustEvalOnFMR(board, eval);

      if (ttScore != UNKNOWN && (ttBound & (ttScore > eval ? BOUND_LOWER : BOUND_UPPER)))
        eval = ttScore;
    } else {
      eval = ss->staticEval = Evaluate(board, thread);
      // correct eval on fmr
      eval = AdjustEvalOnFMR(board, eval);

      TTPut(tt, board->zobrist, -1, UNKNOWN, BOUND_UNKNOWN, NULL_MOVE, ss->ply, ss->staticEval, ttPv);
    }

    // stand pat
    if (eval >= beta)
      return eval;

    if (eval > alpha)
      alpha = eval;

    bestScore = eval;

    futility = bestScore + 60;
  }

  if (!inCheck)
    InitQSMovePicker(&mp, thread, depth >= -1);
  else
    InitQSEvasionsPicker(&mp, hashMove, thread, ss);

  int legalMoves = 0;

  while ((move = NextMove(&mp, board, 1))) {
    if (!IsLegal(move, board))
      continue;

    legalMoves++;

    if (bestScore > -TB_WIN_BOUND) {
      if (inCheck && !(IsCap(move) || IsPromo(move)))
        break;

      if (!inCheck && mp.phase != QS_PLAY_QUIET_CHECKS && futility <= alpha && !SEE(board, move, 1)) {
        bestScore = Max(bestScore, futility);
        continue;
      }

      if (!SEE(board, move, 0))
        continue;
    }

    TTPrefetch(KeyAfter(board, move));
    ss->move = move;
    ss->ch   = &thread->ch[IsCap(move)][Moving(move)][To(move)];
    MakeMove(move, board);

    score = -Quiesce(-beta, -alpha, depth - 1, thread, ss + 1);

    UndoMove(move, board);

    if (score > bestScore) {
      bestScore = score;

      if (score > alpha) {
        bestMove = move;
        alpha    = score;
      }

      // failed high
      if (alpha >= beta)
        break;
    }
  }

  if (!legalMoves && inCheck)
    return -CHECKMATE + ss->ply;

  int bound = bestScore >= beta ? BOUND_LOWER : BOUND_UPPER;
  TTPut(tt, board->zobrist, 0, bestScore, bound, bestMove, ss->ply, ss->staticEval, ttPv);

  return bestScore;
}

void PrintUCI(ThreadData* thread, int alpha, int beta, Board* board) {
  int depth       = thread->depth;
  uint64_t nodes  = NodesSearched();
  uint64_t tbhits = TBHits();
  uint64_t time   = Max(1, GetTimeMS() - Limits.start);
  uint64_t nps    = 1000 * nodes / time;
  int hashfull    = TTFull();

  for (int i = 0; i < Limits.multiPV; i++) {
    int updated = (thread->rootMoves[i].score != -CHECKMATE);
    if (depth == 1 && i > 0 && !updated)
      break;

    int realDepth = updated ? depth : Max(1, depth - 1);
    int bounded   = updated ? Max(alpha, Min(beta, thread->rootMoves[i].score)) : thread->rootMoves[i].previousScore;
    int printable = bounded > MATE_BOUND           ? (CHECKMATE - bounded + 1) / 2 :
                    bounded < -MATE_BOUND          ? -(CHECKMATE + bounded) / 2 :
                    abs(bounded) > TB_WIN_BOUND ? bounded : // don't normalize our fake tb scores or real tb scores
                                                     Normalize(bounded); // convert to 50% WR at 100cp
    char* type  = abs(bounded) > MATE_BOUND ? "mate" : "cp";
    char* bound = " ";
    if (updated)
      bound = bounded >= beta ? " lowerbound " : bounded <= alpha ? " upperbound " : " ";

    printf("info depth %d seldepth %d multipv %d score %s %d%s",
           realDepth,
           thread->rootMoves[i].seldepth,
           i + 1,
           type,
           printable,
           bound);

    if (SHOW_WDL) {
      int ply  = Max(0, 2 * (board->moveNo - 1)) + board->stm;
      int wdlW = WRModel(bounded, ply);
      int wdlL = WRModel(-bounded, ply);
      int wdlD = 1000 - wdlW - wdlL;

      printf("wdl %d %d %d ", wdlW, wdlD, wdlL);
    }

    printf("nodes %" PRId64 " nps %" PRId64 " hashfull %d tbhits %" PRId64 " time %" PRId64 " pv ",
           nodes,
           nps,
           hashfull,
           tbhits,
           time);

    PV* pv = &thread->rootMoves[i].pv;
    if (pv->count)
      PrintPV(pv, board);
    else
      printf("%s\n", MoveToStr(pv->moves[0], board));
  }
}

void PrintPV(PV* pv, Board* board) {
  for (int i = 0; i < pv->count; i++)
    printf("%s ", MoveToStr(pv->moves[i], board));
  printf("\n");
}

void SortRootMoves(ThreadData* thread, int offset) {
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

int ValidRootMove(ThreadData* thread, Move move) {
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

  thread->board.accumulators = thread->accumulators;
  thread->previousScore      = UNKNOWN;
}

void SearchClear() {
  for (int i = 0; i < Threads.count; i++)
    ThreadWake(Threads.threads[i], THREAD_SEARCH_CLEAR);
  for (int i = 0; i < Threads.count; i++)
    ThreadWaitUntilSleep(Threads.threads[i]);
}
