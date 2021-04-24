// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "string.h"
#include "types.h"
#include "util.h"

#define S(mg, eg) (makeScore((mg), (eg)))
#define rel(sq, side) ((side) ? MIRROR[(sq)] : (sq))
#define distance(a, b) max(abs(rank(a) - rank(b)), abs(file(a) - file(b)))
#define psqtIdx(sq) ((rank((sq)) << 2) + (file((sq)) > 3 ? file((sq)) ^ 7 : file((sq))))

const Score UNKNOWN = INT32_MAX;
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};
const uint8_t kpkResults[2 * 64 * 64 * 24 / 8] = {
#include "kpk.h"
};

// Phase generation calculation is standard from CPW
const Score PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};
const Score MAX_PHASE = 24;

const int STATIC_MATERIAL_VALUE[7] = {93, 310, 323, 548, 970, 30000, 0};

// clang-format off
Score MATERIAL_VALUES[7] = {S(60, 93), S(310, 310), S(323, 323), S(435, 548), S(910, 970), S(0, 0), S(0, 0)};

Score PAWN_PSQT[32] = {S(0,0), S(0,0), S(0,0), S(0,0), S(55,130), S(21,145), S(59,127), S(82,117), S(2,13), S(-6,12), S(21,-9), S(21,-18), S(-14,0), S(-2,-5), S(-3,-9), S(13,-11), S(-17,-8), S(-13,-6), S(-1,-8), S(4,-4), S(-16,-10), S(4,-11), S(-3,-5), S(7,-1), S(-15,-10), S(-2,-11), S(-11,-1), S(2,7), S(0,0), S(0,0), S(0,0), S(0,0)};
Score KNIGHT_PSQT[32] = {S(-75,-61), S(-26,-31), S(-25,-19), S(14,-20), S(0,-24), S(1,-5), S(36,-14), S(28,-7), S(15,-25), S(8,-5), S(17,5), S(37,-5), S(22,-8), S(12,-3), S(24,1), S(30,3), S(21,-4), S(24,-7), S(26,3), S(28,6), S(5,-5), S(17,-6), S(13,0), S(24,9), S(15,7), S(20,-6), S(14,-2), S(18,0), S(-4,2), S(8,-6), S(14,-3), S(17,2)};
Score BISHOP_PSQT[32] = {S(-29,-5), S(-23,0), S(-67,4), S(-78,6), S(-21,-5), S(-26,0), S(-13,0), S(-8,-4), S(-12,-4), S(-19,-4), S(-8,-4), S(-1,-9), S(-28,1), S(-6,-6), S(-8,-4), S(4,-3), S(2,-11), S(-14,-7), S(-9,-1), S(0,-5), S(-6,-5), S(6,0), S(-3,1), S(-4,5), S(3,-8), S(9,-8), S(2,-9), S(-10,2), S(0,-7), S(3,-4), S(-6,2), S(-7,1)};
Score ROOK_PSQT[32] = {S(10,18), S(5,21), S(-3,28), S(-15,30), S(13,0), S(6,10), S(22,5), S(23,0), S(3,9), S(38,4), S(35,6), S(36,1), S(-2,15), S(9,11), S(24,9), S(21,8), S(-4,13), S(6,8), S(1,15), S(8,13), S(-3,9), S(9,3), S(10,8), S(9,8), S(-3,13), S(8,6), S(14,6), S(15,8), S(10,5), S(7,6), S(9,7), S(14,3)};
Score QUEEN_PSQT[32] = {S(-42,3), S(-33,1), S(-4,-1), S(-17,21), S(-69,38), S(-92,66), S(-72,81), S(-71,61), S(-55,24), S(-61,56), S(-62,62), S(-57,67), S(-72,65), S(-68,79), S(-68,72), S(-75,92), S(-66,59), S(-67,74), S(-72,80), S(-77,96), S(-68,53), S(-62,54), S(-71,71), S(-71,65), S(-63,29), S(-58,17), S(-60,34), S(-61,45), S(-58,13), S(-72,30), S(-73,30), S(-67,27)};
Score KING_PSQT[32] = {S(-103,-162), S(38,-131), S(12,-104), S(-41,-102), S(0,-115), S(82,-89), S(80,-83), S(77,-91), S(6,-113), S(128,-101), S(108,-90), S(38,-84), S(-51,-106), S(62,-100), S(17,-81), S(-25,-74), S(-96,-98), S(-2,-96), S(1,-85), S(-34,-78), S(-50,-104), S(-5,-97), S(0,-92), S(-10,-87), S(-60,-112), S(-48,-97), S(-49,-87), S(-66,-83), S(-64,-140), S(-58,-117), S(-52,-103), S(-55,-106)};
Score BISHOP_PAIR = S(22,43);
Score BISHOP_TRAPPED = S(-116,-79);
Score BISHOP_POST_PSQT[32] = {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(15,10), S(21,12), S(24,21), S(23,10), S(13,10), S(23,17), S(23,14), S(23,14), S(2,13), S(21,16), S(22,12), S(23,20), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)};
Score KNIGHT_POST_PSQT[32] = {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,12), S(35,4), S(34,9), S(26,27), S(22,5), S(44,15), S(37,23), S(44,24), S(22,1), S(34,12), S(24,18), S(25,26), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)};
Score KNIGHT_OUTPOST_REACHABLE = S(4,14);
Score BISHOP_OUTPOST_REACHABLE = S(4,4);
Score KNIGHT_MOBILITIES[9] = {S(-95,-71), S(-76,-30), S(-64,-6), S(-56,2), S(-47,7), S(-42,11), S(-36,12), S(-30,9), S(-25,1)};
Score BISHOP_MOBILITIES[14] = {S(-62,-43), S(-48,-30), S(-39,-18), S(-32,-3), S(-23,4), S(-18,11), S(-16,15), S(-14,18), S(-12,21), S(-11,24), S(-6,20), S(7,21), S(11,29), S(12,26)};
Score ROOK_MOBILITIES[15] = {S(-90,-120), S(-69,-32), S(-60,-17), S(-54,-20), S(-57,-4), S(-57,3), S(-60,9), S(-57,9), S(-52,10), S(-50,14), S(-47,16), S(-48,18), S(-47,22), S(-42,22), S(-32,17)};
Score QUEEN_MOBILITIES[28] = {S(-10,0), S(-131,-58), S(-114,-192), S(-119,-90), S(-119,23), S(-119,40), S(-121,64), S(-118,76), S(-118,92), S(-115,92), S(-114,96), S(-112,99), S(-112,101), S(-110,100), S(-109,104), S(-106,95), S(-108,94), S(-106,91), S(-108,82), S(-99,67), S(-88,42), S(-76,23), S(-50,-25), S(-37,-49), S(2,-100), S(16,-124), S(-44,-84), S(-12,-130)};
Score DOUBLED_PAWN = S(-2,-15);
Score OPPOSED_ISOLATED_PAWN = S(-9,-8);
Score OPEN_ISOLATED_PAWN = S(-14,-7);
Score BACKWARDS_PAWN = S(-10,-8);
Score CONNECTED_PAWN[8] = {S(0,0), S(57,6), S(24,13), S(10,8), S(4,2), S(0,1), S(0,0), S(0,0)};
Score PASSED_PAWN[8] = {S(0,0), S(33,93), S(25,153), S(13,86), S(2,46), S(3,28), S(6,25), S(0,0)};
Score PASSED_PAWN_ADVANCE_DEFENDED = S(17,18);
Score PASSED_PAWN_EDGE_DISTANCE = S(-5,-8);
Score PASSED_PAWN_KING_PROXIMITY = S(-4,18);
Score ROOK_OPEN_FILE = S(19,10);
Score ROOK_SEMI_OPEN = S(2,6);
Score ROOK_SEVENTH_RANK = S(0,8);
Score ROOK_OPPOSITE_KING = S(22,-9);
Score ROOK_ADJACENT_KING = S(6,-19);
Score ROOK_TRAPPED = S(-42,-12);
Score KNIGHT_THREATS[6] = {S(0,18), S(-1,8), S(34,28), S(65,6), S(43,-40), S(132,18)};
Score BISHOP_THREATS[6] = {S(2,17), S(24,32), S(11,7), S(45,15), S(47,66), S(98,72)};
Score ROOK_THREATS[6] = {S(2,23), S(23,29), S(26,36), S(-1,27), S(69,17), S(122,26)};
Score KING_THREATS[6] = {S(13,52), S(22,41), S(5,30), S(32,17), S(-135,-134), S(0,0)};
Score TEMPO = S(21,0);

