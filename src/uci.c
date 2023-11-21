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

#include "uci.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "nn/accumulator.h"
#include "nn/evaluate.h"
#include "perft.h"
#include "pyrrhic/tbprobe.h"
#include "search.h"
#include "see.h"
#include "thread.h"
#include "transposition.h"
#include "util.h"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

extern float a0;
extern float a1;

extern float b0;
extern float b1;

extern float c0;
extern float c1;

extern float d0;

extern float e0;

extern float f0;
extern float f1;

extern float g0;
extern float g1;
extern float g2;
extern float g3;
extern float g4;

extern float h0;
extern float h1;
extern float h2;

extern int i0;
extern int i1;
extern int i2;
extern int i3;

extern int v0;
extern int v1;

extern int k0;
extern int k1;
extern int k2;
extern int k3;
extern int k4;

extern int l0;
extern int l1;

extern int m0;
extern int m1;

extern int n0;
extern int n1;
extern int n2;

extern int o0;
extern int o1;
extern int o2;
extern int o3;

extern int p0;

extern int q0;

extern int r0;

extern int s0;

extern int t0;

extern int u0;

float w0 = 0.3784;
float w1 = 0.0570;

float x0 = 0.7776;
float x1 = 5.8320;

int MOVE_OVERHEAD  = 6;
int MULTI_PV       = 1;
int PONDER_ENABLED = 0;
int CHESS_960      = 0;
int CONTEMPT       = 0;
int SHOW_WDL       = 1;

SearchParams Limits;

// All WDL work below is thanks to the work of vondele@ and
// this repo: https://github.com/vondele/WLD_model

// Third order polynomial fit of Berserk data
const double as[4] = {-5.83465749, 46.43599644, -58.49798392, 172.62328616};
const double bs[4] = {-7.95320845, 48.50833438, -66.34647240, 56.29169197};

// win% as permilli given score and ply
int WRModel(Score s, int ply) {
  double m = Min(240, ply) / 64.0;

  double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
  double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

  double x = Min(4000.0, Max(-4000.0, s));

  return 0.5 + 1000 / (1 + exp((a - x) / b));
}

void RootMoves(SimpleMoveList* moves, Board* board) {
  moves->count = 0;

  MovePicker mp;
  InitPerftMovePicker(&mp, board);

  Move mv;
  while ((mv = NextMove(&mp, board, 0)))
    moves->moves[moves->count++] = mv;
}

