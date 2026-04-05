// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#include "pawns.h"

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "types.h"
#include "util.h"

#ifdef TUNE
#define T 1
#else
#define T 0
#endif

extern EvalCoeffs C;
extern const int cs[2];

inline PawnHashEntry* TTPawnProbe(uint64_t hash, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  return entry->hash == hash ? entry : NULL;
}

inline void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  *entry = (PawnHashEntry){.hash = hash, .s = s, .passedPawns = passedPawns};
}

// Standard pawn and passer evaluation
Score PawnEval(Board* board, EvalData* data, int side) {
  Score s = 0;

  int xside = side ^ 1;
  BitBoard pawns = PieceBB(PAWN, side);

  while (pawns) {
    BitBoard bb = pawns & -pawns;
    int sq = LSB(pawns);

    int f = File(sq);
    int r = Rank(sq);
    int adjustedRank = side ? 7 - r : r;
    int adjustedFile = f > 3 ? 7 - f : f;

    BitBoard opposed = PieceBB(PAWN, xside) & FILE_MASKS[f] & FORWARD_RANK_MASKS[side][r];
    BitBoard doubled = PieceBB(PAWN, side) & (side == WHITE ? ShiftS(bb) : ShiftN(bb));
    BitBoard neighbors = PieceBB(PAWN, side) & ADJACENT_FILE_MASKS[f];
    BitBoard connected = neighbors & RANK_MASKS[r];
    BitBoard defenders = PieceBB(PAWN, side) & GetPawnAttacks(sq, xside);
    BitBoard levers = PieceBB(PAWN, xside) & GetPawnAttacks(sq, side);
    int advSq = sq + PawnDir(side);
    BitBoard forwardLevers = PieceBB(PAWN, xside) & GetPawnAttacks(advSq, side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][Rank(advSq)]) && forwardLevers;
    BitBoard passerSpan = FORWARD_RANK_MASKS[side][r] & (ADJACENT_FILE_MASKS[f] | FILE_MASKS[f]);
    BitBoard antiPassers = board->pieces[Piece(PAWN, xside)] & passerSpan;
    int passed = (!antiPassers || !(antiPassers ^ levers)) &&
                 // make sure we don't double count passers
                 !(PieceBB(PAWN, side) & FORWARD_RANK_MASKS[side][r] & FILE_MASKS[f]);

    s += DEFENDED_PAWN * BitCount(defenders);

    if (T)
      C.defendedPawns += cs[side] * BitCount(defenders);

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

      // candidate passers
      if (!passed) {
        int enoughSupport = !(antiPassers ^ forwardLevers) && BitCount(connected) >= BitCount(forwardLevers);

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

    pawns &= pawns - 1;
  }

  return s;
}

Score PasserEval(Board* board, EvalData* data, int side) {
  Score s = 0;
  int xside = side ^ 1;

  BitBoard passers = data->passedPawns & PieceBB(PAWN, side);

  while (passers) {
    BitBoard bb = passers & -passers;
    int sq = LSB(passers);
    int f = File(sq);
    int r = Rank(sq);
    int adjustedRank = side ? 7 - r : r;
    int adjustedFile = f > 3 ? 7 - f : f; // 0-3

    s += PASSED_PAWN[adjustedRank] + PASSED_PAWN_EDGE_DISTANCE * adjustedFile;

    if (T) {
      C.passedPawn[adjustedRank] += cs[side];
      C.passedPawnEdgeDistance += cs[side] * adjustedFile;
    }

    int advSq = sq + PawnDir(side);
    BitBoard advance = Bit(advSq);

    if (adjustedRank <= 4) {
      int myDistance = Distance(advSq, data->kingSq[side]);
      int opponentDistance = Distance(advSq, data->kingSq[xside]);

      s += PASSED_PAWN_KING_PROXIMITY * Min(4, Max(opponentDistance - myDistance, -4));

      if (T)
        C.passedPawnKingProximity += cs[side] * Min(4, Max(opponentDistance - myDistance, -4));

      if (!(bb & data->allAttacks[side])) {
        s += PASSED_PAWN_UNSUPPORTED;

        if (T)
          C.passedPawnUnsupported += cs[side];
      }

      BitBoard behind =
          GetRookAttacks(sq, OccBB(BOTH)) & FILE_MASKS[f] & FORWARD_RANK_MASKS[xside][r];
      BitBoard enemySliderBehind = behind & (PieceBB(ROOK, xside) | PieceBB(QUEEN, xside));

      if (enemySliderBehind) {
        s += PASSED_PAWN_ENEMY_SLIDER_BEHIND;

        if (T)
          C.passedPawnEnemySliderBehind += cs[side];
      }

      if (!(OccBB(xside) & advance)) {
        BitBoard pusher = behind & (PieceBB(ROOK, side) | PieceBB(QUEEN, side));
        BitBoard advTwoAtx = advance & (pusher ? data->allAttacks[side] : data->twoAttacks[side]);
        BitBoard advOneAtx = pusher ? advance : advance & data->allAttacks[side];
        BitBoard advPawnSupp = advance & data->attacks[side][PAWN];

        int safeAdvance =
            advPawnSupp || advTwoAtx || !(data->allAttacks[xside] & advance) || (advOneAtx & ~data->twoAttacks[xside]);

        if (safeAdvance) {
          s += PASSED_PAWN_ADVANCE_DEFENDED[adjustedRank];

          if (T)
            C.passedPawnAdvance[adjustedRank] += cs[side];
        }

        // outside passer vs knight
        if (bb & (A_FILE | H_FILE)) {
          if (!(PieceBB(BISHOP, xside) | PieceBB(ROOK, xside) | PieceBB(QUEEN, xside)) && PieceBB(KNIGHT, xside)) {
            s += PASSED_PAWN_OUTSIDE_V_KNIGHT;

            if (T)
              C.passedPawnOutsideVKnight += cs[side];
          }
        }

        // pawns only board
        if (board->piecesCounts < 0x100) {
          int promoSq = side == WHITE ? File(sq) : A1 + File(sq);
          if (Min(5, Distance(sq, promoSq)) < Distance(data->kingSq[xside], promoSq) - (board->stm == xside)) {
            s += PASSED_PAWN_SQ_RULE;

            if (T)
              C.passedPawnSqRule += cs[side];
          }
        }
      }
    }

    passers &= passers - 1;
  }

  return s;
}
