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

#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

int Perft(int depth, SearchData* data) {
  if (depth == 0)
    return 1;

  MoveList moveList = {.count = 0};
  GenerateAllMoves(&moveList, data);

  if (depth == 1)
    return moveList.count;

  int nodes = 0;

  for (int i = 0; i < moveList.count; i++) {
    Move move = moveList.moves[i];

    MakeMove(move, data->board);
    nodes += Perft(depth - 1, data);
    UndoMove(move, data->board);
  }

  return nodes;
}

void PerftTest(int depth, Board* board) {
  int total = 0;

  printf("\nRunning performance test to depth %d\n\n", depth);

  long startTime = GetTimeMS();

  MoveList moveList = {.count = 0};
  SearchData data = {.board = board, .ply = 0};

  GenerateAllMoves(&moveList, &data);

  for (int i = 0; i < moveList.count; i++) {
    Move move = moveList.moves[i];

    MakeMove(move, board);
    int nodes = Perft(depth - 1, &data);
    UndoMove(move, board);

    printf("%s: %d\n", MoveToStr(move), nodes);
    total += nodes;
  }

  long endTime = GetTimeMS();

  printf("\nNodes: %d\n", total);
  printf("Time: %ldms\n", (endTime - startTime));
  printf("NPS: %ld\n\n", total / (endTime - startTime) * 1000);
}