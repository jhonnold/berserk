#ifndef EVAL_H
#define EVAL_H

#include <inttypes.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

extern Score PHASE_MULTIPLIERS[5];

extern int STATIC_MATERIAL_VALUE[7];

extern Score PAWN_PSQT[32][2];
extern Score KNIGHT_PSQT[32][2];
extern Score BISHOP_PSQT[32][2];
extern Score ROOK_PSQT[32][2];
extern Score QUEEN_PSQT[32][2];
extern Score KING_PSQT[32][2];
extern Score KNIGHT_POST_PSQT[32][2];
extern Score MATERIAL_VALUES[7][2];

extern Score KNIGHT_MOBILITIES[9][2];
extern Score BISHOP_MOBILITIES[14][2];
extern Score ROOK_MOBILITIES[15][2];
extern Score QUEEN_MOBILITIES[28][2];

extern Score DOUBLED_PAWN[2];
extern Score OPPOSED_ISOLATED_PAWN[2];
extern Score OPEN_ISOLATED_PAWN[2];
extern Score BACKWARDS_PAWN[2];
extern Score DEFENDED_PAWN[2];
extern Score CONNECTED_PAWN[8][2];
extern Score PASSED_PAWN[8][2];
extern Score PASSED_PAWN_ADVANCE_DEFENDED[2];
extern Score PASSED_PAWN_EDGE_DISTANCE[2];

extern Score DEFENDED_MINOR[2];

extern Score ROOK_OPEN_FILE[2];
extern Score ROOK_SEMI_OPEN[2];
extern Score ROOK_SEVENTH_RANK[2];
extern Score ROOK_OPPOSITE_KING[2];
extern Score ROOK_ADJACENT_KING[2];
extern Score ROOK_TRAPPED[2];
extern Score BISHOP_TRAPPED[2];
extern Score BISHOP_PAIR[2];

extern Score KNIGHT_THREATS[6][2];
extern Score BISHOP_THREATS[6][2];
extern Score ROOK_THREATS[6][2];
extern Score KING_THREATS[6][2];
extern Score HANGING_THREAT[2];

extern Score PAWN_SHELTER[2][8][2];
extern Score PAWN_STORM[8][2];

extern Score KS_ATTACKER_WEIGHTS[5];
extern Score KS_ATTACK;
extern Score KS_WEAK_SQS;
extern Score KS_SAFE_CHECK;
extern Score KS_UNSAFE_CHECK;
extern Score KS_ENEMY_QUEEN;
extern Score KS_ALLIES;

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