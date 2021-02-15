#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "uci.h"
#include "movegen.h"
#include "perft.h"

#define START_FEN  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// thanks to vice for this
void parseGo(char* in) {
  in += 3;

  char *ptrChar = in;
  int perft = 0;

  if ((ptrChar = strstr(in, "perft")))
    perft = atoi(ptrChar + 6);

  if (perft)
    Perft(perft);
}

void parsePosition(char* in) {
  in += 9;
  char *ptrChar = in;

  if (strncmp(in, "startpos", 8) == 0) {
    parseFen(START_FEN);
  } else {
    ptrChar = strstr(in, "fen");
    if (ptrChar == NULL)
      parseFen(START_FEN);
    else {
      ptrChar += 4;
      parseFen(ptrChar);
    }
  }

  ptrChar = strstr(in, "moves");
  move_t enteredMove;

  if (ptrChar == NULL) {
    printBoard();
    return;
  }

  ptrChar += 6;
  while (*ptrChar) {
    enteredMove = parseMove(ptrChar);
    if (!enteredMove) break;

    makeMove(enteredMove);
    while (*ptrChar && *ptrChar != ' ')
      ptrChar++;
    ptrChar++;
  }

  printBoard();
}


void uciLoop() {
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
      parsePosition(in);
    } else if (!strncmp(in, "ucinewgame", 10)) {
      parsePosition("position startpos\n");
    } else if (!strncmp(in, "go", 2)) {
      parseGo(in);
    } else if (!strncmp(in, "quit", 4)) {
      break;
    } else if (!strncmp(in, "uci", 3)) {
      printf("id name Berserk 0.1.0\n");
      printf("id author Jay Honnold\n");
      printf("uciok\n");
    }
  }
}