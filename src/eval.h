#ifndef EVAL_H
#define EVAL_H

#include <inttypes.h>

#include "types.h"

#define makeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define scoreMG(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define scoreEG(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

extern const Score PAWN_VALUE;
extern const Score KNIGHT_VALUE;
extern const Score BISHOP_VALUE;
extern const Score ROOK_VALUE;
extern const Score QUEEN_VALUE;
extern const Score KING_VALUE;
extern const Score MATERIAL_VALUES[];

extern Score MATERIAL_AND_PSQT_VALUES[12][64];

extern Score KS_ATTACKER_WEIGHTS[];
extern Score KS_ATTACK;
extern Score KS_WEAK_SQS;
extern Score KS_SAFE_CHECK;
extern Score KS_UNSAFE_CHECK;
extern Score KS_ENEMY_QUEEN;
extern Score KS_ALLIES;

void initPSQT();

int getPhase(Board* board);
Score taper(Score mg, Score eg, int phase);

Score toScore(EvalData* data, Board* board);
int isMaterialDraw(Board* board);
void EvaluateSide(Board* board, int side, EvalData* data);
void EvaluateThreats(Board* board, int side, EvalData* data, EvalData* enemyData);
void EvaluateKingSafety(Board* board, int side, EvalData* data, EvalData* enemyData);
Score Evaluate(Board* board);
void PrintEvaluation(Board* board);

#endif