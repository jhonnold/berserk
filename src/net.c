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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "net.h"

void NewMatrix(int rows, int columns, Matrix* m) {
  if (m->values)
    free(m->values);

  m->values = calloc(rows * columns, sizeof(float));
  m->rows = rows;
  m->columns = columns;
}

void ClearMatrix(Matrix* m) {
  for (int i = 0; i < m->rows * m->columns; i++)
    m->values = 0;
}

int MatrixIdx(int row, int column, Matrix* m) { return row * m->columns + column; }

void AddMatricies(Matrix* m1, Matrix* m2, Matrix* dest) {
  for (int i = 0; i < m1->rows * m1->columns; i++)
    dest->values[i] = m1->values[i] + m2->values[i];
}

void MultiplyMatricies(Matrix* m1, Matrix* m2, Matrix* dest) {
  for (int r1 = 0; r1 < m1->rows; r1++) {
    for (int c2 = 0; c2 < m2->columns; c2++) {
      float s = 0;
      for (int c1 = 0; c1 < m1->columns; c1++)
        s += m1->values[MatrixIdx(r1, c1, m1)] * m2->values[MatrixIdx(c1, c2, m2)];

      dest->values[MatrixIdx(r1, c2, dest)] = s;
    }
  }
}

void ApplyFuncToMatrix(Matrix* m, float (*fn)(float)) {
  for (int i = 0; i < m->rows * m->columns; i++)
    m->values[i] = fn(m->values[i]);
}

void PrintMatrix(Matrix* m) {
  printf("\n");
  for (int i = 0; i < m->rows; i++) {
    printf("[");
    for (int j = 0; j < m->columns; j++)
      printf("%2.4f,", m->values[MatrixIdx(i, j, m)]);

    printf("]\n");
  }
  printf("\n");
}

void MatrixTesting() {
  Matrix* m1 = malloc(sizeof(Matrix));
  Matrix* m2 = malloc(sizeof(Matrix));
  Matrix* dest = malloc(sizeof(Matrix));

  NewMatrix(1, 4, m1);
  NewMatrix(4, 1, m2);
  NewMatrix(1, 1, dest);

  m1->values[0] = 1;
  m1->values[1] = 2;
  m1->values[2] = 3;
  m1->values[3] = 4;

  PrintMatrix(m1);

  m2->values[0] = 1;
  m2->values[1] = 2;
  m2->values[2] = 3;
  m2->values[3] = 4;

  PrintMatrix(m2);

  MultiplyMatricies(m1, m2, dest);

  PrintMatrix(dest);
}
