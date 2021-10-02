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

#include <math.h>
#include <stdio.h>

#include "tune.h"
#include "util.h"

float Sigmoid(float s, float k) { return 1.0f / (1.0f + expf(-s * k / 1024.0f)); }

float SigmoidPrime(float s, float k) { return s * (1.0 - s) * k / 1024.0f; }

float ReLu(float s) { return s > 0.0f ? s : 0; }

float ReLuPrime(float s) { return s > 0.0f; }