// clang-format on

Score PAWN_SHELTER[4][8] = {
    {-2, 12, 12, 15, 22, 38, 34, 0},
    {-18, -18, -5, -12, -21, 14, 28, 0},
    {-5, -15, 3, 13, -2, 10, 32, 0},
    {-16, -50, -24, -16, -21, -14, -5, 0},
};

Score PAWN_STORM[4][8] = {
    {-36, -20, -20, -20, -40, 70, 119, 0},
    {-18, -9, 4, -14, -20, -50, 10, 0},
    {3, 4, 5, 1, -14, -65, -21, 0},
    {6, 12, 6, -2, 0, -45, 5, 0},
};

Score BLOCKED_PAWN_STORM[8] = {
    S(0, 0), S(0, 0), S(2, -2), S(2, -2), S(2, -4), S(-31, -32), S(0, 0), S(0, 0),
};

Score KS_KING_FILE[4] = {S(9, -4), S(4, 0), S(0, 0), S(-4, 2)};

Score KS_ATTACKER_WEIGHTS[5] = {0, 28, 18, 15, 4};
Score KS_ATTACK = 24;
Score KS_WEAK_SQS = 63;
Score KS_SAFE_CHECK = 200;
Score KS_UNSAFE_CHECK = 51;
Score KS_ENEMY_QUEEN = 300;
Score KS_KNIGHT_DEFENSE = 20;

