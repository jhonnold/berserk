#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

uint64_t rotate(uint64_t v, uint8_t s);
uint64_t randomLong();
void seedRandom(uint64_t seed);
uint64_t randomMagic();

#endif