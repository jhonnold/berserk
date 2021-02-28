#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "uci.h"
#include "util.h"

#define NAME "Berserk"
#ifdef POPCOUNT
#define VERSION "2.0.0-alpha.rc3+popcnt"
#else
#define VERSION "2.0.0-alpha.rc3"
#endif

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// thanks to vice for this
void parseGo(char* in, SearchParams* params, Board* board) {
  in += 3;

  params->depth = 64;
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
    depth = atoi(ptrChar + 6);

  if (moveTime != -1) {
    time = moveTime;
    movesToGo = 1;
  }

  if (perft)
    PerftTest(perft, board);
  else {
    params->startTime = getTimeMs();
    params->depth = depth;

    if (time != -1) {
      params->timeset = 1;
      time /= movesToGo;
      time -= 50;
      time += inc;
      params->endTime = params->startTime + time;
    } else {
      params->endTime = 0;
    }

    if (depth == -1)
      params->depth = 64;

    printf("time %d start %ld stop %ld depth %d timeset %d\n", time, params->startTime, params->endTime, params->depth,
           params->timeset);

    SearchData data[1];
    Search(board, params, data);
  }
}

void parsePosition(char* in, Board* board) {
  in += 9;
  char* ptrChar = in;

  if (strncmp(in, "startpos", 8) == 0) {
    parseFen(START_FEN, board);
  } else {
    ptrChar = strstr(in, "fen");
    if (ptrChar == NULL)
      parseFen(START_FEN, board);
    else {
      ptrChar += 4;
      parseFen(ptrChar, board);
    }
  }

  ptrChar = strstr(in, "moves");
  Move enteredMove;

  if (ptrChar == NULL) {
    printBoard(board);
    return;
  }

  ptrChar += 6;
  while (*ptrChar) {
    enteredMove = parseMove(ptrChar, board);
    if (!enteredMove)
      break;

    makeMove(enteredMove, board);
    while (*ptrChar && *ptrChar != ' ')
      ptrChar++;
    ptrChar++;
  }

  printBoard(board);
}

void UCI(Board* board) {
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  char in[2048];
  SearchParams searchParams[1];
  searchParams->quit = 0;

  printf("id name " NAME " " VERSION "\n");
  printf("id author Jay Honnold\n");
  printf("uciok\n");

  while (!searchParams->quit) {
    memset(&in[0], 0, sizeof(in));

    fflush(stdout);

    if (!fgets(in, 2048, stdin))
      continue;

    if (in[0] == '\n')
      continue;

    if (!strncmp(in, "isready", 7)) {
      printf("readyok\n");
      continue;
    } else if (!strncmp(in, "position", 8)) {
      parsePosition(in, board);
    } else if (!strncmp(in, "ucinewgame", 10)) {
      parsePosition("position startpos\n", board);
    } else if (!strncmp(in, "go", 2)) {
      parseGo(in, searchParams, board);
    } else if (!strncmp(in, "quit", 4)) {
      searchParams->quit = 1;
    } else if (!strncmp(in, "uci", 3)) {
      printf("id name " NAME " " VERSION "\n");
      printf("id author Jay Honnold\n");
      printf("uciok\n");
    } else if (!strncmp(in, "eval", 4)) {
      printf("score cp %d\n", Evaluate(board));
    } else if (!strncmp(in, "moves", 5)) {
      MoveList moveList[1];
      generateMoves(moveList, board, 0);
      printMoves(moveList);
    }
  }
}