Score PSQT[12][64];
Score KNIGHT_POSTS[2][64];
Score BISHOP_POSTS[2][64];

void InitPSQT() {
  for (int sq = 0; sq < 64; sq++) {
    PSQT[PAWN_WHITE][sq] = PSQT[PAWN_BLACK][MIRROR[sq]] = PAWN_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[PAWN_TYPE];
    PSQT[KNIGHT_WHITE][sq] = PSQT[KNIGHT_BLACK][MIRROR[sq]] = KNIGHT_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[KNIGHT_TYPE];
    PSQT[BISHOP_WHITE][sq] = PSQT[BISHOP_BLACK][MIRROR[sq]] = BISHOP_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[BISHOP_TYPE];
    PSQT[ROOK_WHITE][sq] = PSQT[ROOK_BLACK][MIRROR[sq]] = ROOK_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[ROOK_TYPE];
    PSQT[QUEEN_WHITE][sq] = PSQT[QUEEN_BLACK][MIRROR[sq]] = QUEEN_PSQT[psqtIdx(sq)] + MATERIAL_VALUES[QUEEN_TYPE];
    PSQT[KING_WHITE][sq] = PSQT[KING_BLACK][MIRROR[sq]] = KING_PSQT[psqtIdx(sq)];

    KNIGHT_POSTS[WHITE][sq] = KNIGHT_POSTS[BLACK][MIRROR[sq]] = KNIGHT_POST_PSQT[psqtIdx(sq)];
    BISHOP_POSTS[WHITE][sq] = BISHOP_POSTS[BLACK][MIRROR[sq]] = BISHOP_POST_PSQT[psqtIdx(sq)];
  }
}

