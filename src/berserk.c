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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "argparse.h"
#include "attacks.h"
#include "bench.h"
#include "bits.h"
#include "nn/accumulator.h"
#include "nn/evaluate.h"
#include "fengen/fengen.h"
#include "random.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

// Welcome to berserk
int main(int argc, char** argv) {
  SeedRandom(time(NULL));
  InitZobristKeys();
  InitPruningAndReductionTables();
  InitAttacks();
  InitCuckoo();

  LoadDefaultNN();
  ThreadsInit();
  TTInit(16);

  // Set all defaults
  fenGenParams.book = NULL;
  fenGenParams.network = NULL;
  fenGenParams.dir = ".";
  fenGenParams.file_prefix = "";
  fenGenParams.evalLimit = 1500;
  fenGenParams.nodes = 5000;
  fenGenParams.depth = 0;
  fenGenParams.randomMoveCount = 5;
  fenGenParams.randomMoveMax = 24;
  fenGenParams.randomMoveMin = 1;
  fenGenParams.randomMpv = 4;
  fenGenParams.randomMpvDiff = 50;
  fenGenParams.writeMax = 400;
  fenGenParams.writeMin = 16;
  fenGenParams.filterDuplicates = 0;

  int threads    = 1;
  uint64_t total = 1024 * 1024 * 1024;

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_GROUP("Top Level Options"),
    OPT_INTEGER(0, "threads", &threads, "Threads to run on", NULL, 0, 0),
    OPT_INTEGER(0, "total", &total, "Total positions to generate", NULL, 0, 0),
    OPT_BOOLEAN(0, "filter-duplicates", &fenGenParams.filterDuplicates, "Filter duplicates?", NULL, 0, 0),
    OPT_BOOLEAN(0, "chess960", &CHESS_960, "Fischer random?", NULL, 0, 0),
    OPT_STRING(0, "book", &fenGenParams.book, "EPD book", NULL, 0, 0),
    OPT_STRING(0, "network", &fenGenParams.network, "Network to use when generating data", NULL, 0, 0),
    OPT_STRING(0, "output", &fenGenParams.dir, "Output directory", NULL, 0, 0),
    OPT_STRING(0, "file_name", &fenGenParams.file_prefix, "File names prefix", NULL, 0, 0),

    OPT_GROUP("Search Level Options"),
    OPT_INTEGER(0, "nodes", &fenGenParams.nodes, "Min nodes", NULL, 0, 0),
    OPT_INTEGER(0, "depth", &fenGenParams.depth, "Max depth", NULL, 0, 0),

    OPT_GROUP("Randomization Options"),
    OPT_INTEGER(0, "random-move-count", &fenGenParams.randomMoveCount, "Number of random moves", NULL, 0, 0),
    OPT_INTEGER(0, "random-move-min", &fenGenParams.randomMoveMin, "Min ply for random moves", NULL, 0, 0),
    OPT_INTEGER(0, "random-move-max", &fenGenParams.randomMoveMax, "Max ply for random moves", NULL, 0, 0),
    OPT_INTEGER(0, "random-multipv", &fenGenParams.randomMpv, "Number of MPV for randomization", NULL, 0, 0),
    OPT_INTEGER(0, "random-multipv-diff", &fenGenParams.randomMpvDiff, "Selection range for MPV randomization", NULL, 0, 0),

    OPT_GROUP("Write Options"),
    OPT_INTEGER(0, "eval-limit", &fenGenParams.evalLimit, "Eval max for game cutoff", NULL, 0, 0),
    OPT_INTEGER(0, "write-min", &fenGenParams.writeMin, "Write min ply", NULL, 0, 0),
    OPT_INTEGER(0, "write-max", &fenGenParams.writeMax, "Write max ply", NULL, 0, 0),

    OPT_END()
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  ThreadsSetNumber(threads);

  if (fenGenParams.network) {
    if (!LoadNetwork(fenGenParams.network))
      exit(EXIT_FAILURE);
    else
      printf("Successfully set network to: %s\n", fenGenParams.network);
  }

  printf("Executing generator with the following:\n");
  printf("Threads: %d\n", threads);
  printf("Total: %llu\n", total);
  printf("Filter Duplicates: %d\n", fenGenParams.filterDuplicates);
  printf("Chess 960: %d\n", CHESS_960);
  printf("Book: %s\n", fenGenParams.book);
  printf("Network Path: %s\n", fenGenParams.network);
  printf("Output Dir: %s\n", fenGenParams.dir);
  printf("File Names Prefix: %s\n", fenGenParams.file_prefix);
  printf("Nodes: %llu\n", fenGenParams.nodes);
  printf("Depth: %d\n", fenGenParams.depth);
  printf("Random Move Count: %d\n", fenGenParams.randomMoveCount);
  printf("Random Move Min Ply: %d\n", fenGenParams.randomMoveMin);
  printf("Random Move Max Ply: %d\n", fenGenParams.randomMoveMax);
  printf("Random Move MPV: %d\n", fenGenParams.randomMpv);
  printf("Random Move MPV Diff: %d\n", fenGenParams.randomMpvDiff);
  printf("Eval Limit: %d\n", fenGenParams.evalLimit);
  printf("Write Min Ply: %d\n", fenGenParams.writeMin);
  printf("Write Max Ply: %d\n", fenGenParams.writeMax);

  Generate(total);

  return 0;
}
