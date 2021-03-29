#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

extern const int CHECKMATE;
extern const int MATE_BOUND;

void initLMR();
void initSearchData(SearchData* data);

void Search(SearchParams* params, SearchData* data);
int negamax(int alpha, int beta, int depth, SearchParams* params, SearchData* data, PV* pv);
int quiesce(int alpha, int beta, SearchParams* params, SearchData* data, PV* pv);

void printInfo(PV* pv, int score, int depth, SearchParams* params, SearchData* data);
void printPv(PV* pv);

#endif