inline Score GetPhase(Board* board) {
  Score phase = 0;
  for (int i = KNIGHT_WHITE; i <= QUEEN_BLACK; i++)
    phase += PHASE_MULTIPLIERS[PIECE_TYPE[i]] * bits(board->pieces[i]);

  phase = min(MAX_PHASE, phase);
  return phase;
}

void InitEvalData(EvalData* data, Board* board) {
  BitBoard whitePawns = board->pieces[PAWN_WHITE];
  BitBoard blackPawns = board->pieces[PAWN_BLACK];
  BitBoard whitePawnAttacks = Shift(whitePawns, NE) | Shift(whitePawns, NW);
  BitBoard blackPawnAttacks = Shift(blackPawns, SE) | Shift(blackPawns, SW);

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN_TYPE] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN_TYPE] = blackPawnAttacks;
  data->twoAttacks[WHITE] = Shift(whitePawns, NE) & Shift(whitePawns, NW);
  data->twoAttacks[BLACK] = Shift(blackPawns, SE) & Shift(blackPawns, SW);

  data->outposts[WHITE] =
      ~Fill(blackPawnAttacks, S) & (whitePawnAttacks | Shift(whitePawns | blackPawns, S)) & (RANK_4 | RANK_5 | RANK_6);
  data->outposts[BLACK] =
      ~Fill(whitePawnAttacks, N) & (blackPawnAttacks | Shift(whitePawns | blackPawns, N)) & (RANK_5 | RANK_4 | RANK_3);

  BitBoard inTheWayWhitePawns = (Shift(board->occupancies[BOTH], S) | RANK_2 | RANK_3) & whitePawns;
  BitBoard inTheWayBlackPawns = (Shift(board->occupancies[BOTH], N) | RANK_7 | RANK_6) & blackPawns;

  data->mobilitySquares[WHITE] = ~(inTheWayWhitePawns | blackPawnAttacks);
  data->mobilitySquares[BLACK] = ~(inTheWayBlackPawns | whitePawnAttacks);

  data->kingSq[WHITE] = lsb(board->pieces[KING_WHITE]);
  data->kingSq[BLACK] = lsb(board->pieces[KING_BLACK]);

  data->kingArea[WHITE] = GetKingAttacks(data->kingSq[WHITE]);
  data->kingArea[BLACK] = GetKingAttacks(data->kingSq[BLACK]);
}

Score MaterialValue(Board* board, int side) {
  Score s = 0;

  for (int pc = PAWN[side]; pc <= KING[side]; pc += 2) {
    BitBoard pieces = board->pieces[pc];

    while (pieces) {
      s += PSQT[pc][lsb(pieces)];
      popLsb(pieces);
    }
  }

  if (bits(board->pieces[BISHOP[side]]) > 1)
    s += BISHOP_PAIR;

  return s;
}

