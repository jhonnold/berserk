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

#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "pawns.h"
#include "types.h"
#include "util.h"

#ifdef TUNE
#define T 1
#else
#define T 0
#endif

extern EvalCoeffs C;
extern int cs[2];

KPNetwork* KP_NET;
const float KP_NET_DATA[N_KP_VALUES] = {
#include "kpnet.dat"
};

inline PawnHashEntry* TTPawnProbe(uint64_t hash, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  return entry->hash == hash ? entry : NULL;
}

inline void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  *entry = (PawnHashEntry){.hash = hash, .s = s, .passedPawns = passedPawns};
}

inline Score KPNetworkScore(Board* board) {
  float raw = KPNetworkPredict(board->pieces[PAWN_WHITE], board->pieces[PAWN_BLACK], lsb(board->pieces[KING_WHITE]),
                               lsb(board->pieces[KING_BLACK]), KP_NET);

  Score s = (Score)raw;
  return makeScore(s, s);
}

inline int GetKPNetworkIdx(int piece, int sq, int color) {
  if (piece == PAWN_TYPE) {
    return color == WHITE ? sq - 8 : sq + 40; // sq - 8 + 48
  } else {
    return color == WHITE ? sq + 96 : sq + 160; // 96 pawns sit in front
  }
}

float KPNetworkPredict(BitBoard whitePawns, BitBoard blackPawns, int wk, int bk, KPNetwork* network) {
#ifdef TUNE
  float* hidden = network->hiddenActivations;
#else
  float hidden[N_KP_HIDDEN];
#endif

  memcpy(hidden, network->biases0, sizeof(float) * N_KP_HIDDEN);

  while (whitePawns) {
    int idx = GetKPNetworkIdx(PAWN_TYPE, popAndGetLsb(&whitePawns), WHITE);

    for (int i = 0; i < N_KP_HIDDEN; i++)
      hidden[i] += network->weights0[i * N_KP_FEATURES + idx];
  }

  while (blackPawns) {
    int idx = GetKPNetworkIdx(PAWN_TYPE, popAndGetLsb(&blackPawns), BLACK);

    for (int i = 0; i < N_KP_HIDDEN; i++)
      hidden[i] += network->weights0[i * N_KP_FEATURES + idx];
  }

  int idx = GetKPNetworkIdx(KING_TYPE, wk, WHITE);
  for (int i = 0; i < N_KP_HIDDEN; i++)
    hidden[i] += network->weights0[i * N_KP_FEATURES + idx];

  idx = GetKPNetworkIdx(KING_TYPE, bk, BLACK);
  for (int i = 0; i < N_KP_HIDDEN; i++)
    hidden[i] += network->weights0[i * N_KP_FEATURES + idx];

  // Apply ReLu
  for (int i = 0; i < N_KP_HIDDEN; i++)
    hidden[i] = max(0.0, hidden[i]);

  float result = network->biases1[0];
  for (int i = 0; i < N_KP_HIDDEN; i++)
    result += network->weights1[i] * hidden[i];

  return result;
}

void SaveKPNetwork(char* path, KPNetwork* network) {
  FILE* fp = fopen(path, "a");
  if (fp == NULL) {
    printf("info string Unable to save network!\n");
    return;
  }

  for (int i = 0; i < N_KP_FEATURES * N_KP_HIDDEN; i++)
    fprintf(fp, "%f,", network->weights0[i]);

  for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++)
    fprintf(fp, "%f,", network->weights1[i]);

  for (int i = 0; i < N_KP_HIDDEN; i++)
    fprintf(fp, "%f,", network->biases0[i]);

  for (int i = 0; i < N_KP_OUTPUT; i++)
    fprintf(fp, "%f,", network->biases1[i]);

  fprintf(fp, "\n");
  fclose(fp);
}

void InitKPNetwork() {
  KP_NET = malloc(sizeof(KPNetwork));
  int n = 0;

  for (int i = 0; i < N_KP_FEATURES * N_KP_HIDDEN; i++)
    KP_NET->weights0[i] = KP_NET_DATA[n++];

  for (int i = 0; i < N_KP_HIDDEN * N_KP_OUTPUT; i++)
    KP_NET->weights1[i] = KP_NET_DATA[n++];

  for (int i = 0; i < N_KP_HIDDEN; i++)
    KP_NET->biases0[i] = KP_NET_DATA[n++];

  for (int i = 0; i < N_KP_OUTPUT; i++)
    KP_NET->biases1[i] = KP_NET_DATA[n++];
}

