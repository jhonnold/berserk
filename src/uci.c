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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "transposition.h"
#include "uci.h"
#include "util.h"

#define NAME "Berserk"
#define VERSION "3.2.1"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// uci "go" command
void ParseGo(char* in, SearchParams* params, Board* board) {
  in += 3;

  params->depth = MAX_SEARCH_PLY;
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

  // "movetime" is essentially making a move with 1 to go for TC
  if (moveTime != -1) {
    time = moveTime;
    movesToGo = 1;
  }

  if (perft)
    PerftTest(perft, board);
  else {
    params->startTime = GetTimeMS();
    params->depth = depth;

    if (time != -1) {
      // Non-infinite analysis
      // Berserk has a very simple algorithm of
      // 1/30th clocktime - 30ms (buffer) + increment
      // TODO: Improve this, most likely in Search
      params->timeset = 1;
      time /= movesToGo;
      time -= 30;
      time += inc;
      params->endTime = params->startTime + time;
    } else {
      params->endTime = 0;
    }

    if (depth <= 0)
      params->depth = MAX_SEARCH_PLY - 1;

    printf("time %d start %ld stop %ld depth %d timeset %d\n", time, params->startTime, params->endTime, params->depth,
           params->timeset);

    // Data is constructed outside of search so other tools can
    // collect and display search related data.
    // TODO: Actually use this for functionality outside of "bench"
    SearchData data = {.board = board};
    Search(params, &data);
  }
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

  if (ptrChar == NULL) {
    PrintBoard(board);
    return;
  }

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

  PrintBoard(board);
}

void UCILoop() {
  static char in[8192];

  Board board;
  ParseFen(START_FEN, &board);

  SearchParams searchParameters = {.quit = 0};

  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  printf("id name " NAME " " VERSION "\n");
  printf("id author Jay Honnold\n");
  printf("option name Hash type spin default 32 min 4 max 4096\n");
  printf("option name Threads type spin default 1 min 1 max 1\n"); // Currently unused
  printf("uciok\n");

  while (!searchParameters.quit) {
    memset(&in[0], 0, sizeof(in));

    fflush(stdout);

    if (!fgets(in, 8192, stdin))
      continue;

    if (in[0] == '\n')
      continue;

    if (!strncmp(in, "isready", 7)) {
      printf("readyok\n");
    } else if (!strncmp(in, "position", 8)) {
      ParsePosition(in, &board);
    } else if (!strncmp(in, "ucinewgame", 10)) {
      ParsePosition("position startpos\n", &board);
    } else if (!strncmp(in, "go", 2)) {
      ParseGo(in, &searchParameters, &board);
    } else if (!strncmp(in, "quit", 4)) {
      searchParameters.quit = 1;
    } else if (!strncmp(in, "uci", 3)) {
      printf("id name " NAME " " VERSION "\n");
      printf("id author Jay Honnold\n");
      printf("option name Hash type spin default 32 min 4 max 4096\n");
      printf("option name Threads type spin default 1 min 1 max 1\n"); // This is not actually used!
      printf("uciok\n");
    } else if (!strncmp(in, "eval", 4)) {
      PrintEvaluation(&board);
    } else if (!strncmp(in, "moves", 5)) {
      // Print possible moves. If a search has been run and stopped, it will
      // print the moves in the order in which they would be searched on the
      // next iteration. This has been very helpful to debug move ordering
      // related problems.

      MoveList moveList = {.count = 0};
      SearchData data = {.board = &board, .ply = 0};

      GenerateAllMoves(&moveList, &data);
      PrintMoves(&moveList);
    } else if (!strncmp(in, "setoption name Hash value ", 26)) {
      int mb;
      sscanf(in, "%*s %*s %*s %*s %d", &mb);

      mb = max(4, min(4096, mb));
      TTInit(mb);
    } else if (!strncmp(in, "board", 5)) {
      PrintBoard(&board);
    }
  }
}