Score PieceEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard enemyKingArea = data->kingArea[xside];
  BitBoard mob = data->mobilitySquares[side];
  BitBoard outposts = data->outposts[side];
  BitBoard myPawns = board->pieces[PAWN[side]];
  BitBoard opponentPawns = board->pieces[PAWN[xside]];

  for (int p = KNIGHT[side]; p <= KING[side]; p += 2) {
    BitBoard pieces = board->pieces[p];
    int pieceType = PIECE_TYPE[p];

    while (pieces) {
      BitBoard bb = pieces & -pieces;
      int sq = lsb(pieces);

      BitBoard movement = EMPTY;
      switch (pieceType) {
      case KNIGHT_TYPE:
        movement = GetKnightAttacks(sq);
        s += KNIGHT_MOBILITIES[bits(movement & mob)];
        break;
      case BISHOP_TYPE:
        movement = GetBishopAttacks(sq, board->occupancies[BOTH] ^ board->pieces[QUEEN[side]]);
        s += BISHOP_MOBILITIES[bits(movement & mob)];
        break;
      case ROOK_TYPE:
        movement =
            GetRookAttacks(sq, board->occupancies[BOTH] ^ board->pieces[ROOK[side]] ^ board->pieces[QUEEN[side]]);
        s += ROOK_MOBILITIES[bits(movement & mob)];
        break;
      case QUEEN_TYPE:
        movement = GetQueenAttacks(sq, board->occupancies[BOTH]);
        s += QUEEN_MOBILITIES[bits(movement & mob)];
        break;
      case KING_TYPE:
        movement = GetKingAttacks(sq) & ~enemyKingArea;
      }

      data->twoAttacks[side] |= (movement & data->allAttacks[side]);
      data->attacks[side][pieceType] |= movement;
      data->allAttacks[side] |= movement;

      if (movement & enemyKingArea) {
        data->ksAttackWeight[side] += KS_ATTACKER_WEIGHTS[pieceType];
        data->ksSqAttackCount[side] += bits(movement & enemyKingArea);
        data->ksAttackersCount[side]++;
      }

      if (pieceType == KNIGHT_TYPE) {
        if (outposts & bb) {
          s += KNIGHT_POSTS[side][sq];
        } else if (movement & outposts) {
          s += KNIGHT_OUTPOST_REACHABLE;
        }
      } else if (pieceType == BISHOP_TYPE) {
        if (outposts & bb) {
          s += BISHOP_POSTS[side][sq];
        } else if (movement & outposts) {
          s += BISHOP_OUTPOST_REACHABLE;
        }

        if (side == WHITE) {
          switch (sq) {
          case B8:
          case A7:
            if (subset(0x20400ULL, board->pieces[PAWN_BLACK]))
              s += BISHOP_TRAPPED;
            break;
          case G8:
          case H7:
            if (subset(0x402000ULL, board->pieces[PAWN_BLACK]))
              s += BISHOP_TRAPPED;
            break;
          }
        } else {
          switch (sq) {
          case B1:
          case A2:
            if (subset(0x4020000000000ULL, board->pieces[PAWN_WHITE]))
              s += BISHOP_TRAPPED;
            break;
          case G1:
          case H2:
            if (subset(0x20400000000000ULL, board->pieces[PAWN_WHITE]))
              s += BISHOP_TRAPPED;
            break;
          }
        }
      } else if (pieceType == ROOK_TYPE) {
        if (!(FILE_MASKS[file(sq)] & myPawns)) {
          if (!(FILE_MASKS[file(sq)] & opponentPawns)) {
            s += ROOK_OPEN_FILE;
          } else {
            s += ROOK_SEMI_OPEN;
          }

          // king threats bonus - these tuned really negative in the EG which is both surprising but not
          if (file(sq) == file(data->kingSq[xside])) {
            s += ROOK_OPPOSITE_KING;
          } else if (abs(file(sq) - file(data->kingSq[xside])) == 1) {
            s += ROOK_ADJACENT_KING;
          }
        }

        if (side == WHITE) {
          if (getBit(0x301000000000000ULL, sq) && getBit(0x600000000000000ULL, data->kingSq[WHITE])) {
            s += ROOK_TRAPPED;
          } else if (getBit(0xC080000000000000ULL, sq) && getBit(0x6000000000000000ULL, data->kingSq[WHITE])) {
            s += ROOK_TRAPPED;
          }
        } else {
          if (getBit(0x103ULL, sq) && getBit(0x6ULL, data->kingSq[BLACK])) {
            s += ROOK_TRAPPED;
          } else if (getBit(0x80C0ULL, sq) && getBit(0x60ULL, data->kingSq[BLACK])) {
            s += ROOK_TRAPPED;
          }
        }
      }

      popLsb(pieces);
    }
  }

  return s;
}

