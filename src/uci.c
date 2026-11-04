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

#include "uci.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "bench.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "nn/evaluate.h"
#include "options.h"
#include "perft.h"
#include "pyrrhic/tbprobe.h"
#include "search.h"
#include "see.h"
#include "thread.h"
#include "transposition.h"
#include "tune.h"
#include "util.h"

static int MOVE_OVERHEAD  = 50;
static int MULTI_PV       = 1;
static int PONDER_ENABLED = 0;
static int SOFT_NODES     = 0;

// All WDL work below is thanks to the work of vondele@ and
// this repo: https://github.com/vondele/WLD_model

// Third order polynomial fit of Berserk data
static const double as[4] = {0.01538655, -1.33196512, 7.43032607, 152.07701643};
static const double bs[4] = {-2.66442843, 18.35463702, -38.91760538, 56.27652511};

// win% as permilli given score and ply
static int WRModel(Score s, Ply ply) {
  double m = Min(240, ply) / 64.0;

  double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
  double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

  double x = Min(4000.0, Max(-4000.0, s));

  return 0.5 + 1000 / (1 + exp((a - x) / b));
}

static void PrintPV(PV* pv, Board* board);

void PrintUCI(ThreadData* thread, Depth depth, Score alpha, Score beta, Board* board) {
  uint64_t nodes  = NodesSearched();
  uint64_t tbhits = TBHits();
  long elapsed    = GetTimeMS() - Limits.start;
  uint64_t time   = Max(1, elapsed);
  uint64_t nps    = 1000 * nodes / time;
  int hashfull    = TTFull();

  for (int i = 0; i < Limits.multiPV; i++) {
    Bool updated = (thread->rootMoves[i].score != -CHECKMATE);
    if (depth == 1 && i > 0 && !updated)
      break;

    Depth realDepth  = Max(1, updated ? depth : depth - 1);
    Score bounded    = updated ? Max(alpha, Min(beta, thread->rootMoves[i].score)) : thread->rootMoves[i].previousScore;
    int printable    = bounded > MATE_BOUND  ? (CHECKMATE - bounded + 1) / 2 :
                       bounded < -MATE_BOUND ? -(CHECKMATE + bounded) / 2 :
                       !NORMALIZE || abs(bounded) > TB_WIN_BOUND ? // don't normalize our fake tb scores or real tb scores
                      bounded :
                         Normalize(bounded); // convert to 50% WR at 100cp
    const char* type = abs(bounded) > MATE_BOUND ? "mate" : "cp";
    const char* bound = " ";
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
      Ply ply  = Max(0, 2 * (board->moveNo - 1)) + board->stm;
      int wdlW = WRModel(bounded, ply);
      int wdlL = WRModel(-bounded, ply);
      int wdlD = 1000 - wdlW - wdlL;

      printf("wdl %d %d %d ", wdlW, wdlD, wdlL);
    }

    printf("nodes %" PRIu64 " nps %" PRIu64 " hashfull %d tbhits %" PRIu64 " time %" PRIu64 " pv ",
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

static void PrintPV(PV* pv, Board* board) {
  for (int i = 0; i < pv->count; i++)
    printf("%s ", MoveToStr(pv->moves[i], board));
  printf("\n");
}

void PrintBestMove(Move bestMove, Move ponderMove, Board* board) {
  printf("bestmove %s", MoveToStr(bestMove, board));
  if (ponderMove)
    printf(" ponder %s", MoveToStr(ponderMove, board));
  printf("\n");
}

void PrintCurrMove(Depth depth, Move move, int number, Board* board) {
  printf("info depth %d currmove %s currmovenumber %d\n", depth, MoveToStr(move, board), number);
}

// uci "go" command
static void CmdGo(char* args, Board* board) {
  ResetLimits();
  Limits.multiPV = MULTI_PV;

  int perft = 0, movesToGo = -1, moveTime = -1, time = -1, inc = 0, depth = -1, ponder = 0, mate = 0;
  uint64_t nodes = 0;

  SimpleMoveList rootMoves;
  RootMoves(&rootMoves, board);

  char* save;
  for (char* tok = strtok_r(args, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
    if (!strcmp(tok, "infinite")) {
      Limits.infinite = 1;
    } else if (!strcmp(tok, "ponder")) {
      ponder = 1;
    } else if (!strcmp(tok, "searchmoves")) {
      Limits.searchMoves = 1;

      for (char* moves = strtok_r(NULL, " \t", &save); moves; moves = strtok_r(NULL, " \t", &save))
        for (int i = 0; i < rootMoves.count; i++)
          if (!strcmp(MoveToStr(rootMoves.moves[i], board), moves))
            Limits.searchable.moves[Limits.searchable.count++] = rootMoves.moves[i];

      break;
    } else {
      // every other keyword takes a value
      char* value = strtok_r(NULL, " \t", &save);
      if (!value)
        break;

      if (!strcmp(tok, "perft"))
        perft = atoi(value);
      else if (!strcmp(tok, "wtime") && board->stm == WHITE)
        time = atoi(value);
      else if (!strcmp(tok, "btime") && board->stm == BLACK)
        time = atoi(value);
      else if (!strcmp(tok, "winc") && board->stm == WHITE)
        inc = atoi(value);
      else if (!strcmp(tok, "binc") && board->stm == BLACK)
        inc = atoi(value);
      else if (!strcmp(tok, "movestogo"))
        movesToGo = Max(1, Min(50, atoi(value)));
      else if (!strcmp(tok, "movetime"))
        moveTime = atoi(value);
      else if (!strcmp(tok, "depth"))
        depth = Min(MAX_SEARCH_PLY - 1, atoi(value));
      else if (!strcmp(tok, "mate"))
        mate = Min(MAX_SEARCH_PLY - 1, atoi(value));
      else if (!strcmp(tok, "nodes"))
        nodes = Max(1, strtoull(value, NULL, 10));
    }
  }

  if (ponder && !PONDER_ENABLED) {
    printf("info string Enable option Ponder to use 'ponder'\n");
    return;
  }

  if (perft) {
    PerftTest(perft, board);
    return;
  }

  Limits.depth     = depth;
  Limits.mate      = mate;
  Limits.nodes     = nodes;
  Limits.softNodes = 0;

  if (Limits.nodes && SOFT_NODES) {
    Limits.softNodes = Limits.nodes;
    Limits.nodes     = Limits.softNodes * 100;
  }

  if (Limits.nodes)
    Limits.hitrate = Min(1000, Max(1, Limits.nodes / 100));
  else
    Limits.hitrate = 1000;

  // "movetime" is essentially making a move with 1 to go for TC
  if (moveTime != -1) {
    Limits.timeset = 1;
    Limits.alloc   = INT32_MAX;
    Limits.max     = moveTime;
  } else {
    if (time != -1) {
      Limits.timeset = 1;

      if (movesToGo == -1) {
        int total = Max(1, time + 50 * inc - 50 * MOVE_OVERHEAD);

        Limits.alloc = Min(time * TM_ALLOC_TIME, total * TM_ALLOC_TOTAL);
        Limits.max   = Min(time * TM_MAX_TIME - MOVE_OVERHEAD, Limits.alloc * TM_MAX_ALLOC) - 10;
      } else {
        int total = Max(1, time + movesToGo * inc - MOVE_OVERHEAD);

        Limits.alloc = Min(time * 0.9, (0.9 * total) / Max(1, movesToGo / 2.5));
        Limits.max   = Min(time * 0.8 - MOVE_OVERHEAD, Limits.alloc * 5.5) - 10;
      }
    } else {
      // no time control
      Limits.timeset = 0;
      Limits.max     = INT_MAX;
    }
  }

  Limits.multiPV = Min(Limits.multiPV, Limits.searchMoves ? Limits.searchable.count : rootMoves.count);
  if (rootMoves.count == 1 && Limits.timeset)
    Limits.max = Min(250, Limits.max);

  if (depth <= 0)
    Limits.depth = MAX_SEARCH_PLY - 1;

  printf(
    "info string time %d start %ld alloc %d max %d depth %d timeset %d "
    "searchmoves %d\n",
    time,
    Limits.start,
    Limits.alloc,
    Limits.max,
    Limits.depth,
    Limits.timeset,
    Limits.searchable.count);

  StartSearch(board, ponder);
}

// uci "position" command
static void CmdPosition(char* args, Board* board) {
  char* ptrChar = args;

  if (strncmp(args, "startpos", 8) == 0) {
    ParseFen(START_FEN, board);
  } else {
    ptrChar = strstr(args, "fen");
    if (ptrChar == NULL)
      ParseFen(START_FEN, board);
    else {
      ptrChar += 4;
      ParseFen(ptrChar, board);
    }
  }

  ptrChar = strstr(args, "moves");

  if (ptrChar == NULL)
    return;

  ptrChar += 6;

  char* save;
  for (char* moves = strtok_r(ptrChar, " \t", &save); moves != NULL; moves = strtok_r(NULL, " \t", &save)) {
    Move enteredMove = ParseMove(moves, board);
    if (!enteredMove)
      break;

    MakeMoveUpdate(enteredMove, board, 0);

    if (board->fmr == 0)
      board->histPly = board->nullply = 0;
  }
}

// Stop a running search, waking the main thread if it is parked on ponder or infinite
static void StopSearch(void) {
  if (!Threads.searching)
    return;

  Threads.stop = 1;
  pthread_mutex_lock(&Threads.lock);
  if (Threads.sleeping)
    ThreadWake(Threads.threads[0], THREAD_RESUME);
  Threads.sleeping = 0;
  pthread_mutex_unlock(&Threads.lock);
}

// ---- options ----

static void OptHash(int mb, Board* board) {
  (void) board;

  uint64_t bytesAllocated = TTInit(mb);
  ThreadsClearTT();
  uint64_t totalEntries = BUCKET_SIZE * bytesAllocated / sizeof(TTBucket);
  printf("info string set Hash to value %d (%" PRIu64 " bytes) (%" PRIu64 " entries)\n",
         mb,
         bytesAllocated,
         totalEntries);
}

static void OptThreads(int n, Board* board) {
  (void) board;

  ThreadsSetNumber(n);
  printf("info string set Threads to value %d\n", Threads.count);
}

static void OptSyzygyPath(char* path, Board* board) {
  (void) board;

  if (tb_init(path))
    printf("info string set SyzygyPath to value %s\n", path);
  else
    printf("info string FAILED!\n");
}

static void OptMultiPV(int n, Board* board) {
  (void) board;

  MULTI_PV = n;
  printf("info string set MultiPV to value %d\n", MULTI_PV);
}

static void OptPonder(int b, Board* board) {
  (void) board;

  PONDER_ENABLED = b;
  printf("info string set Ponder to value %s\n", PONDER_ENABLED ? "true" : "false");
}

static void OptShowWDL(int b, Board* board) {
  (void) board;

  SHOW_WDL = b;
  printf("info string set SHOW_WDL to value %s\n", SHOW_WDL ? "true" : "false");
}

static void OptChess960(int b, Board* board) {
  CHESS_960 = b;
  printf("info string set UCI_Chess960 to value %s\n", CHESS_960 ? "true" : "false");
  printf("info string Resetting board...\n");

  ParseFen(START_FEN, board);
  ThreadsClearTT();
  ThreadsClearSearch();
}

static void OptMoveOverhead(int n, Board* board) {
  (void) board;

  MOVE_OVERHEAD = n;
}

static void OptContempt(int n, Board* board) {
  (void) board;

  CONTEMPT = n;
}

static void OptEvalFile(char* path, Board* board) {
  (void) board;

  Bool success = 0;

  if (strncmp(path, "<empty>", 7))
    success = LoadNetwork(path);
  else {
    LoadDefaultNN();
    success = 1;
  }

  if (success) {
    // the refresh tables cache accumulators built from the previous network's weights
    ThreadsResetRefreshTables();
    printf("info string set EvalFile to value %s\n", path);
  }
}

static void OptMinimal(int b, Board* board) {
  (void) board;

  MINIMAL = b;
  printf("info string set Minimal to value %s\n", MINIMAL ? "true" : "false");
}

static void OptNormalize(int b, Board* board) {
  (void) board;

  NORMALIZE = b;
  printf("info string set Normalize to value %s\n", NORMALIZE ? "true" : "false");
}

static void OptSoftNodes(int b, Board* board) {
  (void) board;

  SOFT_NODES = b;
  printf("info string set SoftNodes to value %s\n", SOFT_NODES ? "true" : "false");
}

typedef enum {
  OPT_SPIN,
  OPT_CHECK,
  OPT_STRING
} OptionType;

typedef struct {
  const char* name;
  OptionType type;
  int def, min, max;                // spin and check (0/1) only
  void (*setInt)(int, Board*);      // spin and check, value clamped to [min, max]
  void (*setString)(char*, Board*); // string
} UCIOption;

static const UCIOption OPTIONS[] = {
  {"Hash", OPT_SPIN, 16, 2, HASH_MAX, OptHash, NULL},
  {"Threads", OPT_SPIN, 1, 1, 2048, OptThreads, NULL},
  {"SyzygyPath", OPT_STRING, 0, 0, 0, NULL, OptSyzygyPath},
  {"MultiPV", OPT_SPIN, 1, 1, 256, OptMultiPV, NULL},
  {"Ponder", OPT_CHECK, 0, 0, 1, OptPonder, NULL},
  {"UCI_ShowWDL", OPT_CHECK, 1, 0, 1, OptShowWDL, NULL},
  {"UCI_Chess960", OPT_CHECK, 0, 0, 1, OptChess960, NULL},
  {"MoveOverhead", OPT_SPIN, 50, 0, 10000, OptMoveOverhead, NULL},
  {"Contempt", OPT_SPIN, 0, -100, 100, OptContempt, NULL},
  {"EvalFile", OPT_STRING, 0, 0, 0, NULL, OptEvalFile},
  {"Minimal", OPT_CHECK, 0, 0, 1, OptMinimal, NULL},
  {"Normalize", OPT_CHECK, 1, 0, 1, OptNormalize, NULL},
  {"SoftNodes", OPT_CHECK, 0, 0, 1, OptSoftNodes, NULL},
};

#define NUM_OPTIONS ((int) (sizeof(OPTIONS) / sizeof(OPTIONS[0])))

#ifdef SPSA
// OpenBench SPSA input: name, type, value, min, max, C_end, R_end
static double TunableStep(const Tunable* t) {
  double step = fabs(t->defValue) / 10;
  return t->isFloat ? step : Max(0.5, step);
}

static void CmdSpsa(char* args, Board* board) {
  (void) args, (void) board;

  for (int i = 0; i < TUNABLES_COUNT; i++) {
    const Tunable* t = &TUNABLES_TABLE[i];
    printf("%s, %s, %s, %s, %s, %g, 0.002\n",
           t->name,
           t->isFloat ? "float" : "int",
           t->def,
           t->min,
           t->max,
           TunableStep(t));
  }
}

static Bool SetTunable(const char* name, const char* value) {
  for (int i = 0; i < TUNABLES_COUNT; i++) {
    const Tunable* t = &TUNABLES_TABLE[i];
    if (strcasecmp(t->name, name))
      continue;

    if (t->isFloat)
      *(double*) t->value = Max(t->minValue, Min(t->maxValue, atof(value)));
    else
      *(int*) t->value = Max((int) t->minValue, Min((int) t->maxValue, atoi(value)));

    // some tunables only feed precomputed tables
    InitPruningAndReductionTables();
    InitSeeValues();
    return 1;
  }

  return 0;
}
#endif

static void CmdUci(char* args, Board* board) {
  (void) args, (void) board;

  printf("id name Berserk " VERSION "\n");
  printf("id author Jay Honnold\n");

  for (int i = 0; i < NUM_OPTIONS; i++) {
    const UCIOption* o = &OPTIONS[i];

    if (o->type == OPT_SPIN)
      printf("option name %s type spin default %d min %d max %d\n", o->name, o->def, o->min, o->max);
    else if (o->type == OPT_CHECK)
      printf("option name %s type check default %s\n", o->name, o->def ? "true" : "false");
    else
      printf("option name %s type string default <empty>\n", o->name);
  }

#ifdef SPSA
  for (int i = 0; i < TUNABLES_COUNT; i++) {
    const Tunable* t = &TUNABLES_TABLE[i];

    if (t->isFloat)
      printf("option name %s type string default %s\n", t->name, t->def);
    else
      printf("option name %s type spin default %s min %s max %s\n", t->name, t->def, t->min, t->max);
  }
#endif

  printf("uciok\n");
}

// setoption name <name> [value <value>]
static void CmdSetOption(char* args, Board* board) {
  if (strncmp(args, "name ", 5)) {
    printf("info string Invalid setoption: %s\n", args);
    return;
  }

  char* name  = args + 5;
  char* value = strstr(name, " value ");
  if (value) {
    *value = '\0';
    value += 7;
  } else {
    static char none[1];
    value = none;
  }

  for (int i = 0; i < NUM_OPTIONS; i++) {
    const UCIOption* o = &OPTIONS[i];
    if (strcasecmp(o->name, name))
      continue;

    if (o->type == OPT_SPIN)
      o->setInt(Max(o->min, Min(o->max, atoi(value))), board);
    else if (o->type == OPT_CHECK)
      o->setInt(!strncasecmp(value, "true", 4), board);
    else
      o->setString(value, board);

    return;
  }

#ifdef SPSA
  if (SetTunable(name, value))
    return;
#endif

  printf("info string Unknown option: %s\n", name);
}

// ---- commands ----

static void CmdIsReady(char* args, Board* board) {
  (void) args, (void) board;

  printf("readyok\n");
}

static void CmdUciNewGame(char* args, Board* board) {
  (void) args;

  ParseFen(START_FEN, board);
  ThreadsClearTT();
  ThreadsClearSearch();
}

static void CmdStop(char* args, Board* board) {
  (void) args, (void) board;

  StopSearch();
}

static void CmdPonderHit(char* args, Board* board) {
  (void) args, (void) board;

  Threads.ponder = 0;
  if (Threads.stopOnPonderHit)
    Threads.stop = 1;
  pthread_mutex_lock(&Threads.lock);
  if (Threads.sleeping) {
    Threads.stop = 1;
    ThreadWake(Threads.threads[0], THREAD_RESUME);
    Threads.sleeping = 0;
  }
  pthread_mutex_unlock(&Threads.lock);
}

static void CmdBoard(char* args, Board* board) {
  (void) args;

  PrintBoard(board);
}

static void CmdCycle(char* args, Board* board) {
  (void) args;

  int cycle = HasCycle(board, MAX_SEARCH_PLY);
  printf(cycle ? "yes\n" : "no\n");
}

// perft [depth] [fen]
static void CmdPerft(char* args, Board* board) {
  char* save;
  const char* d   = strtok_r(args, " ", &save) ?: "5";
  const char* fen = strtok_r(NULL, "\0", &save) ?: START_FEN;

  Depth depth = atoi(d);
  ParseFen(fen, board);

  PerftTest(depth, board);
}

// bench [depth]
static void CmdBench(char* args, Board* board) {
  (void) board;

  char* save;
  const char* d = strtok_r(args, " ", &save);

  Bench(d ? atoi(d) : DEFAULT_BENCH_DEPTH);
}

static void CmdThreats(char* args, Board* board) {
  (void) args;

  PrintBB(board->threatened);
}

static void CmdEval(char* args, Board* board) {
  (void) args;

  EvaluateTrace(board);
}

static void CmdSee(char* args, Board* board) {
  Move m = ParseMove(args, board);
  if (m)
    printf("info string SEE result: %d\n", SEE(board, m, 0));
  else
    printf("info string Invalid move!\n");
}

static void CmdApply(char* args, Board* board) {
  Move m = ParseMove(args, board);
  if (m) {
    MakeMoveUpdate(m, board, 0);
    PrintBoard(board);
  } else
    printf("info string Invalid move!\n");
}

typedef struct {
  const char* name;
  void (*run)(char* args, Board* board); // args is the rest of the line after the command
} UCICommand;

static const UCICommand COMMANDS[] = {
  {"uci", CmdUci},
  {"isready", CmdIsReady},
  {"setoption", CmdSetOption},
  {"ucinewgame", CmdUciNewGame},
  {"position", CmdPosition},
  {"go", CmdGo},
  {"stop", CmdStop},
  {"ponderhit", CmdPonderHit},
  // non-standard
  {"board", CmdBoard},
  {"cycle", CmdCycle},
  {"perft", CmdPerft},
  {"bench", CmdBench},
  {"threats", CmdThreats},
  {"eval", CmdEval},
  {"see", CmdSee},
  {"apply", CmdApply},
#ifdef SPSA
  {"spsa", CmdSpsa},
#endif
};

#define NUM_COMMANDS ((int) (sizeof(COMMANDS) / sizeof(COMMANDS[0])))

static int ReadLine(char* in) {
  if (fgets(in, 8192, stdin) == NULL)
    return 0;

  size_t c = strcspn(in, "\r\n");
  if (c < strlen(in))
    in[c] = '\0';

  return 1;
}

void UCILoop(void) {
  static char in[8192];

  Board board;
  ParseFen(START_FEN, &board);

  setbuf(stdout, NULL);

  Threads.searching = Threads.sleeping = 0;

  while (ReadLine(in)) {
    // split "<command> <args>"
    char* cmd = in + strspn(in, " \t");
    if (!*cmd)
      continue;

    char* args = cmd + strcspn(cmd, " \t");
    if (*args)
      *args++ = '\0';
    args += strspn(args, " \t");

    if (!strcmp(cmd, "quit")) {
      StopSearch();
      break;
    }

    int i = 0;
    while (i < NUM_COMMANDS && strcmp(COMMANDS[i].name, cmd))
      i++;

    if (i < NUM_COMMANDS)
      COMMANDS[i].run(args, &board);
    else
      printf("Unknown command: %s \n", cmd);
  }

  if (Threads.searching)
    ThreadWaitUntilSleep(Threads.threads[0]);

  ThreadsExit();
}
