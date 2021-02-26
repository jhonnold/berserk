#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

#define INITIAL_RANDOM_STATE 234

uint32_t randomInt();
uint64_t randomLong();
uint64_t randomMagic();

#endif