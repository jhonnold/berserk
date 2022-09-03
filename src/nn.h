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

#include "board.h"
#include "types.h"
#include "util.h"

extern uint64_t NN_HASH;
extern const int QUANTIZATION_PRECISION_IN;
extern const int QUANTIZATION_PRECISION_OUT;

extern int16_t INPUT_WEIGHTS[N_FEATURES * N_HIDDEN];
extern int16_t INPUT_BIASES[N_HIDDEN];
extern int16_t OUTPUT_WEIGHTS[2 * N_HIDDEN];
extern int32_t OUTPUT_BIAS;

int Predict(Board* board);
int OutputLayer(Accumulator stm, Accumulator xstm);
void ResetRefreshTable(Board* board);
void StoreAccumulatorKingState(Accumulator accumulator, Board* board, const int perspective);
void RefreshAccumulator(Accumulator accumulator, Board* board, const int perspective);
void ResetAccumulator(Accumulator output, Board* board, const int perspective);

void ApplyUpdates(Board* board, Move move, int captured, const int view);

void LoadDefaultNN();
int LoadNetwork(char* path);
