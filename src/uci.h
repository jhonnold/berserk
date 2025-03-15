// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#ifndef UCI_H
#define UCI_H

#include "types.h"

extern int SHOW_WDL;
extern int CHESS_960;
extern int CONTEMPT;
extern SearchParams Limits;

// Normalization of a score to 50% WR at 100cp
#define Normalize(s) ((s) / 1.70)

int WRModel(Score s, int ply);

void RootMoves(SimpleMoveList* moves, Board* board);

void ParseGo(char* in, Board* board);
void ParsePosition(char* in, Board* board);
void PrintUCIOptions();

int ReadLine(char* in);
void UCILoop();

int GetOptionIntValue(char* in);

#endif