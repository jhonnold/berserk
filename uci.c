#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "uci.h"

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// thanks to vice for this
void parseGo(char* in, Board* board) {
  in += 3;

  char* ptrChar = in;
  int perft = 0;

  if ((ptrChar = strstr(in, "perft")))
    perft = atoi(ptrChar + 6);

  if (perft)
    PerftTest(perft, board);
  else {
    Move bestMove = 0;
    Search(board, &bestMove);

    printf("bestmove %s\n", moveStr(bestMove));
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

  printf("id name Berserk 0.1.0\n");
  printf("id author Jay Honnold\n");
  printf("uciok\n");

  while (1) {
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
      parseGo(in, board);
    } else if (!strncmp(in, "quit", 4)) {
      break;
    } else if (!strncmp(in, "uci", 3)) {
      printf("id name Berserk 0.1.0\n");
      printf("id author Jay Honnold\n");
      printf("uciok\n");
    } else if (!strncmp(in, "eval", 4)) {
      printf("score cp %d\n", TraceEvaluate(board));
    }
  }
}