Score PawnEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard pawns = board->pieces[PAWN[side]];

  while (pawns) {
    BitBoard bb = pawns & -pawns;
    int sq = lsb(pawns);

    int file = file(sq);
    int rank = rank(sq);
    int adjustedRank = side ? 7 - rank : rank;
    int adjustedFile = file > 3 ? 7 - file : file; // 0-3

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & Shift(bb, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & GetPawnAttacks(sq, xside);
    BitBoard forwardLever = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLever;
    int passed = !(board->pieces[PAWN[xside]] & FORWARD_RANK_MASKS[side][rank] &
                   (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file])) &&
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled)
      s += DOUBLED_PAWN;

    if (!neighbors) {
      s += opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN;
    } else if (backwards) {
      s += BACKWARDS_PAWN;
    } else if (defenders | connected) {
      int scalar = 2 + !!connected - !!opposed;
      s += CONNECTED_PAWN[adjustedRank] * scalar;
    }

    if (passed) {
      s += PASSED_PAWN[adjustedRank] + PASSED_PAWN_EDGE_DISTANCE * adjustedFile;

      int advSq = sq + PAWN_DIRECTIONS[side];
      BitBoard advance = bit(advSq);

      if (adjustedRank <= 4) {
        int myDistance = distance(advSq, data->kingSq[side]);
        int opponentDistance = distance(advSq, data->kingSq[xside]);

        s += PASSED_PAWN_KING_PROXIMITY * min(4, max(opponentDistance - myDistance, -4));

        if (!(board->occupancies[xside] & advance)) {
          BitBoard pusher = GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                            FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

          if (pusher | (data->allAttacks[side] & advance)) {
            s += PASSED_PAWN_ADVANCE_DEFENDED;
          }
        }
      }
    }

    popLsb(pawns);
  }

  return s;
}

Score Threats(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard weak = board->occupancies[xside] &
                  ~(data->attacks[xside][PAWN_TYPE] | (data->twoAttacks[xside] & ~data->twoAttacks[side])) &
                  data->allAttacks[side];

  for (int piece = KNIGHT_TYPE; piece <= KING_TYPE; piece++) {
    if (piece == QUEEN_TYPE)
      continue;

    BitBoard threats = weak & data->attacks[side][piece];
    while (threats) {
      int sq = lsb(threats);
      int attacked = PIECE_TYPE[board->squares[sq]];

      switch (piece) {
      case KNIGHT_TYPE:
        s += KNIGHT_THREATS[attacked];
        break;
      case BISHOP_TYPE:
        s += BISHOP_THREATS[attacked];
        break;
      case ROOK_TYPE:
        s += ROOK_THREATS[attacked];
        break;
      case KING_TYPE:
        s += KING_THREATS[attacked];
        break;
      }

      popLsb(threats);
    }
  }

  return s;
}

