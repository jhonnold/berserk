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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "noobprobe/noobprobe.h"
#include "perft.h"
#include "pyrrhic/tbprobe.h"
#include "search.h"
#include "see.h"
#include "thread.h"
#include "transposition.h"
#include "uci.h"
#include "util.h"

#define NAME "Berserk"
#define VERSION "4.5.0-dev"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

int MOVE_OVERHEAD = 100;
int CONTEMPT = 24;

// uci "go" command
void ParseGo(char* in, SearchParams* params, Board* board, ThreadData* threads) {
  in += 3;

  params->depth = MAX_SEARCH_PLY;
  params->start = GetTimeMS();
  params->timeset = 0;
  params->stopped = 0;
  params->quit = 0;

  char* ptrChar = in;
  int perft = 0, movesToGo = 30, moveTime = -1, time = -1, inc = 0, depth = -1;

  if ((ptrChar = strstr(in, "perft")))
    perft = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "binc")) && board->side == BLACK)
    inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "winc")) && board->side == WHITE)
    inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "wtime")) && board->side == WHITE)
    time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "btime")) && board->side == BLACK)
    time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "movestogo")))
    movesToGo = atoi(ptrChar + 10);

  if ((ptrChar = strstr(in, "movetime")))
    moveTime = atoi(ptrChar + 9);

  if ((ptrChar = strstr(in, "depth")))
    depth = min(MAX_SEARCH_PLY - 1, atoi(ptrChar + 6));

  if (perft) {
    PerftTest(perft, board);
    return;
  }

  params->depth = depth;

  // "movetime" is essentially making a move with 1 to go for TC
  if (moveTime != -1) {
    params->timeset = 1;
    params->alloc = moveTime;
    params->max = moveTime;
  } else {
    if (time != -1) {
      params->timeset = 1;

      time = max(0, time - MOVE_OVERHEAD);
      int spend = time / movesToGo + inc;
      spend = max(1, spend);

      params->max = min(4 * spend, time * 0.9);
      params->alloc = min(spend, params->max / 3);
    } else {
      // no time control
      params->timeset = 0;
    }
  }

  if (depth <= 0)
    params->depth = MAX_SEARCH_PLY - 1;

  printf("info string time %d start %ld alloc %d depth %d timeset %d\n", time, params->start, params->alloc, params->depth,
         params->timeset);

  // this MUST be freed from within the search, or else massive leak
  SearchArgs* args = malloc(sizeof(SearchArgs));
  args->board = board;
  args->params = params;
  args->threads = threads;

  // start the search!
  pthread_t searchThread;
  pthread_create(&searchThread, NULL, &UCISearch, args);
  pthread_detach(searchThread);
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
  Move enteredMove;

  if (ptrChar == NULL)
    return;

  ptrChar += 6;
  while (*ptrChar) {
    enteredMove = ParseMove(ptrChar, board);
    if (!enteredMove)
      break;

    MakeMove(enteredMove, board);
    while (*ptrChar && *ptrChar != ' ')
      ptrChar++;
    ptrChar++;
  }
}

void PrintUCIOptions() {
  printf("id name " NAME " " VERSION "\n");
  printf("id author Jay Honnold\n");
  printf("option name Hash type spin default 32 min 4 max 65536\n");
  printf("option name Threads type spin default 1 min 1 max 256\n");
  printf("option name NoobBookLimit type spin default 8 min 0 max 32\n");
  printf("option name NoobBook type check default false\n");
  printf("option name SyzygyPath type string default <empty>\n");
  printf("option name Contempt type spin default 24 min 0 max 100\n");
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

  ThreadData* threads = CreatePool(1);
  SearchParams searchParameters = {.quit = 0};

  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

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
      ResetThreadPool(&board, &searchParameters, threads);
      failedQueries = 0;
    } else if (!strncmp(in, "go", 2)) {
      ParseGo(in, &searchParameters, &board, threads);
    } else if (!strncmp(in, "stop", 4)) {
      searchParameters.stopped = 1;
    } else if (!strncmp(in, "quit", 4)) {
      searchParameters.quit = 1;
      break;
    } else if (!strncmp(in, "uci", 3)) {
      PrintUCIOptions();
    } else if (!strncmp(in, "board", 5)) {
      PrintBoard(&board);
    } else if (!strncmp(in, "eval", 4)) {
      Score s = Evaluate(&board, &threads[0]);
      if (board.side == BLACK)
        s = -s;

      printf("Score: %dcp (white)\n", s);
    } else if (!strncmp(in, "moves", 5)) {
      PrintMoves(&board, threads);
    } else if (!strncmp(in, "see ", 4)) {
      Move m = ParseMove(in + 4, &board);
      if (m)
        printf("info string SEE result: %d\n", SEE(&board, m));
      else
        printf("info string Invalid move!\n");
    } else if (!strncmp(in, "apply ", 6)) {
      Move m = ParseMove(in + 6, &board);
      if (m) {
        MakeMove(m, &board);
        PrintBoard(&board);
      } else
        printf("info string Invalid move!\n");
    } else if (!strncmp(in, "setoption name Hash value ", 26)) {
      int mb = GetOptionIntValue(in);
      mb = max(4, min(65536, mb));
      size_t bytesAllocated = TTInit(mb);
      printf("info string set Hash to value %d (%zu bytes)\n", mb, bytesAllocated);
    } else if (!strncmp(in, "setoption name Threads value ", 29)) {
      int n = GetOptionIntValue(in);
      free(threads);
      threads = CreatePool(max(1, min(256, n)));
      printf("info string set Threads to value %d\n", n);
    } else if (!strncmp(in, "setoption name SyzygyPath value ", 32)) {
      int success = tb_init(in + 32);
      if (success)
        printf("info string set SyzygyPath to value %s\n", in + 32);
      else
        printf("info string FAILED!\n");
    } else if (!strncmp(in, "setoption name NoobBookLimit value ", 35)) {
      NOOB_DEPTH_LIMIT = min(32, max(0, GetOptionIntValue(in)));
      printf("info string set NoobBookLimit to value %d\n", NOOB_DEPTH_LIMIT);
    } else if (!strncmp(in, "setoption name NoobBook value ", 30)) {
      char opt[5];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      NOOB_BOOK = !strncmp(opt, "true", 4);
      printf("info string set NoobBook to value %s\n", NOOB_BOOK ? "true" : "false");
    } else if (!strncmp(in, "setoption name Contempt value ", 30)) {
      int n = GetOptionIntValue(in);

      CONTEMPT = max(0, min(100, n));
    }
  }
}

int GetOptionIntValue(char* in) {
  int n;
  sscanf(in, "%*s %*s %*s %*s %d", &n);

  return n;
}