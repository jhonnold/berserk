#ifndef FILTER_H
#define FILTER_H

#define THREADS 32
#define FILE_PATH "C:\\Programming\\berserk-testing\\texel\\berserk-texel.epd"
#define OUTPUT_PATH "C:\\Programming\\berserk-testing\\texel\\berserk-texel-quiets.epd"

typedef struct {
  int quiet;
  char fen[100];
  char orig[128];
} PotentialQuietFen;

typedef struct {
  int count;
  PotentialQuietFen* positions;
} BatchFilter;

void FilterAll();
void filter(PotentialQuietFen* positions, int n);
void* batchFilter(void* arg);
void quiet(PotentialQuietFen* position);
PotentialQuietFen* loadFilteringPositions(int* n);

#endif