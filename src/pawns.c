#include "attacks.h"
#include "pawns.h"
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

const PawnHashEntry NO_PAWN_ENTRY = {.hash = 0};
PawnHashEntry PAWN_TT[(1ULL << 16)] = {0};

inline PawnHashEntry TTPawnProbe(uint64_t hash) {
  return PAWN_TT[(hash & 0xFFFF)].hash == hash ? PAWN_TT[(hash & 0xFFFF)] : NO_PAWN_ENTRY;
}

inline void TTPawnPut(uint64_t hash, Score s, BitBoard passedPawns) {
  PAWN_TT[(hash & 0xFFFF)].hash = hash;
  PAWN_TT[(hash & 0xFFFF)].s = s;
  PAWN_TT[(hash & 0xFFFF)].passedPawns = passedPawns;
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

    BitBoard opposed = board->pieces[PAWN[xside]] & FILE_MASKS[file] & FORWARD_RANK_MASKS[side][rank];
    BitBoard doubled = board->pieces[PAWN[side]] & Shift(bb, PAWN_DIRECTIONS[xside]);
    BitBoard neighbors = board->pieces[PAWN[side]] & ADJACENT_FILE_MASKS[file];
    BitBoard connected = neighbors & RANK_MASKS[rank];
    BitBoard defenders = board->pieces[PAWN[side]] & GetPawnAttacks(sq, xside);
    BitBoard levers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq, side);
    BitBoard forwardLevers = board->pieces[PAWN[xside]] & GetPawnAttacks(sq + PAWN_DIRECTIONS[side], side);
    int backwards = !(neighbors & FORWARD_RANK_MASKS[xside][rank(sq + PAWN_DIRECTIONS[side])]) && forwardLevers;
    BitBoard passerSpan = FORWARD_RANK_MASKS[side][rank] & (ADJACENT_FILE_MASKS[file] | FILE_MASKS[file]);
    BitBoard antiPassers = board->pieces[xside] & passerSpan;
    int passed = !antiPassers &&
                 // make sure we don't double count passers
                 !(board->pieces[PAWN[side]] & FORWARD_RANK_MASKS[side][rank] & FILE_MASKS[file]);

    if (doubled) {
      s += DOUBLED_PAWN;

      if (T)
        C.doubledPawns[side]++;
    }

    if (!neighbors) {
      s += opposed ? OPPOSED_ISOLATED_PAWN : OPEN_ISOLATED_PAWN;

      if (T) {
        if (opposed)
          C.opposedIsolatedPawns[side]++;
        else
          C.openIsolatedPawns[side]++;
      }
    } else if (backwards) {
      s += BACKWARDS_PAWN;

      if (T)
        C.backwardsPawns[side]++;
    } else if (defenders | connected) {
      int scalar = 2 + !!connected - !!opposed;
      s += CONNECTED_PAWN[adjustedRank] * scalar;

      if (T)
        C.connectedPawn[side][adjustedRank] += scalar;

      // candidate passers are either in tension right now (and a push is all they need)
      // or a pawn 2 ranks down is stopping them, but our pawns can support it through
      if (!passed) {
        int onlyInTension = !(antiPassers ^ levers);
        int enoughSupport = !(antiPassers ^ forwardLevers) && bits(connected) >= bits(forwardLevers);

        if (onlyInTension | enoughSupport) {
          s += CANDIDATE_PASSER[adjustedRank];

          if (T)
            C.candidatePasser[side][adjustedRank]++;
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
      C.passedPawn[side][adjustedRank]++;
      C.passedPawnEdgeDistance[side] += adjustedFile;
    }

    int advSq = sq + PAWN_DIRECTIONS[side];
    BitBoard advance = bit(advSq);

    if (adjustedRank <= 4) {
      int myDistance = distance(advSq, data->kingSq[side]);
      int opponentDistance = distance(advSq, data->kingSq[xside]);

      s += PASSED_PAWN_KING_PROXIMITY * min(4, max(opponentDistance - myDistance, -4));

      if (T)
        C.passedPawnKingProximity[side] += min(4, max(opponentDistance - myDistance, -4));

      if (!(board->occupancies[xside] & advance)) {
        BitBoard pusher = GetRookAttacks(sq, board->occupancies[BOTH]) & FILE_MASKS[file] &
                          FORWARD_RANK_MASKS[xside][rank] & (board->pieces[ROOK[side]] | board->pieces[QUEEN[side]]);

        if (pusher | (data->allAttacks[side] & advance)) {
          s += PASSED_PAWN_ADVANCE_DEFENDED;

          if (T)
            C.passedPawnAdvance[side]++;
        }
      }
    }

    popLsb(passers);
  }

  return s;
}
