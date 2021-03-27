#ifndef FILTER_H
#define FILTER_H

#define THREADS 32
#define FILE_PATH "C:\\Programming\\berserk-testing\\texel\\berserk-3.2.0-violent.epd"
#define OUTPUT_PATH "C:\\Programming\\berserk-testing\\texel\\berserk-3.2.0-quiet.epd"

typedef struct {
  int quiet;
  char fen[100];
  char result[16];
} PotentialQuietFen;

typedef struct {
  int thread;
  int count;
  PotentialQuietFen* positions;
} BatchFilter;

void RunFilter();
void filter(PotentialQuietFen* positions, int n);
void* batchFilter(void* arg);
void quiet(PotentialQuietFen* position);
PotentialQuietFen* loadFilteringPositions(int* n);

#endif