// uci "go" command
void ParseGo(char* in, Board* board) {
  in += 3;

  Limits.depth            = MAX_SEARCH_PLY;
  Limits.start            = GetTimeMS();
  Limits.timeset          = 0;
  Limits.stopped          = 0;
  Limits.quit             = 0;
  Limits.multiPV          = MULTI_PV;
  Limits.searchMoves      = 0;
  Limits.searchable.count = 0;
  Limits.infinite         = 0;
  Limits.mate             = 0;

  char* ptrChar = in;
  int perft = 0, movesToGo = -1, moveTime = -1, time = -1, inc = 0, depth = -1, nodes = 0, ponder = 0, mate = 0;

  SimpleMoveList rootMoves;
  RootMoves(&rootMoves, board);

  if ((ptrChar = strstr(in, "infinite")))
    Limits.infinite = 1;

  if ((ptrChar = strstr(in, "perft")))
    perft = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "binc")) && board->stm == BLACK)
    inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "winc")) && board->stm == WHITE)
    inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "wtime")) && board->stm == WHITE)
    time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "btime")) && board->stm == BLACK)
    time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "movestogo")))
    movesToGo = Max(1, Min(50, atoi(ptrChar + 10)));

  if ((ptrChar = strstr(in, "movetime")))
    moveTime = atoi(ptrChar + 9);

  if ((ptrChar = strstr(in, "depth")))
    depth = Min(MAX_SEARCH_PLY - 1, atoi(ptrChar + 6));

  if ((ptrChar = strstr(in, "mate")))
    mate = Min(MAX_SEARCH_PLY - 1, atoi(ptrChar + 5));

  if ((ptrChar = strstr(in, "nodes")))
    nodes = Max(1, atol(ptrChar + 6));

  if ((ptrChar = strstr(in, "ponder"))) {
    if (PONDER_ENABLED)
      ponder = 1;
    else {
      printf("info string Enable option Ponder to use 'ponder'\n");
      return;
    }
  }

  if ((ptrChar = strstr(in, "searchmoves"))) {
    Limits.searchMoves = 1;

    for (char* moves = strtok(ptrChar + 12, " "); moves != NULL; moves = strtok(NULL, " "))
      for (int i = 0; i < rootMoves.count; i++)
        if (!strcmp(MoveToStr(rootMoves.moves[i], board), moves))
          Limits.searchable.moves[Limits.searchable.count++] = rootMoves.moves[i];
  }

  if (perft) {
    PerftTest(perft, board);
    return;
  }

  Limits.depth = depth;
  Limits.mate  = mate;
  Limits.nodes = nodes;

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

        Limits.alloc = Min(time * w0, total * w1);
        Limits.max   = Min(time * x0 - MOVE_OVERHEAD, Limits.alloc * x1) - 10;
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
void ParsePosition(char* in, Board* board) {
  in += 9;
  char* ptrChar = in;

  if (strncmp(in, "startpos", 8) == 0) {
    ParseFen(START_FEN, board);
  } else {
    ptrChar = strstr(in, "fen");
    if (ptrChar == NULL)
      ParseFen(START_FEN, board);
    else {
      ptrChar += 4;
      ParseFen(ptrChar, board);
    }
  }

  ptrChar = strstr(in, "moves");

  if (ptrChar == NULL)
    return;

  ptrChar += 6;

  for (char* moves = strtok(ptrChar, " "); moves != NULL; moves = strtok(NULL, " ")) {
    Move enteredMove = ParseMove(moves, board);
    if (!enteredMove)
      break;

    MakeMoveUpdate(enteredMove, board, 0);

    if (board->fmr == 0)
      board->histPly = board->nullply = 0;
  }
}

void PrintUCIOptions() {
  printf("id name Berserk " VERSION "\n");
  printf("id author Jay Honnold\n");
  printf("option name Hash type spin default 16 min 2 max %d\n", HASH_MAX);
  printf("option name Threads type spin default 1 min 1 max 256\n");
  printf("option name SyzygyPath type string default <empty>\n");
  printf("option name MultiPV type spin default 1 min 1 max 256\n");
  printf("option name Ponder type check default false\n");
  printf("option name UCI_ShowWDL type check default true\n");
  printf("option name UCI_Chess960 type check default false\n");
  printf("option name MoveOverhead type spin default 6 min 0 max 10000\n");
  printf("option name Contempt type spin default 0 min -100 max 100\n");
  printf("option name EvalFile type string default <empty>\n");

  printf("option name a0 type string\n");
  printf("option name a1 type string\n");
  printf("option name b0 type string\n");
  printf("option name b1 type string\n");
  printf("option name c0 type string\n");
  printf("option name c1 type string\n");
  printf("option name d0 type string\n");
  printf("option name e0 type string\n");
  printf("option name f0 type string\n");
  printf("option name f1 type string\n");
  printf("option name g0 type string\n");
  printf("option name g1 type string\n");
  printf("option name g2 type string\n");
  printf("option name g3 type string\n");
  printf("option name g4 type string\n");
  printf("option name h0 type string\n");
  printf("option name h1 type string\n");
  printf("option name h2 type string\n");
  printf("option name i0 type string\n");
  printf("option name i1 type string\n");
  printf("option name i2 type string\n");
  printf("option name i3 type string\n");
  printf("option name v0 type string\n");
  printf("option name v1 type string\n");
  printf("option name k0 type string\n");
  printf("option name k1 type string\n");
  printf("option name k2 type string\n");
  printf("option name k3 type string\n");
  printf("option name k4 type string\n");
  printf("option name l0 type string\n");
  printf("option name l1 type string\n");
  printf("option name m0 type string\n");
  printf("option name m1 type string\n");
  printf("option name n0 type string\n");
  printf("option name n1 type string\n");
  printf("option name n2 type string\n");
  printf("option name o0 type string\n");
  printf("option name o1 type string\n");
  printf("option name o2 type string\n");
  printf("option name o3 type string\n");
  printf("option name p0 type string\n");
  printf("option name q0 type string\n");
  printf("option name r0 type string\n");
  printf("option name s0 type string\n");
  printf("option name t0 type string\n");
  printf("option name u0 type string\n");
  printf("option name w0 type string\n");
  printf("option name w1 type string\n");
  printf("option name x0 type string\n");
  printf("option name x1 type string\n");

  printf("uciok\n");
}

