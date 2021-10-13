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

#include "types.h"

void ApplyFirstLayer(Board* board, Accumulator output, int perspective);
int ApplySecondLayer(Accumulator a1, Accumulator a2);

int NNPredict(Board* board);
void LoadDefaultNN();

void ApplyUpdates(NNUpdate* updates, Accumulator output, int stm);
void AddUpdate(int feature, int c, NNUpdate* updates);