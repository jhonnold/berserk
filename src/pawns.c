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
extern int cs[2];

inline PawnHashEntry* TTPawnProbe(uint64_t hash, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  return entry->hash == hash ? entry : NULL;
}

inline void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns, ThreadData* thread) {
  PawnHashEntry* entry = &thread->pawnHashTable[(hash & PAWN_TABLE_MASK)];
  *entry = (PawnHashEntry){.hash = hash, .s = s, .passedPawns = passedPawns};
}

// Standard pawn and passer evaluation
// TODO: Pawn hash table
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
                 // make sure we don't double count passers
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
      s += CONNECTED_PAWN[adjustedRank] * scalar;

      if (T)
        C.connectedPawn[adjustedRank] += cs[side] * scalar;

      // candidate passers are either in tension right now (and a push is all they need)
      // or a pawn 2 ranks down is stopping them, but our pawns can support it through
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

  return s;
}

Score PasserEval(Board* board, EvalData* data, int side) {
  Score s = 0;
  int xside = side ^ 1;

  BitBoard passers = data->passedPawns & board->pieces[PAWN[side]];

  while (passers) {
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
    }

    popLsb(passers);
  }

  return s;
}