int ReadLine(char* in) {
  if (fgets(in, 8192, stdin) == NULL)
    return 0;

  size_t c = strcspn(in, "\r\n");
  if (c < strlen(in))
    in[c] = '\0';

  return 1;
}

void UCILoop() {
  static char in[8192];

  Board board;
  ParseFen(START_FEN, &board);

  setbuf(stdout, NULL);

  Threads.searching = Threads.sleeping = 0;

  while (ReadLine(in)) {
    if (in[0] == '\n')
      continue;

    if (!strncmp(in, "isready", 7)) {
      printf("readyok\n");
    } else if (!strncmp(in, "position", 8)) {
      ParsePosition(in, &board);
    } else if (!strncmp(in, "ucinewgame", 10)) {
      ParsePosition("position startpos\n", &board);
      TTClear();
      SearchClear();
    } else if (!strncmp(in, "go", 2)) {
      ParseGo(in, &board);
    } else if (!strncmp(in, "stop", 4)) {
      if (Threads.searching) {
        Threads.stop = 1;
        pthread_mutex_lock(&Threads.lock);
        if (Threads.sleeping)
          ThreadWake(Threads.threads[0], THREAD_RESUME);
        Threads.sleeping = 0;
        pthread_mutex_unlock(&Threads.lock);
      }
    } else if (!strncmp(in, "quit", 4)) {
      if (Threads.searching) {
        Threads.stop = 1;
        pthread_mutex_lock(&Threads.lock);
        if (Threads.sleeping)
          ThreadWake(Threads.threads[0], THREAD_RESUME);
        Threads.sleeping = 0;
        pthread_mutex_unlock(&Threads.lock);
      }
      break;
    } else if (!strncmp(in, "uci", 3)) {
      PrintUCIOptions();
    } else if (!strncmp(in, "ponderhit", 9)) {
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
    } else if (!strncmp(in, "board", 5)) {
      PrintBoard(&board);
    } else if (!strncmp(in, "cycle", 5)) {
      int cycle = HasCycle(&board, MAX_SEARCH_PLY);
      printf(cycle ? "yes\n" : "no\n");
    } else if (!strncmp(in, "perft", 5)) {
      strtok(in, " ");
      char* d   = strtok(NULL, " ") ?: "5";
      char* fen = strtok(NULL, "\0") ?: START_FEN;

      int depth = atoi(d);
      ParseFen(fen, &board);

      PerftTest(depth, &board);
    } else if (!strncmp(in, "bench", 5)) {
      strtok(in, " ");
      char* d = strtok(NULL, " ") ?: "13";

      int depth = atoi(d);
      Bench(depth);
    } else if (!strncmp(in, "threats", 7)) {
      PrintBB(board.easyCapture);
      PrintBB(board.threatened);
    } else if (!strncmp(in, "eval", 4)) {
      EvaluateTrace(&board);
    } else if (!strncmp(in, "see ", 4)) {
      Move m = ParseMove(in + 4, &board);
      if (m)
        printf("info string SEE result: %d\n", SEE(&board, m, 0));
      else
        printf("info string Invalid move!\n");
    } else if (!strncmp(in, "apply ", 6)) {
      Move m = ParseMove(in + 6, &board);
      if (m) {
        MakeMoveUpdate(m, &board, 0);
        PrintBoard(&board);
      } else
        printf("info string Invalid move!\n");
    } else if (!strncmp(in, "setoption name Hash value ", 26)) {
      int mb                  = GetOptionIntValue(in);
      mb                      = Max(2, Min(HASH_MAX, mb));
      uint64_t bytesAllocated = TTInit(mb);
      uint64_t totalEntries   = BUCKET_SIZE * bytesAllocated / sizeof(TTBucket);
      printf("info string set Hash to value %d (%" PRIu64 " bytes) (%" PRIu64 " entries)\n",
             mb,
             bytesAllocated,
             totalEntries);
    } else if (!strncmp(in, "setoption name Threads value ", 29)) {
      int n = GetOptionIntValue(in);
      ThreadsSetNumber(Max(1, Min(256, n)));
      printf("info string set Threads to value %d\n", Threads.count);
    } else if (!strncmp(in, "setoption name SyzygyPath value ", 32)) {
      int success = tb_init(in + 32);
      if (success)
        printf("info string set SyzygyPath to value %s\n", in + 32);
      else
        printf("info string FAILED!\n");
    } else if (!strncmp(in, "setoption name MultiPV value ", 29)) {
      int n = GetOptionIntValue(in);

      MULTI_PV = Max(1, Min(256, n));
      printf("info string set MultiPV to value %d\n", MULTI_PV);
    } else if (!strncmp(in, "setoption name Ponder value ", 28)) {
      char opt[6];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      PONDER_ENABLED = !strncmp(opt, "true", 4);
      printf("info string set Ponder to value %s\n", PONDER_ENABLED ? "true" : "false");
    } else if (!strncmp(in, "setoption name UCI_ShowWDL value ", 33)) {
      char opt[6];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      SHOW_WDL = !strncmp(opt, "true", 4);
      printf("info string set SHOW_WDL to value %s\n", SHOW_WDL ? "true" : "false");
    } else if (!strncmp(in, "setoption name UCI_Chess960 value ", 34)) {
      char opt[6];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      CHESS_960 = !strncmp(opt, "true", 4);
      printf("info string set UCI_Chess960 to value %s\n", CHESS_960 ? "true" : "false");
      printf("info string Resetting board...\n");

      ParsePosition("position startpos\n", &board);
      TTClear();
      SearchClear();
    } else if (!strncmp(in, "setoption name MoveOverhead value ", 34)) {
      MOVE_OVERHEAD = Min(10000, Max(0, GetOptionIntValue(in)));
    } else if (!strncmp(in, "setoption name Contempt value ", 30)) {
      CONTEMPT = Min(100, Max(-100, GetOptionIntValue(in)));
    } else if (!strncmp(in, "setoption name EvalFile value ", 30)) {
      char* path  = in + 30;
      int success = 0;

      if (strncmp(path, "<empty>", 7))
        success = LoadNetwork(path);
      else {
        LoadDefaultNN();
        success = 1;
      }

      if (success)
        printf("info string set EvalFile to value %s\n", path);
    } else if (!strncmp(in, "setoption name Contempt value ", 30)) {
      CONTEMPT = Min(100, Max(-100, GetOptionIntValue(in)));
    }

    FO(a0)
    FO(a1)
    FO(b0)
    FO(b1)
    FO(c0)
    FO(c1)
    FO(d0)
    FO(e0)
    FO(f0)
    FO(f1)
    FO(g0)
    FO(g1)
    FO(g2)
    FO(g3)
    FO(g4)
    FO(h0)
    FO(h1)
    FO(h2)
    IO(i0)
    IO(i1)
    IO(i2)
    IO(i3)
    IO(v0)
    IO(v1)
    IO(k0)
    IO(k1)
    IO(k2)
    IO(k3)
    IO(k4)
    IO(l0)
    IO(l1)
    IO(m0)
    IO(m1)
    IO(n0)
    IO(n1)
    IO(n2)
    IO(o0)
    IO(o1)
    IO(o2)
    IO(o3)
    IO(p0)
    IO(q0)
    IO(r0)
    IO(s0)
    IO(t0)
    IO(u0)
    FO(w0)
    FO(w1)
    FO(x0)
    FO(x1)

    else
      printf("Unknown command: %s \n", in);

    InitPruningAndReductionTables();
  }

  if (Threads.searching)
    ThreadWaitUntilSleep(Threads.threads[0]);

  pthread_mutex_destroy(&Threads.lock);
  ThreadsExit();
}

int GetOptionIntValue(char* in) {
  int n;
  sscanf(in, "%*s %*s %*s %*s %d", &n);

  return n;
}

float GetOptionFValue(char* in) {
  float n;
  sscanf(in, "%*s %*s %*s %*s %f", &n);

  return n;
}
