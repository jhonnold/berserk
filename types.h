#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint64_t bb_t;

typedef int move_t;
typedef struct {
  move_t moves[256];
  int count;
} moves_t;

#endif
