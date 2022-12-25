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

#include "uci.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "nn.h"
#include "perft.h"
#include "pyrrhic/tbprobe.h"
#include "search.h"
#include "see.h"
#include "thread.h"
#include "transposition.h"
#include "util.h"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

int MOVE_OVERHEAD      = 300;
int MULTI_PV           = 1;
int PONDER_ENABLED     = 0;
int CHESS_960          = 0;
int CONTEMPT           = 12;
volatile int PONDERING = 0;

void RootMoves(SimpleMoveList* moves, Board* board) {
  moves->count = 0;

  MoveList m;
  InitPerftMoves(&m, board);

  Move mv;
  while ((mv = NextMove(&m, board, 0))) moves->moves[moves->count++] = mv;
}

// uci "go" command
void ParseGo(char* in, SearchParams* params, Board* board) {
  in += 3;

  params->depth            = MAX_SEARCH_PLY;
  params->start            = GetTimeMS();
  params->timeset          = 0;
  params->stopped          = 0;
  params->quit             = 0;
  params->multiPV          = MULTI_PV;
  params->searchMoves      = 0;
  params->searchable.count = 0;

  PONDERING = 0;

  char* ptrChar = in;
  int perft = 0, movesToGo = -1, moveTime = -1, time = -1, inc = 0, depth = -1, nodes = 0;

  SimpleMoveList rootMoves;
  RootMoves(&rootMoves, board);

  if ((ptrChar = strstr(in, "perft"))) perft = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "binc")) && board->stm == BLACK) inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "winc")) && board->stm == WHITE) inc = atoi(ptrChar + 5);

  if ((ptrChar = strstr(in, "wtime")) && board->stm == WHITE) time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "btime")) && board->stm == BLACK) time = atoi(ptrChar + 6);

  if ((ptrChar = strstr(in, "movestogo"))) movesToGo = max(1, min(50, atoi(ptrChar + 10)));

  if ((ptrChar = strstr(in, "movetime"))) moveTime = atoi(ptrChar + 9);

  if ((ptrChar = strstr(in, "depth"))) depth = min(MAX_SEARCH_PLY - 1, atoi(ptrChar + 6));

  if ((ptrChar = strstr(in, "nodes"))) nodes = max(1, atol(ptrChar + 6));

  if ((ptrChar = strstr(in, "ponder"))) {
    if (PONDER_ENABLED)
      PONDERING = 1;
    else {
      printf("info string Enable option Ponder to use 'ponder'\n");
      return;
    }
  }

  if ((ptrChar = strstr(in, "searchmoves"))) {
    params->searchMoves = 1;

    for (char* moves = strtok(ptrChar + 12, " "); moves != NULL; moves = strtok(NULL, " "))
      for (int i = 0; i < rootMoves.count; i++)
        if (!strcmp(MoveToStr(rootMoves.moves[i], board), moves))
          params->searchable.moves[params->searchable.count++] = rootMoves.moves[i];
  }

  if (perft) {
    PerftTest(perft, board);
    return;
  }

  params->depth = depth;
  params->nodes = nodes;

  if (params->nodes)
    params->hitrate = min(1000, max(1, params->nodes / 100));
  else
    params->hitrate = 1000;

  // "movetime" is essentially making a move with 1 to go for TC
  if (moveTime != -1) {
    params->timeset = 1;
    params->alloc   = moveTime;
    params->max     = moveTime;
  } else {
    if (time != -1) {
      params->timeset = 1;

      if (movesToGo == -1) {
        int total = max(1, time + 50 * inc - MOVE_OVERHEAD);

        params->alloc = min(time * 0.33, total / 20.0);
      } else {
        int total = max(1, time + movesToGo * inc - MOVE_OVERHEAD);

        params->alloc = min(time * 0.9, (0.9 * total) / max(1, movesToGo / 2.5));
      }

      params->max = min(time * 0.75, params->alloc * 5.5);
    } else {
      // no time control
      params->timeset = 0;
      params->max     = INT_MAX;
    }
  }

  params->multiPV = min(params->multiPV, params->searchMoves ? params->searchable.count : rootMoves.count);
  if (rootMoves.count == 1 && params->timeset) params->max = min(250, params->max);

  if (depth <= 0) params->depth = MAX_SEARCH_PLY - 1;

  printf("info string time %d start %ld alloc %d max %d depth %d timeset %d searchmoves %d\n",
         time,
         params->start,
         params->alloc,
         params->max,
         params->depth,
         params->timeset,
         params->searchable.count);

  // this MUST be freed from within the search, or else massive leak
  SearchArgs* args = malloc(sizeof(SearchArgs));
  args->board      = board;
  args->params     = params;

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

  if (ptrChar == NULL) return;

  ptrChar += 6;

  for (char* moves = strtok(ptrChar, " "); moves != NULL; moves = strtok(NULL, " ")) {
    Move enteredMove = ParseMove(moves, board);
    if (!enteredMove) break;

    MakeMoveUpdate(enteredMove, board, 0);

    if (board->halfMove == 0) board->histPly = 0;
  }
}

void PrintUCIOptions() {
  printf("id name Berserk " VERSION "\n");
  printf("id author Jay Honnold\n");
  printf("option name Hash type spin default 16 min 2 max 131072\n");
  printf("option name Threads type spin default 1 min 1 max 256\n");
  printf("option name SyzygyPath type string default <empty>\n");
  printf("option name MultiPV type spin default 1 min 1 max 256\n");
  printf("option name Ponder type check default false\n");
  printf("option name UCI_Chess960 type check default false\n");
  printf("option name MoveOverhead type spin default 300 min 100 max 10000\n");
  printf("option name Contempt type spin default 12 min -100 max 100\n");
  printf("option name EvalFile type string default <empty>\n");
  printf("uciok\n");
}

