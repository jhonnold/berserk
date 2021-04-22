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
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

// We have a 16 bit range for scores
const int CHECKMATE = INT16_MAX;
const int MATE_BOUND = 30000;

// Reverse futility pruning
int RFP_BASE = 62;
int RFP_STEP_RISE = 8;

// static evaluation pruning (capture/quiet)
int SEE_PRUNE_CAPTURE_CUTOFF = 70;
int SEE_PRUNE_CUTOFF = 20;

// Delta pruning
int DELTA_CUTOFF = 200;

// arrays to store these pruning cutoffs at specific depths
int LMR[MAX_SEARCH_PLY][64];
int LMP[2][MAX_SEARCH_PLY];
int STATIC_PRUNE[2][MAX_SEARCH_PLY];
int RFP[MAX_SEARCH_PLY];

void InitPruningAndReductionTables() {
  for (int depth = 0; depth < MAX_SEARCH_PLY; depth++)
    for (int moves = 0; moves < 64; moves++)
      // Credit to Ethereal for this LMR
      LMR[depth][moves] = (int)(0.6f + log(depth) * log(1.2f * moves) / 2.5f);

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

  // TODO: what is the fallback if no bestMove?
  Move bestMove = threads[0].data.bestMove;
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

  int alpha = -CHECKMATE, beta = CHECKMATE, score = 0;

  // set a hot exit point for this thread
  if (!setjmp(thread->exit)) {
    // Iterative deepening
    for (int depth = 1; depth <= params->depth; depth++) {
      // Ignore aspiration windows till we're at a reasonable depth
      // aspiration windows start at 1/10th of a pawn and grows outward at 150%, utilizing
      // returned fail soft values
      int delta = depth >= 5 ? 10 : CHECKMATE;

      alpha = max(score - delta, -CHECKMATE);
      beta = min(score + delta, CHECKMATE);

      while (!params->stopped) {
        // search!
        score = Negamax(alpha, beta, depth, thread, pv);

        if (score <= alpha) {
          // fail low is no bueno, so lower beta, along with alpha for our window
          beta = (alpha + beta) / 2;
          alpha = max(alpha - delta, -CHECKMATE);
        } else if (score >= beta) {
          // raise beta on fail high
          beta = min(beta + delta, CHECKMATE);
        } else
          break;

        // increase delta
        delta += delta / 2;
      }

      data->bestMove = pv->moves[0];
      data->score = score;

      if (mainThread)
        PrintInfo(pv, score, depth, thread);
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

  int isPV = beta - alpha != 1;
  int isRoot = !data->ply;
  int bestScore = -CHECKMATE;
  int origAlpha = alpha;

  Move bestMove = NULL_MOVE;
  Move skipMove = data->skipMove[data->ply];

  // drop into tactical moves only
  if (depth <= 0)
    return Quiesce(alpha, beta, thread, pv);

  data->nodes++;
  data->seldepth = max(data->ply, data->seldepth);

  // Either mainthread has ended us OR we've run out of time
  // this second check is more expensive and done only every 1024 nodes
  // 1Mnps ~1ms
  if (params->stopped || ((data->nodes & 1023) == 0 && params->timeset && GetTimeMS() > params->endTime))
    longjmp(thread->exit, 1);

  if (!isRoot) {
    // draw
    if (IsRepetition(board) || IsMaterialDraw(board) || (board->halfMove > 99))
      return 0; // TODO: Contempt factor? or randomness?

    // Prevent overflows
    if (data->ply > MAX_SEARCH_PLY - 1)
      return Evaluate(board);

    // Mate distance pruning
    alpha = max(alpha, -CHECKMATE + data->ply);
    beta = min(beta, CHECKMATE - data->ply - 1);
    if (alpha >= beta)
      return alpha;
  }

  // check the transposition table for previous info
  TTValue ttValue = skipMove ? NO_ENTRY : TTProbe(board->zobrist);

  // TT score cutoffs
  if (!isPV && ttValue && TTDepth(ttValue) >= depth) {
    int score = TTScore(ttValue, data->ply);
    int flag = TTFlag(ttValue);

    if (flag == TT_EXACT || (flag == TT_LOWER && score >= beta) || (flag == TT_UPPER && score <= alpha))
      return score;
  }

  // pull previous static eval from tt - this is depth independent
  int eval = data->evals[data->ply] = (ttValue ? TTEval(ttValue) : Evaluate(board));
  // getting better if eval has gone up
  int improving = data->ply >= 2 && (data->evals[data->ply] > data->evals[data->ply - 2]);

  data->skipMove[data->ply + 1] = NULL_MOVE;
  data->killers[data->ply + 1][0] = NULL_MOVE;
  data->killers[data->ply + 1][1] = NULL_MOVE;

  if (!isPV && !board->checkers) {
    // Our TT might have a more accurate evaluation score, use this
    if (ttValue && TTDepth(ttValue) >= depth) {
      int ttScore = TTScore(ttValue, data->ply);
      if (TTFlag(ttValue) == (ttScore > eval ? TT_LOWER : TT_UPPER))
        eval = ttScore;
    }

    // Reverse Futility Pruning
    if (depth <= 6 && eval - RFP[depth] >= beta && eval < MATE_BOUND)
      return eval;

    // Null move pruning
    if (depth >= 3 && data->moves[data->ply - 1] != NULL_MOVE && !skipMove && eval >= beta && HasNonPawn(board)) {
      int R = 3 + depth / 6 + min((eval - beta) / 200, 3);
      R = min(depth, R); // don't go too low

      data->moves[data->ply++] = NULL_MOVE;
      MakeNullMove(board);

      int score = -Negamax(-beta, -beta + 1, depth - R, thread, &childPv);

      UndoNullMove(board);
      data->ply--;

      if (score >= beta)
        return beta;
    }
  }

  // iid (internal iterative deepening)
  // if this is a pv node with no hash entry, then we need to find a good first move
  // Calling a shallower search will force a tt entry and we probe that once done
  if (isPV && !ttValue && !isRoot && depth > 5) {
    Negamax(alpha, beta, depth - 2, thread, pv);
    ttValue = TTProbe(board->zobrist);
  }

  MoveList moveList;
  GenerateAllMoves(&moveList, board, data);

  int numMoves = 0;
  for (int i = 0; i < moveList.count; i++) {
    ChooseTopMove(&moveList, i);
    Move move = moveList.moves[i];

    // don't search this during singular
    if (skipMove == move)
      continue;

    int tactical = MovePromo(move) || MoveCapture(move);

    if (!isPV && bestScore > -MATE_BOUND) {
      // late move pruning at low depth if it's quiet and we've looked at ALOT
      if (depth <= 8 && !tactical && numMoves >= LMP[improving][depth])
        continue;

      // Static evaluation pruning, this applies for both quiet and tactical moves
      // quiet moves use a quadratic scale upwards
      if (SEE(board, move) < STATIC_PRUNE[tactical][depth])
        continue;
    }

    // singular extension
    // if one move is better than all the rest, then we consider this singular
    // and look at it more (extend). Singular is determined by checking all other
    // moves at a shallow depth on a nullwindow that is somewhere below the tt evaluation
    // implemented using "skip move" recursion like in SF (allows for reductions when doing singular search)
    int singularExtension = 0;
    if (depth >= 8 && !skipMove && !isRoot && move == TTMove(ttValue) && TTDepth(ttValue) >= depth - 3 &&
        abs(TTScore(ttValue, data->ply)) < MATE_BOUND && TTFlag(ttValue) == TT_LOWER) {
      int sBeta = max(TTScore(ttValue, data->ply) - depth * 2, -CHECKMATE);
      int sDepth = depth / 2 - 1;

      data->skipMove[data->ply] = move;
      int score = Negamax(sBeta - 1, sBeta, sDepth, thread, pv);
      data->skipMove[data->ply] = NULL_MOVE;

      // no score failed above sBeta, so this is singular
      if (score < sBeta)
        singularExtension = 1;
      else if (sBeta >= beta)
        // multi-cut - since a move failed above sBeta which is already above beta,
        // we can prune (its also assumed tt would fail high)
        return sBeta;
    }

    numMoves++;
    data->moves[data->ply++] = move;
    MakeMove(move, board);

    int newDepth = depth;
    if (singularExtension || board->checkers) // extend if in check
      newDepth++;                             // apply the extension

    // start of late move reductions
    int R = 1, score = -CHECKMATE;
    if (depth >= 2 && numMoves > 1) {
      R = LMR[min(depth, 63)][min(numMoves, 63)];

      if (!tactical) {
        if (!isPV)
          R++;

        if (!improving)
          R++;

        // additional reduction on historical scores
        // counters/killers have a high score (so they get less reduction by 2)
        // ranging from [-2, 2]
        // 100 was picked as a mean since that's what the mean was at depth 13 for the bench
        // TODO: Look at this more closely?
        R -= min(2, (moveList.scores[i] - 149) / 50);
      } else {
        // reduce for all captures
        R--;
      }

      // prevent dropping into QS, extending, or reducing all extensions
      R = min(depth - 1, max(R, 1));
    }

    // First move of a PV node
    if (isPV && numMoves == 1) {
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
        // update histories for this fail high if it's quiet
        if (!tactical) {
          AddKillerMove(data, move);
          AddCounterMove(data, move, data->moves[data->ply - 1]);
          AddHistoryHeuristic(data, move, board->side, depth);
        }

        // all moves that came before this did not fail high and
        // need to have their butterfly heuristic applied
        for (int j = 0; j < i; j++) {
          if (MoveCapture(moveList.moves[j]) || MovePromo(moveList.moves[j]))
            continue;

          AddBFHeuristic(data, moveList.moves[j], board->side, depth);
        }

        break;
      }
    }
  }

  // Checkmate detection using movecount
  if (!moveList.count)
    return board->checkers ? -CHECKMATE + data->ply : 0;

  if (!skipMove) {
    // save to the TT
    // TT_LOWER = we failed high
    // TT_UPPER = we didnt raise alpha
    // TT_EXACT = in
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
  if (IsMaterialDraw(board) || IsRepetition(board) || (board->halfMove > 99))
    return 0;

  // prevent overflows
  if (data->ply > MAX_SEARCH_PLY - 1)
    return Evaluate(board);

  // check the transposition table for previous info
  TTValue ttValue = TTProbe(board->zobrist);
  // TT score pruning - no depth check required since everything in QS is depth 0
  if (ttValue) {
    int score = TTScore(ttValue, data->ply);
    int flag = TTFlag(ttValue);

    if (flag == TT_EXACT || (flag == TT_LOWER && score >= beta) || (flag == TT_UPPER && score <= alpha))
      return score;
  }

  // pull cached eval if it exists
  int eval = data->evals[data->ply] = (ttValue ? TTEval(ttValue) : Evaluate(board));

  // can we use an improved evaluation from the tt?
  if (ttValue) {
    int ttEval = TTScore(ttValue, data->ply);
    if (TTFlag(ttValue) == (ttEval > eval ? TT_LOWER : TT_UPPER))
      eval = ttEval;
  }

  // stand pat
  if (eval >= beta)
    return eval;

  if (eval > alpha)
    alpha = eval;

  int bestScore = eval;

  MoveList moveList;
  GenerateTacticalMoves(&moveList, board);

  for (int i = 0; i < moveList.count; i++) {
    ChooseTopMove(&moveList, i);
    Move move = moveList.moves[i];

    if (MovePromo(move)) {
      // consider all non-queen promotions as quiet
      if (MovePromo(move) < QUEEN[WHITE])
        continue;
    } else {
      int captured = MoveEP(move) ? PAWN[board->xside] : board->squares[MoveEnd(move)];

      assert(captured != NO_PIECE);

      // delta pruning
      // if we can't catch alpha even when capturing a piece and then some, prune
      if (eval + DELTA_CUTOFF + STATIC_MATERIAL_VALUE[PIECE_TYPE[captured]] < alpha)
        continue;
    }

    // skip bad captures (SEE scores are negative)
    if (moveList.scores[i] < 0)
      break;

    data->moves[data->ply++] = move;
    MakeMove(move, board);

    int score = -Quiesce(-beta, -alpha, thread, &childPv);

    UndoMove(move, board);
    data->ply--;

    if (score > bestScore) {
      bestScore = score;

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

  return bestScore;
}

inline void PrintInfo(PV* pv, int score, int depth, ThreadData* thread) {
  uint64_t nodes = NodesSearched(thread->threads);

  if (score > MATE_BOUND) {
    int movesToMate = (CHECKMATE - score) / 2 + ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d nodes %lld time %ld score mate %d pv ", depth, thread->data.seldepth, nodes,
           GetTimeMS() - thread->params->startTime, movesToMate);
  } else if (score < -MATE_BOUND) {
    int movesToMate = (CHECKMATE + score) / 2 - ((CHECKMATE - score) & 1);

    printf("info depth %d seldepth %d  nodes %lld time %ld score mate -%d pv ", depth, thread->data.seldepth, nodes,
           GetTimeMS() - thread->params->startTime, movesToMate);
  } else {
    printf("info depth %d seldepth %d nodes %lld time %ld score cp %d pv ", depth, thread->data.seldepth, nodes,
           GetTimeMS() - thread->params->startTime, score);
  }
  PrintPV(pv);
}

void PrintPV(PV* pv) {
  for (int i = 0; i < pv->count; i++)
    printf("%s ", MoveToStr(pv->moves[i]));
  printf("\n");
}