Score KingSafety(Board* board, EvalData* data, int side) {
  Score s = 0;
  Score shelter = S(2, 2);

  int xside = side ^ 1;

  BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                      ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];

  for (int file = SHELTER_STORM_FILES[file(data->kingSq[side])][0];
       file <= SHELTER_STORM_FILES[file(data->kingSq[side])][1]; file++) {
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[file];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    shelter += S(PAWN_SHELTER[adjustedFile][pawnRank], 0);

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];
    } else {
      // storm score, no change in the EG
      shelter += S(PAWN_STORM[adjustedFile][theirRank], 0);
    }

    if (file == file(data->kingSq[side])) {
      int idx = 2 * !(board->pieces[PAWN[side]] & FILE_MASKS[file]) + !(board->pieces[PAWN[xside]] & FILE_MASKS[file]);
      shelter += KS_KING_FILE[idx];
    }
  }

  BitBoard kingArea = data->kingArea[side];
  BitBoard weak = data->allAttacks[xside] & ~data->twoAttacks[side] &
                  (~data->allAttacks[side] | data->attacks[side][QUEEN_TYPE] | data->attacks[side][KING_TYPE]);
  BitBoard vulnerable = (~data->allAttacks[side] | (weak & data->twoAttacks[xside])) & ~board->occupancies[xside];

  BitBoard possibleKnightChecks =
      GetKnightAttacks(data->kingSq[side]) & data->attacks[xside][KNIGHT_TYPE] & ~board->occupancies[xside];
  BitBoard possibleBishopChecks = GetBishopAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                  data->attacks[xside][BISHOP_TYPE] & ~board->occupancies[xside];
  BitBoard possibleRookChecks = GetRookAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                data->attacks[xside][ROOK_TYPE] & ~board->occupancies[xside];
  BitBoard possibleQueenChecks = GetQueenAttacks(data->kingSq[side], board->occupancies[BOTH]) &
                                 data->attacks[xside][QUEEN_TYPE] & ~board->occupancies[xside];

  int safeChecks = bits(possibleKnightChecks & vulnerable) + bits(possibleBishopChecks & vulnerable) +
                   bits(possibleRookChecks & vulnerable) + bits(possibleQueenChecks & vulnerable);
  int unsafeChecks = bits(possibleKnightChecks & ~vulnerable) + bits(possibleBishopChecks & ~vulnerable) +
                     bits(possibleRookChecks & ~vulnerable);

  Score danger = (data->ksAttackersCount[xside] * data->ksAttackWeight[xside])           // weight of attacks
                 + (KS_SAFE_CHECK * safeChecks)                                          // safe checks
                 + (KS_UNSAFE_CHECK * unsafeChecks)                                      // unsafe checks
                 + (KS_WEAK_SQS * bits(weak & kingArea))                                 // weak sqs
                 + (KS_ATTACK * data->ksSqAttackCount[xside])                            // aimed pieces
                 - (KS_ENEMY_QUEEN * !board->pieces[QUEEN[xside]])                       // enemy has queen?
                 - (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT_TYPE] & kingArea)) // knight on f8 = no m8
                 - scoreMG(shelter) / 2;                                                 // house

  // if my king is in danger apply score
  if (danger > 0) {
    s += S(-danger * danger / 1000, -danger / 30);
  }

  s += shelter;
  return s;
}

Score EvaluateKXK(Board* board) {
  if (board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK] | board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]) {
    Score eval = 10000;
    int ss = (board->pieces[QUEEN_WHITE] | board->pieces[ROOK_WHITE]) ? WHITE : BLACK;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    eval -= distance(ssKingSq, wsKingSq) * 25; // move king towards

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    return ss == board->side ? eval : -eval;
  } else if (board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]) {
    Score eval = 9000;
    int ss = board->pieces[PAWN_WHITE] ? WHITE : BLACK;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);
    int pawnSq = lsb(board->pieces[PAWN[ss]]);

    if (KPKDraw(ss, ssKingSq, wsKingSq, pawnSq, board->side)) {
      return 0;
    } else {
      int queenSq = ss == WHITE ? file(pawnSq) : file(pawnSq) + A1;

      eval -= 10 * distance(pawnSq, queenSq);
      eval -= distance(ssKingSq, queenSq);

      return ss == board->side ? eval : -eval;
    }
  }

  return UNKNOWN;
}

// The following KPK code is modified for my use from Cheng (as is the dataset)
uint8_t GetKPKBit(int bit) { return (uint8_t)(kpkResults[bit >> 3] & (1U << (bit & 7))); }

int KPKIndex(int ssKing, int wsKing, int p, int stm) {
  int file = file(p);
  int x = file > 3 ? 7 : 0;

  ssKing ^= x;
  wsKing ^= x;
  p ^= x;
  file ^= x;

  int pawn = (((p & 0x38) - 8) >> 1) | file;

  return ((unsigned)pawn << 13) | ((unsigned)stm << 12) | ((unsigned)wsKing << 6) | ((unsigned)ssKing);
}

int KPKDraw(int ss, int ssKing, int wsKing, int p, int stm) {
  int x = ss == WHITE ? 0 : 0x38;
  int idx = KPKIndex(ssKing ^ x, wsKing ^ x, p ^ x, stm ^ x);

  return GetKPKBit(idx);
}
//---

