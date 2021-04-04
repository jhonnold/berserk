#ifndef EVAL_H
#define EVAL_H

#include <inttypes.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

extern Score PHASE_MULTIPLIERS[5];

extern int STATIC_MATERIAL_VALUE[7];

extern TScore PAWN_PSQT[32];
extern TScore KNIGHT_PSQT[32];
extern TScore BISHOP_PSQT[32];
extern TScore ROOK_PSQT[32];
extern TScore QUEEN_PSQT[32];
extern TScore KING_PSQT[32];
extern TScore KNIGHT_POST_PSQT[32];
extern TScore MATERIAL_VALUES[7];

extern TScore KNIGHT_MOBILITIES[9];
extern TScore BISHOP_MOBILITIES[14];
extern TScore ROOK_MOBILITIES[15];
extern TScore QUEEN_MOBILITIES[28];

extern TScore DOUBLED_PAWN;
extern TScore OPPOSED_ISOLATED_PAWN;
extern TScore OPEN_ISOLATED_PAWN;
extern TScore BACKWARDS_PAWN;
extern TScore DEFENDED_PAWN;
extern TScore CONNECTED_PAWN[8];
extern TScore PASSED_PAWN[8];
extern TScore PASSED_PAWN_ADVANCE_DEFENDED;
extern TScore PASSED_PAWN_EDGE_DISTANCE;

extern TScore ROOK_OPEN_FILE;
extern TScore ROOK_SEMI_OPEN;
extern TScore ROOK_SEVENTH_RANK;
extern TScore ROOK_OPPOSITE_KING;
extern TScore ROOK_ADJACENT_KING;
extern TScore ROOK_TRAPPED;
extern TScore BISHOP_TRAPPED;
extern TScore BISHOP_PAIR;

extern TScore KNIGHT_THREATS[6];
extern TScore BISHOP_THREATS[6];
extern TScore ROOK_THREATS[6];
extern TScore KING_THREATS[6];

extern TScore TEMPO;

void initPSQT();

Score maxPhase();
Score getPhase(Board* board);
Score taper(Score mg, Score eg, Score phase);

Score toScore(EvalData* data, Board* board);
int isMaterialDraw(Board* board);
void EvaluateSide(Board* board, int side, EvalData* data);
void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData);
void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData);
Score Evaluate(Board* board);
void PrintEvaluation(Board* board);

#endif