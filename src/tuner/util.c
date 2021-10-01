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


float ComputeK(int n, Position* positions) {
  float dK = 0.01;
  float dEdK = 1;
  float rate = 100;
  float dev = 1e-6;
  float k = -3.0;

  while (fabs(dEdK) > dev) {
    k += dK;
    float Epdk = TotalStaticError(n, positions);
    k -= 2 * dK;
    float Emdk = TotalStaticError(n, positions);
    k += dK;

    dEdK = (Epdk - Emdk) / (2 * dK);

    printf("K: %.9f, Error: %.9f, Deviation: %.9f\n", k, (Epdk + Emdk) / 2, fabs(dEdK));

    k -= dEdK * rate;
  }

  return k;
}

float Sigmoid(float s, float k) { return 1.0f / (1.0f + expf(s * k / 800.0f)); }

float SigmoidPrime(float s, float k) { return s * (1.0 - s) * k / 800.0f; }