int ReadLine(char* in) {
  if (fgets(in, 8192, stdin) == NULL) return 0;

  size_t c = strcspn(in, "\r\n");
  if (c < strlen(in)) in[c] = '\0';

  return 1;
}

void UCILoop() {
  static char in[8192];

  Board board;
  ParseFen(START_FEN, &board);

  SearchParams searchParameters = {.quit = 0};

  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  while (ReadLine(in)) {
    if (in[0] == '\n') continue;

    if (!strncmp(in, "isready", 7)) {
      printf("readyok\n");
    } else if (!strncmp(in, "position", 8)) {
      ParsePosition(in, &board);
    } else if (!strncmp(in, "ucinewgame", 10)) {
      ParsePosition("position startpos\n", &board);
      TTClear();
      ResetThreadPool();
    } else if (!strncmp(in, "go", 2)) {
      ParseGo(in, &searchParameters, &board);
    } else if (!strncmp(in, "stop", 4)) {
      PONDERING                = 0;
      searchParameters.stopped = 1;
    } else if (!strncmp(in, "quit", 4)) {
      PONDERING             = 0;
      searchParameters.quit = 1;
      break;
    } else if (!strncmp(in, "uci", 3)) {
      PrintUCIOptions();
    } else if (!strncmp(in, "ponderhit", 9)) {
      PONDERING = 0;
    } else if (!strncmp(in, "board", 5)) {
      PrintBoard(&board);
    } else if (!strncmp(in, "threats", 7)) {
      Threat threats[1];
      Threats(threats, &board, board.stm);

      PrintBB(threats->pcs);
      PrintBB(threats->sqs);
    } else if (!strncmp(in, "eval", 4)) {
      board.acc = 0;

      board.accumulators[WHITE] = threads->accumulators[WHITE];
      board.accumulators[BLACK] = threads->accumulators[BLACK];

      ResetAccumulator(board.accumulators[WHITE][0], &board, WHITE);
      ResetAccumulator(board.accumulators[BLACK][0], &board, BLACK);

      int score = Evaluate(&board, threads);
      score     = board.stm == WHITE ? score : -score;

      for (int r = 0; r < 8; r++) {
        printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
        printf("|");
        for (int f = 0; f < 16; f++) {
          if (f == 8) printf("\n|");

          int sq = r * 8 + (f > 7 ? f - 8 : f);

          if (f < 8) {
            if (board.squares[sq] == NO_PIECE)
              printf("       |");
            else
              printf("   %c   |", PIECE_TO_CHAR[board.squares[sq]]);
          } else if (board.squares[sq] < WHITE_KING) {
            popBit(board.occupancies[BOTH], sq);

            ResetAccumulator(board.accumulators[WHITE][0], &board, WHITE);
            ResetAccumulator(board.accumulators[BLACK][0], &board, BLACK);

            int new = Evaluate(&board, threads);
            new     = board.stm == WHITE ? new : -new;

            int diff = score - new;
            printf("%+7d|", diff);
            setBit(board.occupancies[BOTH], sq);
          } else {
            printf("       |");
          }
        }
        printf("\n");
      }
      printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");

      printf("Score: %dcp (white)\n", score);
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
      mb                      = max(2, min(131072, mb));
      uint64_t bytesAllocated = TTInit(mb);
      printf("info string set Hash to value %d (%" PRIu64 " bytes)\n", mb, bytesAllocated);
    } else if (!strncmp(in, "setoption name Threads value ", 29)) {
      int n = GetOptionIntValue(in);
      CreatePool(max(1, min(256, n)));
      printf("info string set Threads to value %d\n", n);
    } else if (!strncmp(in, "setoption name SyzygyPath value ", 32)) {
      int success = tb_init(in + 32);
      if (success)
        printf("info string set SyzygyPath to value %s\n", in + 32);
      else
        printf("info string FAILED!\n");
    } else if (!strncmp(in, "setoption name MultiPV value ", 29)) {
      int n = GetOptionIntValue(in);

      MULTI_PV = max(1, min(256, n));
      printf("info string set MultiPV to value %d\n", MULTI_PV);
    } else if (!strncmp(in, "setoption name Ponder value ", 28)) {
      char opt[5];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      PONDER_ENABLED = !strncmp(opt, "true", 4);
      printf("info string set Ponder to value %s\n", PONDER_ENABLED ? "true" : "false");
    } else if (!strncmp(in, "setoption name UCI_Chess960 value ", 34)) {
      char opt[5];
      sscanf(in, "%*s %*s %*s %*s %5s", opt);

      CHESS_960 = !strncmp(opt, "true", 4);
      printf("info string set UCI_Chess960 to value %s\n", CHESS_960 ? "true" : "false");
      printf("info string Resetting board...\n");

      ParsePosition("position startpos\n", &board);
      TTClear();
      ResetThreadPool();
    } else if (!strncmp(in, "setoption name MoveOverhead value ", 34)) {
      MOVE_OVERHEAD = min(10000, max(100, GetOptionIntValue(in)));
    } else if (!strncmp(in, "setoption name Contempt value ", 30)) {
      CONTEMPT = min(100, max(-100, GetOptionIntValue(in)));
    } else if (!strncmp(in, "setoption name EvalFile value ", 30)) {
      char* path  = in + 30;
      int success = 0;

      if (strncmp(path, "<empty>", 7))
        success = LoadNetwork(path);
      else {
        LoadDefaultNN();
        success = 1;
      }

      if (success) printf("info string set EvalFile to value %s\n", path);
    }
  }
}

int GetOptionIntValue(char* in) {
  int n;
  sscanf(in, "%*s %*s %*s %*s %d", &n);

  return n;
}