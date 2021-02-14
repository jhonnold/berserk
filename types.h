#ifndef TYPES_H
#define TYPES_H

typedef unsigned long bb_t;

typedef int move_t;
typedef struct {
  move_t moves[256];
  int count;
} moves_t;

#endif
