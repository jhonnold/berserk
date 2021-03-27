#ifdef TUNE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "eval.h"
#include "texel.h"
#include "types.h"

const int sideScalar[] = {1, -1};
double K = 1;

void addParam(char* name, Score* p, TexelParam* params, int* n) {
  params[*n].name = name;
  params[*n].param = p;

  *n += 1;
}

void Texel() {
  int n = 0;
  Position* positions = loadPositions(&n);

  int numParams = 0;
  TexelParam params[1024];

  addParam("PHASE_MULTIPLIERS[KNIGHT]", &PHASE_MULTIPLIERS[KNIGHT_TYPE], params, &numParams);
  addParam("PHASE_MULTIPLIERS[BISHOP]", &PHASE_MULTIPLIERS[BISHOP_TYPE], params, &numParams);
  addParam("PHASE_MULTIPLIERS[ROOK]", &PHASE_MULTIPLIERS[ROOK_TYPE], params, &numParams);
  addParam("PHASE_MULTIPLIERS[QUEEN]", &PHASE_MULTIPLIERS[QUEEN_TYPE], params, &numParams);

  addParam("MATERIAL_VALUES_PAWN[EG]", &MATERIAL_VALUES[PAWN_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_KNIGHT[MG]", &MATERIAL_VALUES[KNIGHT_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_KNIGHT[EG]", &MATERIAL_VALUES[KNIGHT_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_BISHOP[MG]", &MATERIAL_VALUES[BISHOP_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_BISHOP[EG]", &MATERIAL_VALUES[BISHOP_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_ROOK[MG]", &MATERIAL_VALUES[ROOK_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_ROOK[EG]", &MATERIAL_VALUES[ROOK_TYPE][EG], params, &numParams);
  addParam("MATERIAL_VALUES_QUEEN[MG]", &MATERIAL_VALUES[QUEEN_TYPE][MG], params, &numParams);
  addParam("MATERIAL_VALUES_QUEEN[EG]", &MATERIAL_VALUES[QUEEN_TYPE][EG], params, &numParams);

  // addParam("PAWN_PSQT[4][MG]", &PAWN_PSQT[4][MG], params, &numParams);
  // addParam("PAWN_PSQT[4][EG]", &PAWN_PSQT[4][EG], params, &numParams);
  // addParam("PAWN_PSQT[5][MG]", &PAWN_PSQT[5][MG], params, &numParams);
  // addParam("PAWN_PSQT[5][EG]", &PAWN_PSQT[5][EG], params, &numParams);
  // addParam("PAWN_PSQT[6][MG]", &PAWN_PSQT[6][MG], params, &numParams);
  // addParam("PAWN_PSQT[6][EG]", &PAWN_PSQT[6][EG], params, &numParams);
  // addParam("PAWN_PSQT[7][MG]", &PAWN_PSQT[7][MG], params, &numParams);
  // addParam("PAWN_PSQT[7][EG]", &PAWN_PSQT[7][EG], params, &numParams);
  // addParam("PAWN_PSQT[8][MG]", &PAWN_PSQT[8][MG], params, &numParams);
  // addParam("PAWN_PSQT[8][EG]", &PAWN_PSQT[8][EG], params, &numParams);
  // addParam("PAWN_PSQT[9][MG]", &PAWN_PSQT[9][MG], params, &numParams);
  // addParam("PAWN_PSQT[9][EG]", &PAWN_PSQT[9][EG], params, &numParams);
  // addParam("PAWN_PSQT[10][MG]", &PAWN_PSQT[10][MG], params, &numParams);
  // addParam("PAWN_PSQT[10][EG]", &PAWN_PSQT[10][EG], params, &numParams);
  // addParam("PAWN_PSQT[11][MG]", &PAWN_PSQT[11][MG], params, &numParams);
  // addParam("PAWN_PSQT[11][EG]", &PAWN_PSQT[11][EG], params, &numParams);
  // addParam("PAWN_PSQT[12][MG]", &PAWN_PSQT[12][MG], params, &numParams);
  // addParam("PAWN_PSQT[12][EG]", &PAWN_PSQT[12][EG], params, &numParams);
  // addParam("PAWN_PSQT[13][MG]", &PAWN_PSQT[13][MG], params, &numParams);
  // addParam("PAWN_PSQT[13][EG]", &PAWN_PSQT[13][EG], params, &numParams);
  // addParam("PAWN_PSQT[14][MG]", &PAWN_PSQT[14][MG], params, &numParams);
  // addParam("PAWN_PSQT[14][EG]", &PAWN_PSQT[14][EG], params, &numParams);
  // addParam("PAWN_PSQT[15][MG]", &PAWN_PSQT[15][MG], params, &numParams);
  // addParam("PAWN_PSQT[15][EG]", &PAWN_PSQT[15][EG], params, &numParams);
  // addParam("PAWN_PSQT[16][MG]", &PAWN_PSQT[16][MG], params, &numParams);
  // addParam("PAWN_PSQT[16][EG]", &PAWN_PSQT[16][EG], params, &numParams);
  // addParam("PAWN_PSQT[17][MG]", &PAWN_PSQT[17][MG], params, &numParams);
  // addParam("PAWN_PSQT[17][EG]", &PAWN_PSQT[17][EG], params, &numParams);
  // addParam("PAWN_PSQT[18][MG]", &PAWN_PSQT[18][MG], params, &numParams);
  // addParam("PAWN_PSQT[18][EG]", &PAWN_PSQT[18][EG], params, &numParams);
  // addParam("PAWN_PSQT[19][MG]", &PAWN_PSQT[19][MG], params, &numParams);
  // addParam("PAWN_PSQT[19][EG]", &PAWN_PSQT[19][EG], params, &numParams);
  // addParam("PAWN_PSQT[20][MG]", &PAWN_PSQT[20][MG], params, &numParams);
  // addParam("PAWN_PSQT[20][EG]", &PAWN_PSQT[20][EG], params, &numParams);
  // addParam("PAWN_PSQT[21][MG]", &PAWN_PSQT[21][MG], params, &numParams);
  // addParam("PAWN_PSQT[21][EG]", &PAWN_PSQT[21][EG], params, &numParams);
  // addParam("PAWN_PSQT[22][MG]", &PAWN_PSQT[22][MG], params, &numParams);
  // addParam("PAWN_PSQT[22][EG]", &PAWN_PSQT[22][EG], params, &numParams);
  // addParam("PAWN_PSQT[23][MG]", &PAWN_PSQT[23][MG], params, &numParams);
  // addParam("PAWN_PSQT[23][EG]", &PAWN_PSQT[23][EG], params, &numParams);
  // addParam("PAWN_PSQT[24][MG]", &PAWN_PSQT[24][MG], params, &numParams);
  // addParam("PAWN_PSQT[24][EG]", &PAWN_PSQT[24][EG], params, &numParams);
  // addParam("PAWN_PSQT[25][MG]", &PAWN_PSQT[25][MG], params, &numParams);
  // addParam("PAWN_PSQT[25][EG]", &PAWN_PSQT[25][EG], params, &numParams);
  // addParam("PAWN_PSQT[26][MG]", &PAWN_PSQT[26][MG], params, &numParams);
  // addParam("PAWN_PSQT[26][EG]", &PAWN_PSQT[26][EG], params, &numParams);
  // addParam("PAWN_PSQT[27][MG]", &PAWN_PSQT[27][MG], params, &numParams);
  // addParam("PAWN_PSQT[27][EG]", &PAWN_PSQT[27][EG], params, &numParams);

  // addParam("KNIGHT_PSQT[0][MG]", &KNIGHT_PSQT[0][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[0][EG]", &KNIGHT_PSQT[0][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[1][MG]", &KNIGHT_PSQT[1][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[1][EG]", &KNIGHT_PSQT[1][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[2][MG]", &KNIGHT_PSQT[2][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[2][EG]", &KNIGHT_PSQT[2][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[3][MG]", &KNIGHT_PSQT[3][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[3][EG]", &KNIGHT_PSQT[3][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[4][MG]", &KNIGHT_PSQT[4][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[4][EG]", &KNIGHT_PSQT[4][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[5][MG]", &KNIGHT_PSQT[5][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[5][EG]", &KNIGHT_PSQT[5][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[6][MG]", &KNIGHT_PSQT[6][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[6][EG]", &KNIGHT_PSQT[6][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[7][MG]", &KNIGHT_PSQT[7][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[7][EG]", &KNIGHT_PSQT[7][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[8][MG]", &KNIGHT_PSQT[8][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[8][EG]", &KNIGHT_PSQT[8][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[9][MG]", &KNIGHT_PSQT[9][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[9][EG]", &KNIGHT_PSQT[9][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[10][MG]", &KNIGHT_PSQT[10][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[10][EG]", &KNIGHT_PSQT[10][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[11][MG]", &KNIGHT_PSQT[11][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[11][EG]", &KNIGHT_PSQT[11][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[12][MG]", &KNIGHT_PSQT[12][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[12][EG]", &KNIGHT_PSQT[12][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[13][MG]", &KNIGHT_PSQT[13][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[13][EG]", &KNIGHT_PSQT[13][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[14][MG]", &KNIGHT_PSQT[14][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[14][EG]", &KNIGHT_PSQT[14][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[15][MG]", &KNIGHT_PSQT[15][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[15][EG]", &KNIGHT_PSQT[15][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[16][MG]", &KNIGHT_PSQT[16][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[16][EG]", &KNIGHT_PSQT[16][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[17][MG]", &KNIGHT_PSQT[17][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[17][EG]", &KNIGHT_PSQT[17][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[18][MG]", &KNIGHT_PSQT[18][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[18][EG]", &KNIGHT_PSQT[18][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[19][MG]", &KNIGHT_PSQT[19][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[19][EG]", &KNIGHT_PSQT[19][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[20][MG]", &KNIGHT_PSQT[20][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[20][EG]", &KNIGHT_PSQT[20][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[21][MG]", &KNIGHT_PSQT[21][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[21][EG]", &KNIGHT_PSQT[21][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[22][MG]", &KNIGHT_PSQT[22][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[22][EG]", &KNIGHT_PSQT[22][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[23][MG]", &KNIGHT_PSQT[23][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[23][EG]", &KNIGHT_PSQT[23][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[24][MG]", &KNIGHT_PSQT[24][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[24][EG]", &KNIGHT_PSQT[24][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[25][MG]", &KNIGHT_PSQT[25][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[25][EG]", &KNIGHT_PSQT[25][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[26][MG]", &KNIGHT_PSQT[26][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[26][EG]", &KNIGHT_PSQT[26][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[27][MG]", &KNIGHT_PSQT[27][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[27][EG]", &KNIGHT_PSQT[27][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[28][MG]", &KNIGHT_PSQT[28][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[28][EG]", &KNIGHT_PSQT[28][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[29][MG]", &KNIGHT_PSQT[29][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[29][EG]", &KNIGHT_PSQT[29][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[30][MG]", &KNIGHT_PSQT[30][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[30][EG]", &KNIGHT_PSQT[30][EG], params, &numParams);
  // addParam("KNIGHT_PSQT[31][MG]", &KNIGHT_PSQT[31][MG], params, &numParams);
  // addParam("KNIGHT_PSQT[31][EG]", &KNIGHT_PSQT[31][EG], params, &numParams);

  // addParam("BISHOP_PSQT[0][MG]", &BISHOP_PSQT[0][MG], params, &numParams);
  // addParam("BISHOP_PSQT[0][EG]", &BISHOP_PSQT[0][EG], params, &numParams);
  // addParam("BISHOP_PSQT[1][MG]", &BISHOP_PSQT[1][MG], params, &numParams);
  // addParam("BISHOP_PSQT[1][EG]", &BISHOP_PSQT[1][EG], params, &numParams);
  // addParam("BISHOP_PSQT[2][MG]", &BISHOP_PSQT[2][MG], params, &numParams);
  // addParam("BISHOP_PSQT[2][EG]", &BISHOP_PSQT[2][EG], params, &numParams);
  // addParam("BISHOP_PSQT[3][MG]", &BISHOP_PSQT[3][MG], params, &numParams);
  // addParam("BISHOP_PSQT[3][EG]", &BISHOP_PSQT[3][EG], params, &numParams);
  // addParam("BISHOP_PSQT[4][MG]", &BISHOP_PSQT[4][MG], params, &numParams);
  // addParam("BISHOP_PSQT[4][EG]", &BISHOP_PSQT[4][EG], params, &numParams);
  // addParam("BISHOP_PSQT[5][MG]", &BISHOP_PSQT[5][MG], params, &numParams);
  // addParam("BISHOP_PSQT[5][EG]", &BISHOP_PSQT[5][EG], params, &numParams);
  // addParam("BISHOP_PSQT[6][MG]", &BISHOP_PSQT[6][MG], params, &numParams);
  // addParam("BISHOP_PSQT[6][EG]", &BISHOP_PSQT[6][EG], params, &numParams);
  // addParam("BISHOP_PSQT[7][MG]", &BISHOP_PSQT[7][MG], params, &numParams);
  // addParam("BISHOP_PSQT[7][EG]", &BISHOP_PSQT[7][EG], params, &numParams);
  // addParam("BISHOP_PSQT[8][MG]", &BISHOP_PSQT[8][MG], params, &numParams);
  // addParam("BISHOP_PSQT[8][EG]", &BISHOP_PSQT[8][EG], params, &numParams);
  // addParam("BISHOP_PSQT[9][MG]", &BISHOP_PSQT[9][MG], params, &numParams);
  // addParam("BISHOP_PSQT[9][EG]", &BISHOP_PSQT[9][EG], params, &numParams);
  // addParam("BISHOP_PSQT[10][MG]", &BISHOP_PSQT[10][MG], params, &numParams);
  // addParam("BISHOP_PSQT[10][EG]", &BISHOP_PSQT[10][EG], params, &numParams);
  // addParam("BISHOP_PSQT[11][MG]", &BISHOP_PSQT[11][MG], params, &numParams);
  // addParam("BISHOP_PSQT[11][EG]", &BISHOP_PSQT[11][EG], params, &numParams);
  // addParam("BISHOP_PSQT[12][MG]", &BISHOP_PSQT[12][MG], params, &numParams);
  // addParam("BISHOP_PSQT[12][EG]", &BISHOP_PSQT[12][EG], params, &numParams);
  // addParam("BISHOP_PSQT[13][MG]", &BISHOP_PSQT[13][MG], params, &numParams);
  // addParam("BISHOP_PSQT[13][EG]", &BISHOP_PSQT[13][EG], params, &numParams);
  // addParam("BISHOP_PSQT[14][MG]", &BISHOP_PSQT[14][MG], params, &numParams);
  // addParam("BISHOP_PSQT[14][EG]", &BISHOP_PSQT[14][EG], params, &numParams);
  // addParam("BISHOP_PSQT[15][MG]", &BISHOP_PSQT[15][MG], params, &numParams);
  // addParam("BISHOP_PSQT[15][EG]", &BISHOP_PSQT[15][EG], params, &numParams);
  // addParam("BISHOP_PSQT[16][MG]", &BISHOP_PSQT[16][MG], params, &numParams);
  // addParam("BISHOP_PSQT[16][EG]", &BISHOP_PSQT[16][EG], params, &numParams);
  // addParam("BISHOP_PSQT[17][MG]", &BISHOP_PSQT[17][MG], params, &numParams);
  // addParam("BISHOP_PSQT[17][EG]", &BISHOP_PSQT[17][EG], params, &numParams);
  // addParam("BISHOP_PSQT[18][MG]", &BISHOP_PSQT[18][MG], params, &numParams);
  // addParam("BISHOP_PSQT[18][EG]", &BISHOP_PSQT[18][EG], params, &numParams);
  // addParam("BISHOP_PSQT[19][MG]", &BISHOP_PSQT[19][MG], params, &numParams);
  // addParam("BISHOP_PSQT[19][EG]", &BISHOP_PSQT[19][EG], params, &numParams);
  // addParam("BISHOP_PSQT[20][MG]", &BISHOP_PSQT[20][MG], params, &numParams);
  // addParam("BISHOP_PSQT[20][EG]", &BISHOP_PSQT[20][EG], params, &numParams);
  // addParam("BISHOP_PSQT[21][MG]", &BISHOP_PSQT[21][MG], params, &numParams);
  // addParam("BISHOP_PSQT[21][EG]", &BISHOP_PSQT[21][EG], params, &numParams);
  // addParam("BISHOP_PSQT[22][MG]", &BISHOP_PSQT[22][MG], params, &numParams);
  // addParam("BISHOP_PSQT[22][EG]", &BISHOP_PSQT[22][EG], params, &numParams);
  // addParam("BISHOP_PSQT[23][MG]", &BISHOP_PSQT[23][MG], params, &numParams);
  // addParam("BISHOP_PSQT[23][EG]", &BISHOP_PSQT[23][EG], params, &numParams);
  // addParam("BISHOP_PSQT[24][MG]", &BISHOP_PSQT[24][MG], params, &numParams);
  // addParam("BISHOP_PSQT[24][EG]", &BISHOP_PSQT[24][EG], params, &numParams);
  // addParam("BISHOP_PSQT[25][MG]", &BISHOP_PSQT[25][MG], params, &numParams);
  // addParam("BISHOP_PSQT[25][EG]", &BISHOP_PSQT[25][EG], params, &numParams);
  // addParam("BISHOP_PSQT[26][MG]", &BISHOP_PSQT[26][MG], params, &numParams);
  // addParam("BISHOP_PSQT[26][EG]", &BISHOP_PSQT[26][EG], params, &numParams);
  // addParam("BISHOP_PSQT[27][MG]", &BISHOP_PSQT[27][MG], params, &numParams);
  // addParam("BISHOP_PSQT[27][EG]", &BISHOP_PSQT[27][EG], params, &numParams);
  // addParam("BISHOP_PSQT[28][MG]", &BISHOP_PSQT[28][MG], params, &numParams);
  // addParam("BISHOP_PSQT[28][EG]", &BISHOP_PSQT[28][EG], params, &numParams);
  // addParam("BISHOP_PSQT[29][MG]", &BISHOP_PSQT[29][MG], params, &numParams);
  // addParam("BISHOP_PSQT[29][EG]", &BISHOP_PSQT[29][EG], params, &numParams);
  // addParam("BISHOP_PSQT[30][MG]", &BISHOP_PSQT[30][MG], params, &numParams);
  // addParam("BISHOP_PSQT[30][EG]", &BISHOP_PSQT[30][EG], params, &numParams);
  // addParam("BISHOP_PSQT[31][MG]", &BISHOP_PSQT[31][MG], params, &numParams);
  // addParam("BISHOP_PSQT[31][EG]", &BISHOP_PSQT[31][EG], params, &numParams);

  // addParam("ROOK_PSQT[0][MG]", &ROOK_PSQT[0][MG], params, &numParams);
  // addParam("ROOK_PSQT[0][EG]", &ROOK_PSQT[0][EG], params, &numParams);
  // addParam("ROOK_PSQT[1][MG]", &ROOK_PSQT[1][MG], params, &numParams);
  // addParam("ROOK_PSQT[1][EG]", &ROOK_PSQT[1][EG], params, &numParams);
  // addParam("ROOK_PSQT[2][MG]", &ROOK_PSQT[2][MG], params, &numParams);
  // addParam("ROOK_PSQT[2][EG]", &ROOK_PSQT[2][EG], params, &numParams);
  // addParam("ROOK_PSQT[3][MG]", &ROOK_PSQT[3][MG], params, &numParams);
  // addParam("ROOK_PSQT[3][EG]", &ROOK_PSQT[3][EG], params, &numParams);
  // addParam("ROOK_PSQT[4][MG]", &ROOK_PSQT[4][MG], params, &numParams);
  // addParam("ROOK_PSQT[4][EG]", &ROOK_PSQT[4][EG], params, &numParams);
  // addParam("ROOK_PSQT[5][MG]", &ROOK_PSQT[5][MG], params, &numParams);
  // addParam("ROOK_PSQT[5][EG]", &ROOK_PSQT[5][EG], params, &numParams);
  // addParam("ROOK_PSQT[6][MG]", &ROOK_PSQT[6][MG], params, &numParams);
  // addParam("ROOK_PSQT[6][EG]", &ROOK_PSQT[6][EG], params, &numParams);
  // addParam("ROOK_PSQT[7][MG]", &ROOK_PSQT[7][MG], params, &numParams);
  // addParam("ROOK_PSQT[7][EG]", &ROOK_PSQT[7][EG], params, &numParams);
  // addParam("ROOK_PSQT[8][MG]", &ROOK_PSQT[8][MG], params, &numParams);
  // addParam("ROOK_PSQT[8][EG]", &ROOK_PSQT[8][EG], params, &numParams);
  // addParam("ROOK_PSQT[9][MG]", &ROOK_PSQT[9][MG], params, &numParams);
  // addParam("ROOK_PSQT[9][EG]", &ROOK_PSQT[9][EG], params, &numParams);
  // addParam("ROOK_PSQT[10][MG]", &ROOK_PSQT[10][MG], params, &numParams);
  // addParam("ROOK_PSQT[10][EG]", &ROOK_PSQT[10][EG], params, &numParams);
  // addParam("ROOK_PSQT[11][MG]", &ROOK_PSQT[11][MG], params, &numParams);
  // addParam("ROOK_PSQT[11][EG]", &ROOK_PSQT[11][EG], params, &numParams);
  // addParam("ROOK_PSQT[12][MG]", &ROOK_PSQT[12][MG], params, &numParams);
  // addParam("ROOK_PSQT[12][EG]", &ROOK_PSQT[12][EG], params, &numParams);
  // addParam("ROOK_PSQT[13][MG]", &ROOK_PSQT[13][MG], params, &numParams);
  // addParam("ROOK_PSQT[13][EG]", &ROOK_PSQT[13][EG], params, &numParams);
  // addParam("ROOK_PSQT[14][MG]", &ROOK_PSQT[14][MG], params, &numParams);
  // addParam("ROOK_PSQT[14][EG]", &ROOK_PSQT[14][EG], params, &numParams);
  // addParam("ROOK_PSQT[15][MG]", &ROOK_PSQT[15][MG], params, &numParams);
  // addParam("ROOK_PSQT[15][EG]", &ROOK_PSQT[15][EG], params, &numParams);
  // addParam("ROOK_PSQT[16][MG]", &ROOK_PSQT[16][MG], params, &numParams);
  // addParam("ROOK_PSQT[16][EG]", &ROOK_PSQT[16][EG], params, &numParams);
  // addParam("ROOK_PSQT[17][MG]", &ROOK_PSQT[17][MG], params, &numParams);
  // addParam("ROOK_PSQT[17][EG]", &ROOK_PSQT[17][EG], params, &numParams);
  // addParam("ROOK_PSQT[18][MG]", &ROOK_PSQT[18][MG], params, &numParams);
  // addParam("ROOK_PSQT[18][EG]", &ROOK_PSQT[18][EG], params, &numParams);
  // addParam("ROOK_PSQT[19][MG]", &ROOK_PSQT[19][MG], params, &numParams);
  // addParam("ROOK_PSQT[19][EG]", &ROOK_PSQT[19][EG], params, &numParams);
  // addParam("ROOK_PSQT[20][MG]", &ROOK_PSQT[20][MG], params, &numParams);
  // addParam("ROOK_PSQT[20][EG]", &ROOK_PSQT[20][EG], params, &numParams);
  // addParam("ROOK_PSQT[21][MG]", &ROOK_PSQT[21][MG], params, &numParams);
  // addParam("ROOK_PSQT[21][EG]", &ROOK_PSQT[21][EG], params, &numParams);
  // addParam("ROOK_PSQT[22][MG]", &ROOK_PSQT[22][MG], params, &numParams);
  // addParam("ROOK_PSQT[22][EG]", &ROOK_PSQT[22][EG], params, &numParams);
  // addParam("ROOK_PSQT[23][MG]", &ROOK_PSQT[23][MG], params, &numParams);
  // addParam("ROOK_PSQT[23][EG]", &ROOK_PSQT[23][EG], params, &numParams);
  // addParam("ROOK_PSQT[24][MG]", &ROOK_PSQT[24][MG], params, &numParams);
  // addParam("ROOK_PSQT[24][EG]", &ROOK_PSQT[24][EG], params, &numParams);
  // addParam("ROOK_PSQT[25][MG]", &ROOK_PSQT[25][MG], params, &numParams);
  // addParam("ROOK_PSQT[25][EG]", &ROOK_PSQT[25][EG], params, &numParams);
  // addParam("ROOK_PSQT[26][MG]", &ROOK_PSQT[26][MG], params, &numParams);
  // addParam("ROOK_PSQT[26][EG]", &ROOK_PSQT[26][EG], params, &numParams);
  // addParam("ROOK_PSQT[27][MG]", &ROOK_PSQT[27][MG], params, &numParams);
  // addParam("ROOK_PSQT[27][EG]", &ROOK_PSQT[27][EG], params, &numParams);
  // addParam("ROOK_PSQT[28][MG]", &ROOK_PSQT[28][MG], params, &numParams);
  // addParam("ROOK_PSQT[28][EG]", &ROOK_PSQT[28][EG], params, &numParams);
  // addParam("ROOK_PSQT[29][MG]", &ROOK_PSQT[29][MG], params, &numParams);
  // addParam("ROOK_PSQT[29][EG]", &ROOK_PSQT[29][EG], params, &numParams);
  // addParam("ROOK_PSQT[30][MG]", &ROOK_PSQT[30][MG], params, &numParams);
  // addParam("ROOK_PSQT[30][EG]", &ROOK_PSQT[30][EG], params, &numParams);
  // addParam("ROOK_PSQT[31][MG]", &ROOK_PSQT[31][MG], params, &numParams);
  // addParam("ROOK_PSQT[31][EG]", &ROOK_PSQT[31][EG], params, &numParams);

  // addParam("QUEEN_PSQT[0][MG]", &QUEEN_PSQT[0][MG], params, &numParams);
  // addParam("QUEEN_PSQT[0][EG]", &QUEEN_PSQT[0][EG], params, &numParams);
  // addParam("QUEEN_PSQT[1][MG]", &QUEEN_PSQT[1][MG], params, &numParams);
  // addParam("QUEEN_PSQT[1][EG]", &QUEEN_PSQT[1][EG], params, &numParams);
  // addParam("QUEEN_PSQT[2][MG]", &QUEEN_PSQT[2][MG], params, &numParams);
  // addParam("QUEEN_PSQT[2][EG]", &QUEEN_PSQT[2][EG], params, &numParams);
  // addParam("QUEEN_PSQT[3][MG]", &QUEEN_PSQT[3][MG], params, &numParams);
  // addParam("QUEEN_PSQT[3][EG]", &QUEEN_PSQT[3][EG], params, &numParams);
  // addParam("QUEEN_PSQT[4][MG]", &QUEEN_PSQT[4][MG], params, &numParams);
  // addParam("QUEEN_PSQT[4][EG]", &QUEEN_PSQT[4][EG], params, &numParams);
  // addParam("QUEEN_PSQT[5][MG]", &QUEEN_PSQT[5][MG], params, &numParams);
  // addParam("QUEEN_PSQT[5][EG]", &QUEEN_PSQT[5][EG], params, &numParams);
  // addParam("QUEEN_PSQT[6][MG]", &QUEEN_PSQT[6][MG], params, &numParams);
  // addParam("QUEEN_PSQT[6][EG]", &QUEEN_PSQT[6][EG], params, &numParams);
  // addParam("QUEEN_PSQT[7][MG]", &QUEEN_PSQT[7][MG], params, &numParams);
  // addParam("QUEEN_PSQT[7][EG]", &QUEEN_PSQT[7][EG], params, &numParams);
  // addParam("QUEEN_PSQT[8][MG]", &QUEEN_PSQT[8][MG], params, &numParams);
  // addParam("QUEEN_PSQT[8][EG]", &QUEEN_PSQT[8][EG], params, &numParams);
  // addParam("QUEEN_PSQT[9][MG]", &QUEEN_PSQT[9][MG], params, &numParams);
  // addParam("QUEEN_PSQT[9][EG]", &QUEEN_PSQT[9][EG], params, &numParams);
  // addParam("QUEEN_PSQT[10][MG]", &QUEEN_PSQT[10][MG], params, &numParams);
  // addParam("QUEEN_PSQT[10][EG]", &QUEEN_PSQT[10][EG], params, &numParams);
  // addParam("QUEEN_PSQT[11][MG]", &QUEEN_PSQT[11][MG], params, &numParams);
  // addParam("QUEEN_PSQT[11][EG]", &QUEEN_PSQT[11][EG], params, &numParams);
  // addParam("QUEEN_PSQT[12][MG]", &QUEEN_PSQT[12][MG], params, &numParams);
  // addParam("QUEEN_PSQT[12][EG]", &QUEEN_PSQT[12][EG], params, &numParams);
  // addParam("QUEEN_PSQT[13][MG]", &QUEEN_PSQT[13][MG], params, &numParams);
  // addParam("QUEEN_PSQT[13][EG]", &QUEEN_PSQT[13][EG], params, &numParams);
  // addParam("QUEEN_PSQT[14][MG]", &QUEEN_PSQT[14][MG], params, &numParams);
  // addParam("QUEEN_PSQT[14][EG]", &QUEEN_PSQT[14][EG], params, &numParams);
  // addParam("QUEEN_PSQT[15][MG]", &QUEEN_PSQT[15][MG], params, &numParams);
  // addParam("QUEEN_PSQT[15][EG]", &QUEEN_PSQT[15][EG], params, &numParams);
  // addParam("QUEEN_PSQT[16][MG]", &QUEEN_PSQT[16][MG], params, &numParams);
  // addParam("QUEEN_PSQT[16][EG]", &QUEEN_PSQT[16][EG], params, &numParams);
  // addParam("QUEEN_PSQT[17][MG]", &QUEEN_PSQT[17][MG], params, &numParams);
  // addParam("QUEEN_PSQT[17][EG]", &QUEEN_PSQT[17][EG], params, &numParams);
  // addParam("QUEEN_PSQT[18][MG]", &QUEEN_PSQT[18][MG], params, &numParams);
  // addParam("QUEEN_PSQT[18][EG]", &QUEEN_PSQT[18][EG], params, &numParams);
  // addParam("QUEEN_PSQT[19][MG]", &QUEEN_PSQT[19][MG], params, &numParams);
  // addParam("QUEEN_PSQT[19][EG]", &QUEEN_PSQT[19][EG], params, &numParams);
  // addParam("QUEEN_PSQT[20][MG]", &QUEEN_PSQT[20][MG], params, &numParams);
  // addParam("QUEEN_PSQT[20][EG]", &QUEEN_PSQT[20][EG], params, &numParams);
  // addParam("QUEEN_PSQT[21][MG]", &QUEEN_PSQT[21][MG], params, &numParams);
  // addParam("QUEEN_PSQT[21][EG]", &QUEEN_PSQT[21][EG], params, &numParams);
  // addParam("QUEEN_PSQT[22][MG]", &QUEEN_PSQT[22][MG], params, &numParams);
  // addParam("QUEEN_PSQT[22][EG]", &QUEEN_PSQT[22][EG], params, &numParams);
  // addParam("QUEEN_PSQT[23][MG]", &QUEEN_PSQT[23][MG], params, &numParams);
  // addParam("QUEEN_PSQT[23][EG]", &QUEEN_PSQT[23][EG], params, &numParams);
  // addParam("QUEEN_PSQT[24][MG]", &QUEEN_PSQT[24][MG], params, &numParams);
  // addParam("QUEEN_PSQT[24][EG]", &QUEEN_PSQT[24][EG], params, &numParams);
  // addParam("QUEEN_PSQT[25][MG]", &QUEEN_PSQT[25][MG], params, &numParams);
  // addParam("QUEEN_PSQT[25][EG]", &QUEEN_PSQT[25][EG], params, &numParams);
  // addParam("QUEEN_PSQT[26][MG]", &QUEEN_PSQT[26][MG], params, &numParams);
  // addParam("QUEEN_PSQT[26][EG]", &QUEEN_PSQT[26][EG], params, &numParams);
  // addParam("QUEEN_PSQT[27][MG]", &QUEEN_PSQT[27][MG], params, &numParams);
  // addParam("QUEEN_PSQT[27][EG]", &QUEEN_PSQT[27][EG], params, &numParams);
  // addParam("QUEEN_PSQT[28][MG]", &QUEEN_PSQT[28][MG], params, &numParams);
  // addParam("QUEEN_PSQT[28][EG]", &QUEEN_PSQT[28][EG], params, &numParams);
  // addParam("QUEEN_PSQT[29][MG]", &QUEEN_PSQT[29][MG], params, &numParams);
  // addParam("QUEEN_PSQT[29][EG]", &QUEEN_PSQT[29][EG], params, &numParams);
  // addParam("QUEEN_PSQT[30][MG]", &QUEEN_PSQT[30][MG], params, &numParams);
  // addParam("QUEEN_PSQT[30][EG]", &QUEEN_PSQT[30][EG], params, &numParams);
  // addParam("QUEEN_PSQT[31][MG]", &QUEEN_PSQT[31][MG], params, &numParams);
  // addParam("QUEEN_PSQT[31][EG]", &QUEEN_PSQT[31][EG], params, &numParams);

  // addParam("KING_PSQT[0][MG]", &KING_PSQT[0][MG], params, &numParams);
  // addParam("KING_PSQT[0][EG]", &KING_PSQT[0][EG], params, &numParams);
  // addParam("KING_PSQT[1][MG]", &KING_PSQT[1][MG], params, &numParams);
  // addParam("KING_PSQT[1][EG]", &KING_PSQT[1][EG], params, &numParams);
  // addParam("KING_PSQT[2][MG]", &KING_PSQT[2][MG], params, &numParams);
  // addParam("KING_PSQT[2][EG]", &KING_PSQT[2][EG], params, &numParams);
  // addParam("KING_PSQT[3][MG]", &KING_PSQT[3][MG], params, &numParams);
  // addParam("KING_PSQT[3][EG]", &KING_PSQT[3][EG], params, &numParams);
  // addParam("KING_PSQT[4][MG]", &KING_PSQT[4][MG], params, &numParams);
  // addParam("KING_PSQT[4][EG]", &KING_PSQT[4][EG], params, &numParams);
  // addParam("KING_PSQT[5][MG]", &KING_PSQT[5][MG], params, &numParams);
  // addParam("KING_PSQT[5][EG]", &KING_PSQT[5][EG], params, &numParams);
  // addParam("KING_PSQT[6][MG]", &KING_PSQT[6][MG], params, &numParams);
  // addParam("KING_PSQT[6][EG]", &KING_PSQT[6][EG], params, &numParams);
  // addParam("KING_PSQT[7][MG]", &KING_PSQT[7][MG], params, &numParams);
  // addParam("KING_PSQT[7][EG]", &KING_PSQT[7][EG], params, &numParams);
  // addParam("KING_PSQT[8][MG]", &KING_PSQT[8][MG], params, &numParams);
  // addParam("KING_PSQT[8][EG]", &KING_PSQT[8][EG], params, &numParams);
  // addParam("KING_PSQT[9][MG]", &KING_PSQT[9][MG], params, &numParams);
  // addParam("KING_PSQT[9][EG]", &KING_PSQT[9][EG], params, &numParams);
  // addParam("KING_PSQT[10][MG]", &KING_PSQT[10][MG], params, &numParams);
  // addParam("KING_PSQT[10][EG]", &KING_PSQT[10][EG], params, &numParams);
  // addParam("KING_PSQT[11][MG]", &KING_PSQT[11][MG], params, &numParams);
  // addParam("KING_PSQT[11][EG]", &KING_PSQT[11][EG], params, &numParams);
  // addParam("KING_PSQT[12][MG]", &KING_PSQT[12][MG], params, &numParams);
  // addParam("KING_PSQT[12][EG]", &KING_PSQT[12][EG], params, &numParams);
  // addParam("KING_PSQT[13][MG]", &KING_PSQT[13][MG], params, &numParams);
  // addParam("KING_PSQT[13][EG]", &KING_PSQT[13][EG], params, &numParams);
  // addParam("KING_PSQT[14][MG]", &KING_PSQT[14][MG], params, &numParams);
  // addParam("KING_PSQT[14][EG]", &KING_PSQT[14][EG], params, &numParams);
  // addParam("KING_PSQT[15][MG]", &KING_PSQT[15][MG], params, &numParams);
  // addParam("KING_PSQT[15][EG]", &KING_PSQT[15][EG], params, &numParams);
  // addParam("KING_PSQT[16][MG]", &KING_PSQT[16][MG], params, &numParams);
  // addParam("KING_PSQT[16][EG]", &KING_PSQT[16][EG], params, &numParams);
  // addParam("KING_PSQT[17][MG]", &KING_PSQT[17][MG], params, &numParams);
  // addParam("KING_PSQT[17][EG]", &KING_PSQT[17][EG], params, &numParams);
  // addParam("KING_PSQT[18][MG]", &KING_PSQT[18][MG], params, &numParams);
  // addParam("KING_PSQT[18][EG]", &KING_PSQT[18][EG], params, &numParams);
  // addParam("KING_PSQT[19][MG]", &KING_PSQT[19][MG], params, &numParams);
  // addParam("KING_PSQT[19][EG]", &KING_PSQT[19][EG], params, &numParams);
  // addParam("KING_PSQT[20][MG]", &KING_PSQT[20][MG], params, &numParams);
  // addParam("KING_PSQT[20][EG]", &KING_PSQT[20][EG], params, &numParams);
  // addParam("KING_PSQT[21][MG]", &KING_PSQT[21][MG], params, &numParams);
  // addParam("KING_PSQT[21][EG]", &KING_PSQT[21][EG], params, &numParams);
  // addParam("KING_PSQT[22][MG]", &KING_PSQT[22][MG], params, &numParams);
  // addParam("KING_PSQT[22][EG]", &KING_PSQT[22][EG], params, &numParams);
  // addParam("KING_PSQT[23][MG]", &KING_PSQT[23][MG], params, &numParams);
  // addParam("KING_PSQT[23][EG]", &KING_PSQT[23][EG], params, &numParams);
  // addParam("KING_PSQT[24][MG]", &KING_PSQT[24][MG], params, &numParams);
  // addParam("KING_PSQT[24][EG]", &KING_PSQT[24][EG], params, &numParams);
  // addParam("KING_PSQT[25][MG]", &KING_PSQT[25][MG], params, &numParams);
  // addParam("KING_PSQT[25][EG]", &KING_PSQT[25][EG], params, &numParams);
  // addParam("KING_PSQT[26][MG]", &KING_PSQT[26][MG], params, &numParams);
  // addParam("KING_PSQT[26][EG]", &KING_PSQT[26][EG], params, &numParams);
  // addParam("KING_PSQT[27][MG]", &KING_PSQT[27][MG], params, &numParams);
  // addParam("KING_PSQT[27][EG]", &KING_PSQT[27][EG], params, &numParams);
  // addParam("KING_PSQT[28][MG]", &KING_PSQT[28][MG], params, &numParams);
  // addParam("KING_PSQT[28][EG]", &KING_PSQT[28][EG], params, &numParams);
  // addParam("KING_PSQT[29][MG]", &KING_PSQT[29][MG], params, &numParams);
  // addParam("KING_PSQT[29][EG]", &KING_PSQT[29][EG], params, &numParams);
  // addParam("KING_PSQT[30][MG]", &KING_PSQT[30][MG], params, &numParams);
  // addParam("KING_PSQT[30][EG]", &KING_PSQT[30][EG], params, &numParams);
  // addParam("KING_PSQT[31][MG]", &KING_PSQT[31][MG], params, &numParams);
  // addParam("KING_PSQT[31][EG]", &KING_PSQT[31][EG], params, &numParams);

  // addParam("KNIGHT_POST_PSQT[0][MG]", &KNIGHT_POST_PSQT[0][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[0][EG]", &KNIGHT_POST_PSQT[0][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[1][MG]", &KNIGHT_POST_PSQT[1][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[1][EG]", &KNIGHT_POST_PSQT[1][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[2][MG]", &KNIGHT_POST_PSQT[2][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[2][EG]", &KNIGHT_POST_PSQT[2][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[3][MG]", &KNIGHT_POST_PSQT[3][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[3][EG]", &KNIGHT_POST_PSQT[3][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[4][MG]", &KNIGHT_POST_PSQT[4][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[4][EG]", &KNIGHT_POST_PSQT[4][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[5][MG]", &KNIGHT_POST_PSQT[5][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[5][EG]", &KNIGHT_POST_PSQT[5][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[6][MG]", &KNIGHT_POST_PSQT[6][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[6][EG]", &KNIGHT_POST_PSQT[6][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[7][MG]", &KNIGHT_POST_PSQT[7][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[7][EG]", &KNIGHT_POST_PSQT[7][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[8][MG]", &KNIGHT_POST_PSQT[8][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[8][EG]", &KNIGHT_POST_PSQT[8][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[9][MG]", &KNIGHT_POST_PSQT[9][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[9][EG]", &KNIGHT_POST_PSQT[9][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[10][MG]", &KNIGHT_POST_PSQT[10][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[10][EG]", &KNIGHT_POST_PSQT[10][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[11][MG]", &KNIGHT_POST_PSQT[11][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[11][EG]", &KNIGHT_POST_PSQT[11][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[12][MG]", &KNIGHT_POST_PSQT[12][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[12][EG]", &KNIGHT_POST_PSQT[12][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[13][MG]", &KNIGHT_POST_PSQT[13][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[13][EG]", &KNIGHT_POST_PSQT[13][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[14][MG]", &KNIGHT_POST_PSQT[14][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[14][EG]", &KNIGHT_POST_PSQT[14][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[15][MG]", &KNIGHT_POST_PSQT[15][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[15][EG]", &KNIGHT_POST_PSQT[15][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[16][MG]", &KNIGHT_POST_PSQT[16][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[16][EG]", &KNIGHT_POST_PSQT[16][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[17][MG]", &KNIGHT_POST_PSQT[17][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[17][EG]", &KNIGHT_POST_PSQT[17][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[18][MG]", &KNIGHT_POST_PSQT[18][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[18][EG]", &KNIGHT_POST_PSQT[18][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[19][MG]", &KNIGHT_POST_PSQT[19][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[19][EG]", &KNIGHT_POST_PSQT[19][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[20][MG]", &KNIGHT_POST_PSQT[20][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[20][EG]", &KNIGHT_POST_PSQT[20][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[21][MG]", &KNIGHT_POST_PSQT[21][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[21][EG]", &KNIGHT_POST_PSQT[21][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[22][MG]", &KNIGHT_POST_PSQT[22][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[22][EG]", &KNIGHT_POST_PSQT[22][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[23][MG]", &KNIGHT_POST_PSQT[23][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[23][EG]", &KNIGHT_POST_PSQT[23][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[24][MG]", &KNIGHT_POST_PSQT[24][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[24][EG]", &KNIGHT_POST_PSQT[24][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[25][MG]", &KNIGHT_POST_PSQT[25][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[25][EG]", &KNIGHT_POST_PSQT[25][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[26][MG]", &KNIGHT_POST_PSQT[26][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[26][EG]", &KNIGHT_POST_PSQT[26][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[27][MG]", &KNIGHT_POST_PSQT[27][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[27][EG]", &KNIGHT_POST_PSQT[27][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[28][MG]", &KNIGHT_POST_PSQT[28][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[28][EG]", &KNIGHT_POST_PSQT[28][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[29][MG]", &KNIGHT_POST_PSQT[29][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[29][EG]", &KNIGHT_POST_PSQT[29][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[30][MG]", &KNIGHT_POST_PSQT[30][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[30][EG]", &KNIGHT_POST_PSQT[30][EG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[31][MG]", &KNIGHT_POST_PSQT[31][MG], params, &numParams);
  // addParam("KNIGHT_POST_PSQT[31][EG]", &KNIGHT_POST_PSQT[31][EG], params, &numParams);

  // addParam("KNIGHT_MOBILITIES[0][MG]", &KNIGHT_MOBILITIES[0][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[0][EG]", &KNIGHT_MOBILITIES[0][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[1][MG]", &KNIGHT_MOBILITIES[1][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[1][EG]", &KNIGHT_MOBILITIES[1][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[2][MG]", &KNIGHT_MOBILITIES[2][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[2][EG]", &KNIGHT_MOBILITIES[2][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[3][MG]", &KNIGHT_MOBILITIES[3][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[3][EG]", &KNIGHT_MOBILITIES[3][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[4][MG]", &KNIGHT_MOBILITIES[4][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[4][EG]", &KNIGHT_MOBILITIES[4][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[5][MG]", &KNIGHT_MOBILITIES[5][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[5][EG]", &KNIGHT_MOBILITIES[5][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[6][MG]", &KNIGHT_MOBILITIES[6][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[6][EG]", &KNIGHT_MOBILITIES[6][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[7][MG]", &KNIGHT_MOBILITIES[7][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[7][EG]", &KNIGHT_MOBILITIES[7][EG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[8][MG]", &KNIGHT_MOBILITIES[8][MG], params, &numParams);
  // addParam("KNIGHT_MOBILITIES[8][EG]", &KNIGHT_MOBILITIES[8][EG], params, &numParams);

  // addParam("BISHOP_MOBILITIES[0][MG]", &BISHOP_MOBILITIES[0][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[0][EG]", &BISHOP_MOBILITIES[0][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[1][MG]", &BISHOP_MOBILITIES[1][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[1][EG]", &BISHOP_MOBILITIES[1][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[2][MG]", &BISHOP_MOBILITIES[2][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[2][EG]", &BISHOP_MOBILITIES[2][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[3][MG]", &BISHOP_MOBILITIES[3][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[3][EG]", &BISHOP_MOBILITIES[3][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[4][MG]", &BISHOP_MOBILITIES[4][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[4][EG]", &BISHOP_MOBILITIES[4][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[5][MG]", &BISHOP_MOBILITIES[5][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[5][EG]", &BISHOP_MOBILITIES[5][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[6][MG]", &BISHOP_MOBILITIES[6][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[6][EG]", &BISHOP_MOBILITIES[6][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[7][MG]", &BISHOP_MOBILITIES[7][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[7][EG]", &BISHOP_MOBILITIES[7][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[8][MG]", &BISHOP_MOBILITIES[8][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[8][EG]", &BISHOP_MOBILITIES[8][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[9][MG]", &BISHOP_MOBILITIES[9][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[9][EG]", &BISHOP_MOBILITIES[9][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[10][MG]", &BISHOP_MOBILITIES[10][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[10][EG]", &BISHOP_MOBILITIES[10][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[11][MG]", &BISHOP_MOBILITIES[11][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[11][EG]", &BISHOP_MOBILITIES[11][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[12][MG]", &BISHOP_MOBILITIES[12][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[12][EG]", &BISHOP_MOBILITIES[12][EG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[13][MG]", &BISHOP_MOBILITIES[13][MG], params, &numParams);
  // addParam("BISHOP_MOBILITIES[13][EG]", &BISHOP_MOBILITIES[13][EG], params, &numParams);

  // addParam("ROOK_MOBILITIES[0][MG]", &ROOK_MOBILITIES[0][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[0][EG]", &ROOK_MOBILITIES[0][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[1][MG]", &ROOK_MOBILITIES[1][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[1][EG]", &ROOK_MOBILITIES[1][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[2][MG]", &ROOK_MOBILITIES[2][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[2][EG]", &ROOK_MOBILITIES[2][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[3][MG]", &ROOK_MOBILITIES[3][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[3][EG]", &ROOK_MOBILITIES[3][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[4][MG]", &ROOK_MOBILITIES[4][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[4][EG]", &ROOK_MOBILITIES[4][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[5][MG]", &ROOK_MOBILITIES[5][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[5][EG]", &ROOK_MOBILITIES[5][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[6][MG]", &ROOK_MOBILITIES[6][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[6][EG]", &ROOK_MOBILITIES[6][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[7][MG]", &ROOK_MOBILITIES[7][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[7][EG]", &ROOK_MOBILITIES[7][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[8][MG]", &ROOK_MOBILITIES[8][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[8][EG]", &ROOK_MOBILITIES[8][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[9][MG]", &ROOK_MOBILITIES[9][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[9][EG]", &ROOK_MOBILITIES[9][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[10][MG]", &ROOK_MOBILITIES[10][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[10][EG]", &ROOK_MOBILITIES[10][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[11][MG]", &ROOK_MOBILITIES[11][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[11][EG]", &ROOK_MOBILITIES[11][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[12][MG]", &ROOK_MOBILITIES[12][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[12][EG]", &ROOK_MOBILITIES[12][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[13][MG]", &ROOK_MOBILITIES[13][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[13][EG]", &ROOK_MOBILITIES[13][EG], params, &numParams);
  // addParam("ROOK_MOBILITIES[14][MG]", &ROOK_MOBILITIES[14][MG], params, &numParams);
  // addParam("ROOK_MOBILITIES[14][EG]", &ROOK_MOBILITIES[14][EG], params, &numParams);

  // addParam("QUEEN_MOBILITIES[0][MG]", &QUEEN_MOBILITIES[0][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[0][EG]", &QUEEN_MOBILITIES[0][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[1][MG]", &QUEEN_MOBILITIES[1][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[1][EG]", &QUEEN_MOBILITIES[1][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[2][MG]", &QUEEN_MOBILITIES[2][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[2][EG]", &QUEEN_MOBILITIES[2][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[3][MG]", &QUEEN_MOBILITIES[3][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[3][EG]", &QUEEN_MOBILITIES[3][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[4][MG]", &QUEEN_MOBILITIES[4][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[4][EG]", &QUEEN_MOBILITIES[4][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[5][MG]", &QUEEN_MOBILITIES[5][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[5][EG]", &QUEEN_MOBILITIES[5][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[6][MG]", &QUEEN_MOBILITIES[6][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[6][EG]", &QUEEN_MOBILITIES[6][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[7][MG]", &QUEEN_MOBILITIES[7][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[7][EG]", &QUEEN_MOBILITIES[7][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[8][MG]", &QUEEN_MOBILITIES[8][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[8][EG]", &QUEEN_MOBILITIES[8][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[9][MG]", &QUEEN_MOBILITIES[9][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[9][EG]", &QUEEN_MOBILITIES[9][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[10][MG]", &QUEEN_MOBILITIES[10][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[10][EG]", &QUEEN_MOBILITIES[10][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[11][MG]", &QUEEN_MOBILITIES[11][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[11][EG]", &QUEEN_MOBILITIES[11][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[12][MG]", &QUEEN_MOBILITIES[12][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[12][EG]", &QUEEN_MOBILITIES[12][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[13][MG]", &QUEEN_MOBILITIES[13][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[13][EG]", &QUEEN_MOBILITIES[13][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[14][MG]", &QUEEN_MOBILITIES[14][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[14][EG]", &QUEEN_MOBILITIES[14][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[15][MG]", &QUEEN_MOBILITIES[15][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[15][EG]", &QUEEN_MOBILITIES[15][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[16][MG]", &QUEEN_MOBILITIES[16][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[16][EG]", &QUEEN_MOBILITIES[16][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[17][MG]", &QUEEN_MOBILITIES[17][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[17][EG]", &QUEEN_MOBILITIES[17][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[18][MG]", &QUEEN_MOBILITIES[18][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[18][EG]", &QUEEN_MOBILITIES[18][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[19][MG]", &QUEEN_MOBILITIES[19][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[19][EG]", &QUEEN_MOBILITIES[19][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[20][MG]", &QUEEN_MOBILITIES[20][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[20][EG]", &QUEEN_MOBILITIES[20][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[21][MG]", &QUEEN_MOBILITIES[21][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[21][EG]", &QUEEN_MOBILITIES[21][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[22][MG]", &QUEEN_MOBILITIES[22][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[22][EG]", &QUEEN_MOBILITIES[22][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[23][MG]", &QUEEN_MOBILITIES[23][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[23][EG]", &QUEEN_MOBILITIES[23][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[24][MG]", &QUEEN_MOBILITIES[24][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[24][EG]", &QUEEN_MOBILITIES[24][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[25][MG]", &QUEEN_MOBILITIES[25][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[25][EG]", &QUEEN_MOBILITIES[25][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[26][MG]", &QUEEN_MOBILITIES[26][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[26][EG]", &QUEEN_MOBILITIES[26][EG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[27][MG]", &QUEEN_MOBILITIES[27][MG], params, &numParams);
  // addParam("QUEEN_MOBILITIES[27][EG]", &QUEEN_MOBILITIES[27][EG], params, &numParams);

  // addParam("BISHOP_PAIR[MG]", &BISHOP_PAIR[MG], params, &numParams);
  // addParam("BISHOP_PAIR[EG]", &BISHOP_PAIR[EG], params, &numParams);

  // addParam("DOUBLED_PAWN[MG]", &DOUBLED_PAWN[MG], params, &numParams);
  // addParam("DOUBLED_PAWN[EG]", &DOUBLED_PAWN[EG], params, &numParams);
  // addParam("OPPOSED_ISOLATED_PAWN[MG]", &OPPOSED_ISOLATED_PAWN[MG], params, &numParams);
  // addParam("OPPOSED_ISOLATED_PAWN[EG]", &OPPOSED_ISOLATED_PAWN[EG], params, &numParams);
  // addParam("OPEN_ISOLATED_PAWN[MG]", &OPEN_ISOLATED_PAWN[MG], params, &numParams);
  // addParam("OPEN_ISOLATED_PAWN[EG]", &OPEN_ISOLATED_PAWN[EG], params, &numParams);
  // addParam("BACKWARDS_PAWN[MG]", &BACKWARDS_PAWN[MG], params, &numParams);
  // addParam("BACKWARDS_PAWN[EG]", &BACKWARDS_PAWN[EG], params, &numParams);
  // addParam("DEFENDED_PAWN[MG]", &DEFENDED_PAWN[MG], params, &numParams);
  // addParam("DEFENDED_PAWN[EG]", &DEFENDED_PAWN[EG], params, &numParams);

  // addParam("CONNECTED_PAWN[1][MG]", &CONNECTED_PAWN[1][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[1][EG]", &CONNECTED_PAWN[1][EG], params, &numParams);
  // addParam("CONNECTED_PAWN[2][MG]", &CONNECTED_PAWN[2][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[2][EG]", &CONNECTED_PAWN[2][EG], params, &numParams);
  // addParam("CONNECTED_PAWN[3][MG]", &CONNECTED_PAWN[3][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[3][EG]", &CONNECTED_PAWN[3][EG], params, &numParams);
  // addParam("CONNECTED_PAWN[4][MG]", &CONNECTED_PAWN[4][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[4][EG]", &CONNECTED_PAWN[4][EG], params, &numParams);
  // addParam("CONNECTED_PAWN[5][MG]", &CONNECTED_PAWN[5][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[5][EG]", &CONNECTED_PAWN[5][EG], params, &numParams);
  // addParam("CONNECTED_PAWN[6][MG]", &CONNECTED_PAWN[6][MG], params, &numParams);
  // addParam("CONNECTED_PAWN[6][EG]", &CONNECTED_PAWN[6][EG], params, &numParams);

  // addParam("PASSED_PAWN[1][MG]", &PASSED_PAWN[1][MG], params, &numParams);
  // addParam("PASSED_PAWN[1][EG]", &PASSED_PAWN[1][EG], params, &numParams);
  // addParam("PASSED_PAWN[2][MG]", &PASSED_PAWN[2][MG], params, &numParams);
  // addParam("PASSED_PAWN[2][EG]", &PASSED_PAWN[2][EG], params, &numParams);
  // addParam("PASSED_PAWN[3][MG]", &PASSED_PAWN[3][MG], params, &numParams);
  // addParam("PASSED_PAWN[3][EG]", &PASSED_PAWN[3][EG], params, &numParams);
  // addParam("PASSED_PAWN[4][MG]", &PASSED_PAWN[4][MG], params, &numParams);
  // addParam("PASSED_PAWN[4][EG]", &PASSED_PAWN[4][EG], params, &numParams);
  // addParam("PASSED_PAWN[5][MG]", &PASSED_PAWN[5][MG], params, &numParams);
  // addParam("PASSED_PAWN[5][EG]", &PASSED_PAWN[5][EG], params, &numParams);
  // addParam("PASSED_PAWN[6][MG]", &PASSED_PAWN[6][MG], params, &numParams);
  // addParam("PASSED_PAWN[6][EG]", &PASSED_PAWN[6][EG], params, &numParams);

  // addParam("DEFENDED_MINOR[MG]", &DEFENDED_MINOR[MG], params, &numParams);
  // addParam("DEFENDED_MINOR[EG]", &DEFENDED_MINOR[EG], params, &numParams);

  // addParam("ROOK_OPEN_FILE[MG]", &ROOK_OPEN_FILE[MG], params, &numParams);
  // addParam("ROOK_OPEN_FILE[EG]", &ROOK_OPEN_FILE[EG], params, &numParams);
  // addParam("ROOK_SEMI_OPEN[MG]", &ROOK_SEMI_OPEN[MG], params, &numParams);
  // addParam("ROOK_SEMI_OPEN[EG]", &ROOK_SEMI_OPEN[EG], params, &numParams);
  // addParam("ROOK_SEVENTH_RANK[MG]", &ROOK_SEVENTH_RANK[MG], params, &numParams);
  // addParam("ROOK_SEVENTH_RANK[EG]", &ROOK_SEVENTH_RANK[EG], params, &numParams);
  // addParam("ROOK_OPPOSITE_KING[MG]", &ROOK_OPPOSITE_KING[MG], params, &numParams);
  // addParam("ROOK_OPPOSITE_KING[EG]", &ROOK_OPPOSITE_KING[EG], params, &numParams);
  // addParam("ROOK_ADJACENT_KING[MG]", &ROOK_ADJACENT_KING[MG], params, &numParams);
  // addParam("ROOK_ADJACENT_KING[EG]", &ROOK_ADJACENT_KING[EG], params, &numParams);

  // addParam("ROOK_TRAPPED[MG]", &ROOK_TRAPPED[MG], params, &numParams);
  // addParam("ROOK_TRAPPED[EG]", &ROOK_TRAPPED[EG], params, &numParams);
  // addParam("BISHOP_TRAPPED[MG]", &BISHOP_TRAPPED[MG], params, &numParams);
  // addParam("BISHOP_TRAPPED[EG]", &BISHOP_TRAPPED[EG], params, &numParams);

  // addParam("KNIGHT_THREATS[0][MG]", &KNIGHT_THREATS[0][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[0][EG]", &KNIGHT_THREATS[0][EG], params, &numParams);
  // addParam("KNIGHT_THREATS[1][MG]", &KNIGHT_THREATS[1][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[1][EG]", &KNIGHT_THREATS[1][EG], params, &numParams);
  // addParam("KNIGHT_THREATS[2][MG]", &KNIGHT_THREATS[2][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[2][EG]", &KNIGHT_THREATS[2][EG], params, &numParams);
  // addParam("KNIGHT_THREATS[3][MG]", &KNIGHT_THREATS[3][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[3][EG]", &KNIGHT_THREATS[3][EG], params, &numParams);
  // addParam("KNIGHT_THREATS[4][MG]", &KNIGHT_THREATS[4][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[4][EG]", &KNIGHT_THREATS[4][EG], params, &numParams);
  // addParam("KNIGHT_THREATS[5][MG]", &KNIGHT_THREATS[5][MG], params, &numParams);
  // addParam("KNIGHT_THREATS[5][EG]", &KNIGHT_THREATS[5][EG], params, &numParams);

  // addParam("BISHOP_THREATS[0][MG]", &BISHOP_THREATS[0][MG], params, &numParams);
  // addParam("BISHOP_THREATS[0][EG]", &BISHOP_THREATS[0][EG], params, &numParams);
  // addParam("BISHOP_THREATS[1][MG]", &BISHOP_THREATS[1][MG], params, &numParams);
  // addParam("BISHOP_THREATS[1][EG]", &BISHOP_THREATS[1][EG], params, &numParams);
  // addParam("BISHOP_THREATS[2][MG]", &BISHOP_THREATS[2][MG], params, &numParams);
  // addParam("BISHOP_THREATS[2][EG]", &BISHOP_THREATS[2][EG], params, &numParams);
  // addParam("BISHOP_THREATS[3][MG]", &BISHOP_THREATS[3][MG], params, &numParams);
  // addParam("BISHOP_THREATS[3][EG]", &BISHOP_THREATS[3][EG], params, &numParams);
  // addParam("BISHOP_THREATS[4][MG]", &BISHOP_THREATS[4][MG], params, &numParams);
  // addParam("BISHOP_THREATS[4][EG]", &BISHOP_THREATS[4][EG], params, &numParams);
  // addParam("BISHOP_THREATS[5][MG]", &BISHOP_THREATS[5][MG], params, &numParams);
  // addParam("BISHOP_THREATS[5][EG]", &BISHOP_THREATS[5][EG], params, &numParams);

  // addParam("ROOK_THREATS[0][MG]", &ROOK_THREATS[0][MG], params, &numParams);
  // addParam("ROOK_THREATS[0][EG]", &ROOK_THREATS[0][EG], params, &numParams);
  // addParam("ROOK_THREATS[1][MG]", &ROOK_THREATS[1][MG], params, &numParams);
  // addParam("ROOK_THREATS[1][EG]", &ROOK_THREATS[1][EG], params, &numParams);
  // addParam("ROOK_THREATS[2][MG]", &ROOK_THREATS[2][MG], params, &numParams);
  // addParam("ROOK_THREATS[2][EG]", &ROOK_THREATS[2][EG], params, &numParams);
  // addParam("ROOK_THREATS[3][MG]", &ROOK_THREATS[3][MG], params, &numParams);
  // addParam("ROOK_THREATS[3][EG]", &ROOK_THREATS[3][EG], params, &numParams);
  // addParam("ROOK_THREATS[4][MG]", &ROOK_THREATS[4][MG], params, &numParams);
  // addParam("ROOK_THREATS[4][EG]", &ROOK_THREATS[4][EG], params, &numParams);
  // addParam("ROOK_THREATS[5][MG]", &ROOK_THREATS[5][MG], params, &numParams);
  // addParam("ROOK_THREATS[5][EG]", &ROOK_THREATS[5][EG], params, &numParams);

  // addParam("KING_THREATS[0][MG]", &KING_THREATS[0][MG], params, &numParams);
  // addParam("KING_THREATS[0][EG]", &KING_THREATS[0][EG], params, &numParams);
  // addParam("KING_THREATS[1][MG]", &KING_THREATS[1][MG], params, &numParams);
  // addParam("KING_THREATS[1][EG]", &KING_THREATS[1][EG], params, &numParams);
  // addParam("KING_THREATS[2][MG]", &KING_THREATS[2][MG], params, &numParams);
  // addParam("KING_THREATS[2][EG]", &KING_THREATS[2][EG], params, &numParams);
  // addParam("KING_THREATS[3][MG]", &KING_THREATS[3][MG], params, &numParams);
  // addParam("KING_THREATS[3][EG]", &KING_THREATS[3][EG], params, &numParams);
  // addParam("KING_THREATS[4][MG]", &KING_THREATS[4][MG], params, &numParams);
  // addParam("KING_THREATS[4][EG]", &KING_THREATS[4][EG], params, &numParams);
  // addParam("KING_THREATS[5][MG]", &KING_THREATS[5][MG], params, &numParams);
  // addParam("KING_THREATS[5][EG]", &KING_THREATS[5][EG], params, &numParams);

  // addParam("HANGING_THREAT[MG]", &HANGING_THREAT[MG], params, &numParams);
  // addParam("HANGING_THREAT[EG]", &HANGING_THREAT[EG], params, &numParams);

  // addParam("PAWN_SHELTER[0][0][MG]", &PAWN_SHELTER[0][0][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][0][EG]", &PAWN_SHELTER[0][0][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][1][MG]", &PAWN_SHELTER[0][1][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][1][EG]", &PAWN_SHELTER[0][1][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][2][MG]", &PAWN_SHELTER[0][2][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][2][EG]", &PAWN_SHELTER[0][2][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][3][MG]", &PAWN_SHELTER[0][3][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][3][EG]", &PAWN_SHELTER[0][3][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][4][MG]", &PAWN_SHELTER[0][4][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][4][EG]", &PAWN_SHELTER[0][4][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][5][MG]", &PAWN_SHELTER[0][5][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][5][EG]", &PAWN_SHELTER[0][5][EG], params, &numParams);
  // addParam("PAWN_SHELTER[0][6][MG]", &PAWN_SHELTER[0][6][MG], params, &numParams);
  // addParam("PAWN_SHELTER[0][6][EG]", &PAWN_SHELTER[0][6][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][0][MG]", &PAWN_SHELTER[1][0][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][0][EG]", &PAWN_SHELTER[1][0][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][1][MG]", &PAWN_SHELTER[1][1][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][1][EG]", &PAWN_SHELTER[1][1][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][2][MG]", &PAWN_SHELTER[1][2][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][2][EG]", &PAWN_SHELTER[1][2][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][3][MG]", &PAWN_SHELTER[1][3][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][3][EG]", &PAWN_SHELTER[1][3][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][4][MG]", &PAWN_SHELTER[1][4][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][4][EG]", &PAWN_SHELTER[1][4][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][5][MG]", &PAWN_SHELTER[1][5][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][5][EG]", &PAWN_SHELTER[1][5][EG], params, &numParams);
  // addParam("PAWN_SHELTER[1][6][MG]", &PAWN_SHELTER[1][6][MG], params, &numParams);
  // addParam("PAWN_SHELTER[1][6][EG]", &PAWN_SHELTER[1][6][EG], params, &numParams);

  // addParam("PAWN_STORM[2][MG]", &PAWN_STORM[2][MG], params, &numParams);
  // addParam("PAWN_STORM[2][EG]", &PAWN_STORM[2][EG], params, &numParams);
  // addParam("PAWN_STORM[3][MG]", &PAWN_STORM[3][MG], params, &numParams);
  // addParam("PAWN_STORM[3][EG]", &PAWN_STORM[3][EG], params, &numParams);
  // addParam("PAWN_STORM[4][MG]", &PAWN_STORM[4][MG], params, &numParams);
  // addParam("PAWN_STORM[4][EG]", &PAWN_STORM[4][EG], params, &numParams);
  // addParam("PAWN_STORM[5][MG]", &PAWN_STORM[5][MG], params, &numParams);
  // addParam("PAWN_STORM[5][EG]", &PAWN_STORM[5][EG], params, &numParams);
  // addParam("PAWN_STORM[6][MG]", &PAWN_STORM[6][MG], params, &numParams);
  // addParam("PAWN_STORM[6][EG]", &PAWN_STORM[6][EG], params, &numParams);

  // addParam("KS_ATTACKER_WEIGHTS[1]", &KS_ATTACKER_WEIGHTS[1], params, &numParams);
  // addParam("KS_ATTACKER_WEIGHTS[2]", &KS_ATTACKER_WEIGHTS[2], params, &numParams);
  // addParam("KS_ATTACKER_WEIGHTS[3]", &KS_ATTACKER_WEIGHTS[3], params, &numParams);
  // addParam("KS_ATTACKER_WEIGHTS[4]", &KS_ATTACKER_WEIGHTS[4], params, &numParams);

  // addParam("KS_ATTACK", &KS_ATTACK, params, &numParams);
  // addParam("KS_WEAK_SQS", &KS_WEAK_SQS, params, &numParams);
  // addParam("KS_SAFE_CHECK", &KS_SAFE_CHECK, params, &numParams);
  // addParam("KS_UNSAFE_CHECK", &KS_UNSAFE_CHECK, params, &numParams);
  // addParam("KS_ENEMY_QUEEN", &KS_ENEMY_QUEEN, params, &numParams);
  // addParam("KS_ALLIES", &KS_ALLIES, params, &numParams);

  printf("Running texel tuning on %d parameters...", numParams);

  determineK(positions, n);
  SGD(params, numParams, positions, n);

  free(positions);
}

void SGD(TexelParam* params, int numParams, Position* positions, int numPositions) {
  const double a = 3;
  const double b1 = 0.9;
  const double b2 = 0.999;
  const double epsilon = 1.0e-8;

  double best = totalError(positions, numPositions);

  double* gradients = calloc(numParams, sizeof(double));
  double* M = calloc(numParams, sizeof(double));
  double* V = calloc(numParams, sizeof(double));

  for (int epoch = 1; epoch <= 250; epoch++) {
    printf("\n\nEpoch %d\n\n", epoch);
    shuffle(positions, numPositions);

    double base = totalError(positions, numPositions);

    CalculateGradients(gradients, params, numParams, positions, numPositions, base);

    for (int k = 0; k < numParams; k++) {
      double gradient = gradients[k];

      M[k] = b1 * M[k] + (1.0 - b1) * gradient;
      V[k] = b2 * V[k] + (1.0 - b2) * gradient * gradient;

      double rate = a * (1.0 - pow(b2, epoch)) / (1.0 - pow(b1, epoch));
      double delta = rate * M[k] / (sqrt(V[k]) + epsilon);

      const Score oldValue = *params[k].param;
      *params[k].param = *params[k].param - delta;

      if (*params[k].param != oldValue)
        printf("%-30s: (%.12f, %.12f)\n", params[k].name, oldValue, *params[k].param);
    }

    double curr = totalError(positions, numPositions);
    printf("\nBase: %.12f, Current: %.12f, Diff: %.12f\n", base, curr, (base - curr) * 10e6);

    if (epoch % 10 == 0) {
      double completed = totalError(positions, numPositions);
      PrintParams(params, numParams, best, completed, epoch);
      if (completed == best)
        break;
      else
        best = completed;
    }
  }
}

void CalculateGradients(double* gradients, TexelParam* params, int numParams, Position* positions, int numPositions,
                        double base) {
  for (int i = 0; i < numParams; i++) {
    TexelParam param = params[i];
    const Score oldValue = *param.param;

    *param.param += 1;

    double ep1 = totalError(positions, numPositions);
    gradients[i] = ep1 - base;

    *param.param = oldValue;
  }
}

Position* loadPositions(int* n) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = malloc(sizeof(Position) * 8000000);

  int buffersize = 100;
  char buffer[100];

  int p = 0;
  while (fgets(buffer, buffersize, fp)) {
    int i;
    for (i = 0; i < buffersize; i++)
      if (buffer[i] == '"')
        break;

    memcpy(positions[p].fen, buffer, i * sizeof(char));

    i++;
    if (buffer[i] == '0') // 0-1
      positions[p].result = 0.0;
    else {
      i++;
      if (buffer[i] == '/') // 1/2-1/2
        positions[p].result = 0.5;
      else // 1-0
        positions[p].result = 1.0;
    }

    p++;
  }

  *n = p;

  return positions;
}

void determineK(Position* positions, int n) {
  double min = -10, max = 10, delta = 1, best = 1, error = 100;

  for (int p = 0; p < 7; p++) {
    printf("Determining K: (%.6f, %.6f, %.7f)\n", min, max, delta);

    while (min < max) {
      K = min;
      double e = totalError(positions, n);
      if (e < error) {
        error = e;
        best = K;
        printf("New best K of %.6f, Error %.10f\n", K, error);
      }
      min += delta;
    }

    min = best - delta;
    max = best + delta;
    delta /= 10;
  }

  K = best;
  printf("Using K of %.6f\n", K);
}

double totalError(Position* positions, int n) {
  pthread_t threads[THREADS];
  BatchJob jobs[THREADS];

  int chunkSize = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    jobs[t].positions = positions + (t * chunkSize);
    jobs[t].n = t != THREADS - 1 ? chunkSize : (n - ((THREADS - 1) * chunkSize));
    jobs[t].threadid = t;

    pthread_create(&threads[t], NULL, batchError, (void*)(jobs + t));
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  double sum = 0.0, c = 0.0;
  for (int t = 0; t < THREADS; t++) {
    double e = jobs[t].totalError;

    double t1 = sum + e;
    if (fabs(sum) >= fabs(e))
      c += (sum - t1) + e;
    else
      c += (e - t1) + sum;

    sum = t1;
  }

  return (sum + c) / n;
}

void* batchError(void* arg) {
  BatchJob* batch = (BatchJob*)arg;

  double sum = 0.0, c = 0.0;
  for (int i = 0; i < batch->n; i++) {
    double e = error(batch->positions + i);
    double t = sum + e;
    if (fabs(sum) >= fabs(e))
      c += (sum - t) + e;
    else
      c += (e - t) + sum;

    sum = t;
  }

  batch->totalError = sum;

  pthread_exit(NULL);
}

double error(Position* p) {
  Board* board = malloc(sizeof(Board));
  parseFen(p->fen, board);

  Score score = sideScalar[board->side] * Evaluate(board);

  free(board);
  return pow(p->result - sigmoid(score), 2);
}

double sigmoid(Score score) { return (double)1 / (1 + pow(10, K * score / 400)); }

void shuffle(Position* array, int n) {
  if (n > 1) {
    for (int i = 0; i < n - 1; i++) {
      int j = i + rand() / (RAND_MAX / (n - i) + 1);
      Position temp = array[j];
      array[j] = array[i];
      array[i] = temp;
    }
  }
}

void PrintParams(TexelParam* params, int numParams, double best, double current, int epoch) {
  printf("\n\nCurrent Values at Epoch %d:\n", epoch);
  printf("Start E: %.12f - New E: %.12f - Improvement: %.12f\n", best, current, (best - current) * 10e6);

  for (int i = 0; i < numParams; i++)
    printf("%-30s: %.12f\n", params[i].name, *params[i].param);

  FILE* fp;

  fp = fopen("texel-output.txt", "a");
  if (fp == NULL)
    return;

  fprintf(fp, "Current Values at Epoch %d:\n", epoch);
  fprintf(fp, "Start E: %.12f - New E: %.12f - Improvement: %.12f\n", best, current, (best - current) * 10e6);

  for (int i = 0; i < numParams; i++)
    fprintf(fp, "%-30s: %.12f\n", params[i].name, *params[i].param);

  fprintf(fp, "\n\n");
  fclose(fp);
}

#endif