Score StaticMaterialScore(int side, Board* board) {
  Score s = 0;
  for (int p = PAWN[side]; p <= QUEEN[side]; p += 2)
    s += bits(board->pieces[p]) * STATIC_MATERIAL_VALUE[PIECE_TYPE[p]];

  return s;
}

Score EvaluateMaterialOnlyEndgame(Board* board) {
  Score whiteMaterial = StaticMaterialScore(WHITE, board);
  int whitePieceCount = bits(board->occupancies[WHITE]);

  Score blackMaterial = StaticMaterialScore(BLACK, board);
  int blackPieceCount = bits(board->occupancies[BLACK]);

  int ss = whiteMaterial >= blackMaterial ? WHITE : BLACK;

  if (!whiteMaterial || !blackMaterial) {
    // TODO: Clean this up - This is a repeat of KRK and KQK

    // We set eval to 10000 since we now know one side is completely winning
    // (they have to have mating material and the other side can't defend with no pieces)
    Score eval = 10000;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    eval -= distance(ssKingSq, wsKingSq) * 25;

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    return ss == board->side ? eval : -eval;
  } else {
    int ssMaterial = ss == WHITE ? whiteMaterial : blackMaterial;
    int wsMaterial = ss == WHITE ? blackMaterial : whiteMaterial;
    int ssPieceCount = ss == WHITE ? whitePieceCount : blackPieceCount;
    int wsPieceCount = ss == WHITE ? blackPieceCount : whitePieceCount;

    int materialDiff = ssMaterial - wsMaterial;
    Score eval = materialDiff;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

    // I keep doing this...
    eval -= distance(ssKingSq, wsKingSq) * 25;

    int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
    eval -= wsKingRank * 15;

    int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
    eval -= wsKingFile * 15;

    if (ssPieceCount <= wsPieceCount + 1 && materialDiff <= STATIC_MATERIAL_VALUE[BISHOP_TYPE]) {
      if (ssMaterial >= STATIC_MATERIAL_VALUE[ROOK_TYPE] + STATIC_MATERIAL_VALUE[BISHOP_TYPE])
        eval /= 4;
      else
        eval /= 8;
    }

    return ss == board->side ? eval : -eval;
  }
}

// Main evalution method
Score Evaluate(Board* board) {
  if (IsMaterialDraw(board))
    return 0;

  Score eval;
  if (bits(board->occupancies[BOTH]) == 3) {
    eval = EvaluateKXK(board);
    if (eval != UNKNOWN)
      return eval;
  } else if (!(board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK])) {
    eval = EvaluateMaterialOnlyEndgame(board);
    if (eval != UNKNOWN)
      return eval;
  }

  EvalData data = {0};
  InitEvalData(&data, board);

  Score s = 0;

  Score a = MaterialValue(board, WHITE) - MaterialValue(board, BLACK);
  Score b = PieceEval(board, &data, WHITE) - PieceEval(board, &data, BLACK);
  Score c = PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
  Score d = Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
  Score e = KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);

  s = a + b + c + d + e;

  s += board->side == WHITE ? TEMPO : -TEMPO;

  int phase = GetPhase(board);
  s = (phase * scoreMG(s) + (MAX_PHASE - phase) * scoreEG(s)) / MAX_PHASE;
  s = s * Scale(board, eval > 0 ? board->side : board->xside) / 100;
  return board->side == WHITE ? s : -s;
}

inline int Scale(Board* board, int ss) {
  if (bits(board->occupancies[ss]) == 2 && (board->pieces[KNIGHT[ss]] | board->pieces[BISHOP[ss]]))
    return 0;

  if (IsOCB(board))
    return 50;

  int ssPawns = bits(board->pieces[PAWN[ss]]);
  if (ssPawns < 4)
    return 100 - 3 * (4 - ssPawns) * (4 - ssPawns);

  return 100;
}