// Standard pawn evaluation
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
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & (side == WHITE ? ShiftS(bb) : ShiftN(bb));
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & GetPawnAttacks(sq, xside);
    BitBoard levers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq, side);
    BitBoard forwardLevers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLevers;
    BitBoard passerSpan = FORWARD_RANK_MASKS[side][rank] & (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file]);
    BitBoard antiPassers = board->pieces[xside] & passerSpan;
    int passed = (!antiPassers || !(antiPassers ^ levers)) &&
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    s += DEFENDED_PAWN * bits(defenders);

    if (T)
      C.defendedPawns += cs[side] * bits(defenders);

    if (doubled) {
      s += DOUBLED_PAWN;

      if (T)
        C.doubledPawns += cs[side];
    }

    if (!neighbors) {
      s += ISOLATED_PAWN[adjustedFile] + !opposed * OPEN_ISOLATED_PAWN;

      if (T) {
        C.isolatedPawns[adjustedFile] += cs[side];
        C.openIsolatedPawns += cs[side] * !opposed;
      }
    } else if (backwards) {
      s += BACKWARDS_PAWN;

      if (T)
        C.backwardsPawns += cs[side];
    } else if (defenders | connected) {
      int scalar = 2 + !!connected - !!opposed;
      s += CONNECTED_PAWN[adjustedFile][adjustedRank] * scalar;

      if (T)
        C.connectedPawn[adjustedFile][adjustedRank] += cs[side] * scalar;

      // candidate passers are stopped by a pawn 2 ranks down,
      // but our pawns can support it through
      if (!passed) {
        int enoughSupport = !(antiPassers ^ forwardLevers) && bits(connected) >= bits(forwardLevers);

        if (enoughSupport) {
          s += CANDIDATE_PASSER[adjustedRank] + adjustedFile * CANDIDATE_EDGE_DISTANCE;

          if (T) {
            C.candidatePasser[adjustedRank] += cs[side];
            C.candidateEdgeDistance += cs[side] * adjustedFile;
          }
        }
      }
    }

    if (passed)
      data->passedPawns |= bb;

    popLsb(pawns);
  }

  const BitBoard ourPawns = board->pieces[PAWN[side]] & ~data->attacks[xside][PAWN_TYPE] &
                            ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  const BitBoard opponentPawns = board->pieces[PAWN[xside]] & ~FORWARD_RANK_MASKS[xside][rank(data->kingSq[side])];
  const int kf = min(6, max(1, file(data->kingSq[side])));

  // pawn shelter includes, pawns in front of king/enemy pawn storm (blocked/moving)
  for (int file = kf - 1; file <= kf + 1; file++) {
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[file];
    int pawnRank = ourPawnFile ? (side ? 7 - rank(lsb(ourPawnFile)) : rank(msb(ourPawnFile))) : 0;
    s += PAWN_SHELTER[adjustedFile][pawnRank];
    if (T)
      C.pawnShelter[adjustedFile][pawnRank] += cs[side];

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - rank(lsb(opponentPawnFile)) : rank(msb(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      s += BLOCKED_PAWN_STORM[theirRank];

      if (T)
        C.blockedPawnStorm[theirRank] += cs[side];
    } else {
      s += PAWN_STORM[adjustedFile][theirRank];

      if (T)
        C.pawnStorm[adjustedFile][theirRank] += cs[side];
    }
  }

  return s;
}

Score PasserEval(Board* board, EvalData* data, int side) {
  Score s = 0;
  int xside = side ^ 1;

  BitBoard passers = data->passedPawns & board->pieces[PAWN[side]];

  while (passers) {
    BitBoard bb = passers & -passers;
    int sq = lsb(passers);
    int file = file(sq);
    int rank = rank(sq);
    int adjustedRank = side ? 7 - rank : rank;
    int adjustedFile = file > 3 ? 7 - file : file; // 0-3

    s += PASSED_PAWN[adjustedRank] + PASSED_PAWN_EDGE_DISTANCE * adjustedFile;

    if (T) {
      C.passedPawn[adjustedRank] += cs[side];
      C.passedPawnEdgeDistance += cs[side] * adjustedFile;
    }

    int advSq = sq + PAWN_DIRECTIONS[side];
    BitBoard advance = bit(advSq);

    if (adjustedRank <= 4) {
      int myDistance = distance(advSq, data->kingSq[side]);
      int opponentDistance = distance(advSq, data->kingSq[xside]);

      s += PASSED_PAWN_KING_PROXIMITY * min(4, max(opponentDistance - myDistance, -4));

      if (T)
        C.passedPawnKingProximity += cs[side] * min(4, max(opponentDistance - myDistance, -4));

      if (!(bb & data->allAttacks[side])) {
        s += PASSED_PAWN_UNSUPPORTED;

        if (T)
          C.passedPawnUnsupported += cs[side];
      }

      BitBoard behind =
          GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] & FORWARD_RANK_MASKS[xside][rank];
      BitBoard enemySliderBehind = behind & (board->pieces[ROOK[xside]] | board->pieces[QUEEN[xside]]);

      if (enemySliderBehind) {
        s += PASSED_PAWN_ENEMY_SLIDER_BEHIND;

        if (T)
          C.passedPawnEnemySliderBehind += cs[side];
      }

      if (!(board->occupancies[xside] & advance)) {
        BitBoard pusher = behind & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);
        BitBoard advTwoAtx = advance & (pusher ? data->allAttacks[side] : data->twoAttacks[side]);
        BitBoard advOneAtx = pusher ? advance : advance & data->allAttacks[side];
        BitBoard advPawnSupp = advance & data->attacks[side][PAWN_TYPE];

        int safeAdvance =
            advPawnSupp || advTwoAtx || !(data->allAttacks[xside] & advance) || (advOneAtx & ~data->twoAttacks[xside]);

        if (safeAdvance) {
          s += PASSED_PAWN_ADVANCE_DEFENDED[adjustedRank];

          if (T)
            C.passedPawnAdvance[adjustedRank] += cs[side];
        }

        // pawns only board
        if (board->piecesCounts < 0x100) {
          int promoSq = side == WHITE ? file(sq) : A1 + file(sq);
          if (min(5, distance(sq, promoSq)) < distance(data->kingSq[xside], promoSq) - (board->side == xside)) {
            s += PASSED_PAWN_SQ_RULE;

            if (T)
              C.passedPawnSqRule += cs[side];
          }
        }
      }

      if (bb & (A_FILE | H_FILE)) {
        if (!(board->pieces[BISHOP[xside]] | board->pieces[ROOK[xside]] | board->pieces[QUEEN[xside]]) &&
            board->pieces[KNIGHT[xside]]) {
          s += PASSED_PAWN_OUTSIDE_V_KNIGHT;

          if (T)
            C.passedPawnOutsideVKnight += cs[side];
        }
      }
    }

    popLsb(passers);
  }

  return s;
}
