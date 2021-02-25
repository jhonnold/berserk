#ifndef UCI_H
#define UCI_H

void parseGo(char* in, SearchParams* params, Board* board);
void parsePosition(char* in, Board* board);
void UCI(Board* board);

#endif