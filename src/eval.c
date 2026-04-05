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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "pawns.h"
#include "search.h"
#include "tuner/tune.h"
#include "types.h"
#include "uci.h"
#include "util.h"

#ifdef TUNE
#define T 1
#else
#define T 0
#endif

// a utility for texel tuning
// berserk uses a coeff based tuner, ethereal's design
EvalCoeffs C;
const int cs[2] = {1, -1};

const int PHASE_VALUES[6] = {0, 3, 3, 5, 10, 0};
const int MAX_PHASE       = 24;
const int PHASE_MULTIPLIERS[5] = {0, 1, 1, 2, 4};
const int MAX_SCALE = 128;

const int STATIC_MATERIAL_VALUE[7] = {100, 565, 565, 705, 1000, 30000, 0};
const int SHELTER_STORM_FILES[8][2] = {{0, 2}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {5, 7}};

// Get the phase of the game
inline int GetPhase(Board* board) {
  int phase = 0;
  for (int i = WHITE_KNIGHT; i <= BLACK_QUEEN; i++)
    phase += PHASE_MULTIPLIERS[PieceType(i)] * BitCount(board->pieces[i]);

  phase = Min(MAX_PHASE, phase);
  return (phase * 128 + MAX_PHASE / 2) / MAX_PHASE;
}

inline int IsOCB(Board* board) {
  BitBoard nonBishopMaterial = PPieceBB(WHITE_QUEEN) | PPieceBB(BLACK_QUEEN) | PPieceBB(WHITE_ROOK) |
                               PPieceBB(BLACK_ROOK) | PPieceBB(WHITE_KNIGHT) | PPieceBB(BLACK_KNIGHT);

  return !nonBishopMaterial && BitCount(PPieceBB(WHITE_BISHOP)) == 1 && BitCount(PPieceBB(BLACK_BISHOP)) == 1 &&
         BitCount((PPieceBB(WHITE_BISHOP) | PPieceBB(BLACK_BISHOP)) & DARK_SQS) == 1;
}

// Get a scalar for the given board for the stronger side (out of 100)
inline int Scale(Board* board, int ss) {
  if (BitCount(OccBB(ss)) == 2 && (PieceBB(KNIGHT, ss) | PieceBB(BISHOP, ss)))
    return 0;

  if (IsOCB(board))
    return 64;

  int ssPawns = BitCount(PieceBB(PAWN, ss));
  return MAX_SCALE - (8 - ssPawns) * (8 - ssPawns);
}

// preload a bunch of important evaluation data
void InitEvalData(EvalData* data, Board* board) {
  data->passedPawns = 0ULL;

  BitBoard whitePawns = PPieceBB(WHITE_PAWN);
  BitBoard blackPawns = PPieceBB(BLACK_PAWN);
  BitBoard whitePawnAttacks = ShiftNE(whitePawns) | ShiftNW(whitePawns);
  BitBoard blackPawnAttacks = ShiftSE(blackPawns) | ShiftSW(blackPawns);

  data->openFiles = ~(Fill(whitePawns | blackPawns, N) | Fill(whitePawns | blackPawns, S));

  data->allAttacks[WHITE] = data->attacks[WHITE][PAWN] = whitePawnAttacks;
  data->allAttacks[BLACK] = data->attacks[BLACK][PAWN] = blackPawnAttacks;
  data->attacks[WHITE][KNIGHT] = data->attacks[BLACK][KNIGHT] = 0ULL;
  data->attacks[WHITE][BISHOP] = data->attacks[BLACK][BISHOP] = 0ULL;
  data->attacks[WHITE][ROOK] = data->attacks[BLACK][ROOK] = 0ULL;
  data->attacks[WHITE][QUEEN] = data->attacks[BLACK][QUEEN] = 0ULL;
  data->attacks[WHITE][KING] = data->attacks[BLACK][KING] = 0ULL;
  data->twoAttacks[WHITE] = ShiftNE(whitePawns) & ShiftNW(whitePawns);
  data->twoAttacks[BLACK] = ShiftSE(blackPawns) & ShiftSW(blackPawns);

  data->outposts[WHITE] =
      ~Fill(blackPawnAttacks, S) & (whitePawnAttacks | ShiftS(whitePawns | blackPawns)) & (RANK_4 | RANK_5 | RANK_6);
  data->outposts[BLACK] =
      ~Fill(whitePawnAttacks, N) & (blackPawnAttacks | ShiftN(whitePawns | blackPawns)) & (RANK_5 | RANK_4 | RANK_3);

  BitBoard inTheWayWhitePawns = (ShiftS(OccBB(BOTH)) | RANK_2 | RANK_3) & whitePawns;
  BitBoard inTheWayBlackPawns = (ShiftN(OccBB(BOTH)) | RANK_7 | RANK_6) & blackPawns;

  data->mobilitySquares[WHITE] = ~(inTheWayWhitePawns | blackPawnAttacks);
  data->mobilitySquares[BLACK] = ~(inTheWayBlackPawns | whitePawnAttacks);

  data->kingSq[WHITE] = LSB(PPieceBB(WHITE_KING));
  data->kingSq[BLACK] = LSB(PPieceBB(BLACK_KING));
  data->ksAttackWeight[WHITE] = data->ksAttackWeight[BLACK] = 0;
  data->ksAttackerCount[WHITE] = data->ksAttackerCount[BLACK] = 0;

  int whiteKingF = Max(1, Min(6, File(data->kingSq[WHITE])));
  int whiteKingR = Max(1, Min(6, Rank(data->kingSq[WHITE])));
  int blackKingF = Max(1, Min(6, File(data->kingSq[BLACK])));
  int blackKingR = Max(1, Min(6, Rank(data->kingSq[BLACK])));

  data->kingArea[WHITE] = GetKingAttacks(Sq(whiteKingR, whiteKingF)) | Bit(Sq(whiteKingR, whiteKingF));
  data->kingArea[BLACK] = GetKingAttacks(Sq(blackKingR, blackKingF)) | Bit(Sq(blackKingR, blackKingF));
}

Score NonPawnMaterialValue(Board* board) {
  Score s = 0;

  for (int pc = WHITE_KNIGHT; pc <= BLACK_QUEEN; pc++)
    s += BitCount(board->pieces[pc]) * scoreMG(MATERIAL_VALUES[PieceType(pc)]);

  return s;
}

// Material + PSQT value
inline Score MaterialValue(Board* board, const int side) {
  Score s = S(0, 0);

  const int xside = side ^ 1;

  int eks = File(LSB(PieceBB(KING, xside))) > 3;

  for (int pc = Piece(PAWN, side); pc <= Piece(KING, side); pc += 2) {
    BitBoard pieces = board->pieces[pc];

    if (T)
      C.pieces[PieceType(pc)] += cs[side] * BitCount(pieces);

    while (pieces) {
      int sq = LSB(pieces);

      s += PSQT[pc][eks == (File(sq) > 3)][sq];

      if (T)
        C.psqt[PieceType(pc)][eks == (File(sq) > 3)][psqtIdx(side == WHITE ? sq : MIRROR[sq])] += cs[side];

      pieces &= pieces - 1;
    }
  }

  return s;
}

INLINE Score PieceEval(Board* board, EvalData* data, const int piece) {
  Score s = S(0, 0);

  const int side = piece & 1;
  const int xside = side ^ 1;
  const int pieceType = PieceType(piece);
  const BitBoard enemyKingArea = data->kingArea[xside];
  const BitBoard mob = data->mobilitySquares[side];
  const BitBoard outposts = data->outposts[side];
  const BitBoard myPawns = PieceBB(PAWN, side);
  const BitBoard opponentPawns = PieceBB(PAWN, xside);
  const BitBoard allPawns = myPawns | opponentPawns;

  if (pieceType == BISHOP) {
    // bishop pair bonus first
    if (BitCount(PieceBB(BISHOP, side)) > 1) {
      s += BISHOP_PAIR;

      if (T)
        C.bishopPair += cs[side];
    }

    BitBoard minorsBehindPawns = (PieceBB(KNIGHT, side) | PieceBB(BISHOP, side)) &
                                 (side == WHITE ? ShiftS(allPawns) : ShiftN(allPawns));
    s += BitCount(minorsBehindPawns) * MINOR_BEHIND_PAWN;

    if (T)
      C.minorBehindPawn += BitCount(minorsBehindPawns) * cs[side];
  }

  BitBoard pieces = board->pieces[piece];
  while (pieces) {
    BitBoard bb = pieces & -pieces;
    int sq = LSB(pieces);

    BitBoard movement = 0ULL;
    if (pieceType == KNIGHT) {
      movement = GetKnightAttacks(sq);
      s += KNIGHT_MOBILITIES[BitCount(movement & mob)];

      if (T)
        C.knightMobilities[BitCount(movement & mob)] += cs[side];
    } else if (pieceType == BISHOP) {
      movement =
          GetBishopAttacks(sq, OccBB(BOTH) ^ PieceBB(QUEEN, side) ^ PieceBB(QUEEN, xside));
      s += BISHOP_MOBILITIES[BitCount(movement & mob)];

      if (T)
        C.bishopMobilities[BitCount(movement & mob)] += cs[side];
    } else if (pieceType == ROOK) {
      movement = GetRookAttacks(sq, OccBB(BOTH) ^ PieceBB(ROOK, side) ^ PieceBB(QUEEN, side) ^
                                        PieceBB(QUEEN, xside));
      s += ROOK_MOBILITIES[BitCount(movement & mob)];

      if (T)
        C.rookMobilities[BitCount(movement & mob)] += cs[side];
    } else if (pieceType == QUEEN) {
      movement = GetQueenAttacks(sq, OccBB(BOTH));
      s += QUEEN_MOBILITIES[BitCount(movement & mob)];

      if (T)
        C.queenMobilities[BitCount(movement & mob)] += cs[side];
    } else if (pieceType == KING) {
      movement = GetKingAttacks(sq) & ~enemyKingArea;
      int numMoves = BitCount(movement & ~data->allAttacks[xside] & ~OccBB(side));

      s += KING_MOBILITIES[numMoves];

      if (T)
        C.kingMobilities[numMoves] += cs[side];
    }

    // Update attack/king safety data
    data->twoAttacks[side] |= (movement & data->allAttacks[side]);
    data->attacks[side][pieceType] |= movement;
    data->allAttacks[side] |= movement;

    if (movement & enemyKingArea) {
      data->ksAttackWeight[side] += KS_ATTACKER_WEIGHTS[pieceType];
      data->ksAttackerCount[side]++;

      if (T) {
        C.ksAttackerCount[xside]++;
        C.ksAttackerWeights[xside][pieceType]++;
      }
    }

    // Piece specific bonus'
    if (pieceType == KNIGHT) {
      if (outposts & bb) {
        s += KNIGHT_POSTS[side][sq];

        if (T)
          C.knightPostPsqt[psqtIdx(side == WHITE ? sq : MIRROR[sq]) - 8] += cs[side];
      } else if (movement & outposts) {
        s += KNIGHT_OUTPOST_REACHABLE;

        if (T)
          C.knightPostReachable += cs[side];
      }
    } else if (pieceType == BISHOP) {
      BitBoard bishopSquares = (bb & DARK_SQS) ? DARK_SQS : ~DARK_SQS;
      BitBoard inTheWayPawns = PieceBB(PAWN, side) & bishopSquares;
      BitBoard blockedInTheWayPawns =
          (side == WHITE ? ShiftS(OccBB(BOTH)) : ShiftN(OccBB(BOTH))) &
          PieceBB(PAWN, side) & ~(A_FILE | B_FILE | G_FILE | H_FILE);

      int scalar = BitCount(inTheWayPawns) * BitCount(blockedInTheWayPawns);
      s += BAD_BISHOP_PAWNS * scalar;

      if (T)
        C.badBishopPawns += cs[side] * scalar;

      if (!(CENTER_SQS & bb) && BitCount(CENTER_SQS & GetBishopAttacks(sq, (myPawns | opponentPawns))) > 1) {
        s += DRAGON_BISHOP;

        if (T)
          C.dragonBishop += cs[side];
      }

      if (outposts & bb) {
        s += BISHOP_POSTS[side][sq];

        if (T)
          C.bishopPostPsqt[psqtIdx(side == WHITE ? sq : MIRROR[sq]) - 8] += cs[side];
      } else if (movement & outposts) {
        s += BISHOP_OUTPOST_REACHABLE;

        if (T)
          C.bishopPostReachable += cs[side];
      }

      if (side == WHITE) {
        if ((sq == A7 || sq == B8) && GetBit(opponentPawns, B6) && GetBit(opponentPawns, C7)) {
          s += BISHOP_TRAPPED;

          if (T)
            C.bishopTrapped += cs[side];
        } else if ((sq == H7 || sq == G8) && GetBit(opponentPawns, F7) && GetBit(opponentPawns, G6)) {
          s += BISHOP_TRAPPED;

          if (T)
            C.bishopTrapped += cs[side];
        }
      } else {
        if ((sq == A2 || sq == B1) && GetBit(opponentPawns, B3) && GetBit(opponentPawns, C2)) {
          s += BISHOP_TRAPPED;

          if (T)
            C.bishopTrapped += cs[side];
        } else if ((sq == H2 || sq == G1) && GetBit(opponentPawns, G3) && GetBit(opponentPawns, F2)) {
          s += BISHOP_TRAPPED;

          if (T)
            C.bishopTrapped += cs[side];
        }
      }
    } else if (pieceType == ROOK) {
      int numOpenFiles = 8 - BitCount(Fill(PPieceBB(WHITE_PAWN) | PPieceBB(BLACK_PAWN), N) & 0xFFULL);
      s += ROOK_OPEN_FILE_OFFSET * numOpenFiles;

      if (T)
        C.rookOpenFileOffset += cs[side] * numOpenFiles;

      if (!(FILE_MASKS[File(sq)] & myPawns)) {
        if (!(FILE_MASKS[File(sq)] & opponentPawns)) {
          s += ROOK_OPEN_FILE;

          if (T)
            C.rookOpenFile += cs[side];
        } else {
          if (!(movement & FORWARD_RANK_MASKS[side][Rank(sq)] & opponentPawns & data->attacks[xside][PAWN])) {
            s += ROOK_SEMI_OPEN;

            if (T)
              C.rookSemiOpen += cs[side];
          }
        }
      }

      if (movement & data->openFiles) {
        s += ROOK_TO_OPEN;

        if (T)
          C.rookToOpen += cs[side];
      }

      if (side == WHITE) {
        if ((sq == A1 || sq == A2 || sq == B1) && (data->kingSq[side] == C1 || data->kingSq[side] == B1)) {
          s += ROOK_TRAPPED;

          if (T)
            C.rookTrapped += cs[side];
        } else if ((sq == H1 || sq == H2 || sq == G1) && (data->kingSq[side] == F1 || data->kingSq[side] == G1)) {
          s += ROOK_TRAPPED;

          if (T)
            C.rookTrapped += cs[side];
        }
      } else {
        if ((sq == A8 || sq == A7 || sq == B8) && (data->kingSq[side] == B8 || data->kingSq[side] == C8)) {
          s += ROOK_TRAPPED;

          if (T)
            C.rookTrapped += cs[side];
        } else if ((sq == H8 || sq == H7 || sq == G8) && (data->kingSq[side] == F8 || data->kingSq[side] == G8)) {
          s += ROOK_TRAPPED;

          if (T)
            C.rookTrapped += cs[side];
        }
      }
    } else if (pieceType == QUEEN) {
      if (FILE_MASKS[File(sq)] & PieceBB(ROOK, xside)) {
        s += QUEEN_OPPOSITE_ROOK;

        if (T)
          C.queenOppositeRook += cs[side];
      }

      if ((FILE_MASKS[File(sq)] & FORWARD_RANK_MASKS[side][Rank(sq)] & PieceBB(ROOK, side)) &&
          !(FILE_MASKS[File(sq)] & myPawns)) {
        s += QUEEN_ROOK_BATTERY;

        if (T)
          C.queenRookBattery += cs[side];
      }
    }

    pieces &= pieces - 1;
  }

  return s;
}

// Threats bonus (piece attacks piece)
INLINE Score Threats(Board* board, EvalData* data, const int side) {
  Score s = S(0, 0);

  const int xside = side ^ 1;

  const BitBoard covered = data->attacks[xside][PAWN] | (data->twoAttacks[xside] & ~data->twoAttacks[side]);
  const BitBoard nonPawnEnemies = OccBB(xside) & ~PieceBB(PAWN, xside);
  const BitBoard weak = ~covered & data->allAttacks[side];

  for (int piece = KNIGHT; piece <= ROOK; piece++) {
    BitBoard threats = OccBB(xside) & data->attacks[side][piece];
    if (piece == KNIGHT || piece == BISHOP)
      threats &= nonPawnEnemies | weak;
    else
      threats &= weak;

    while (threats) {
      int sq = LSB(threats);
      int attacked = PieceType(board->squares[sq]);

      switch (piece) {
      case KNIGHT:
        s += KNIGHT_THREATS[attacked];

        if (T)
          C.knightThreats[attacked] += cs[side];
        break;
      case BISHOP:
        s += BISHOP_THREATS[attacked];

        if (T)
          C.bishopThreats[attacked] += cs[side];
        break;
      case ROOK:
        s += ROOK_THREATS[attacked];

        if (T)
          C.rookThreats[attacked] += cs[side];
        break;
      }

      threats &= threats - 1;
    }
  }

  BitBoard kingThreats = weak & data->attacks[side][KING] & OccBB(xside);
  if (kingThreats) {
    s += KING_THREAT;

    if (T)
      C.kingThreat += cs[side];
  }

  BitBoard pawnThreats = nonPawnEnemies & data->attacks[side][PAWN];
  s += BitCount(pawnThreats) * PAWN_THREAT;

  BitBoard hangingPieces = OccBB(xside) & ~data->allAttacks[xside] & data->allAttacks[side];
  s += BitCount(hangingPieces) * HANGING_THREAT;

  BitBoard pawnPushes = ~OccBB(BOTH) &
                        (side == WHITE ? ShiftN(PPieceBB(WHITE_PAWN)) : ShiftS(PPieceBB(BLACK_PAWN)));
  pawnPushes |= ~OccBB(BOTH) & (side == WHITE ? ShiftN(pawnPushes & RANK_3) : ShiftS(pawnPushes & RANK_6));
  pawnPushes &= (data->allAttacks[side] | ~data->allAttacks[xside]);
  BitBoard pawnPushAttacks =
      (side == WHITE ? ShiftNE(pawnPushes) | ShiftNW(pawnPushes) : ShiftSE(pawnPushes) | ShiftSW(pawnPushes));
  pawnPushAttacks &= nonPawnEnemies;

  s += BitCount(pawnPushAttacks) * PAWN_PUSH_THREAT + BitCount(pawnPushAttacks & board->pinned) * PAWN_PUSH_THREAT_PINNED;

  if (T) {
    C.pawnThreat += cs[side] * BitCount(pawnThreats);
    C.hangingThreat += cs[side] * BitCount(hangingPieces);
    C.pawnPushThreat += cs[side] * BitCount(pawnPushAttacks);
    C.pawnPushThreatPinned += cs[side] * BitCount(pawnPushAttacks & board->pinned);
  }

  if (PieceBB(QUEEN, xside)) {
    int oppQueenSquare = LSB(PieceBB(QUEEN, xside));

    BitBoard knightQueenHits =
        GetKnightAttacks(oppQueenSquare) & data->attacks[side][KNIGHT] & ~PieceBB(PAWN, side) & ~covered;
    s += BitCount(knightQueenHits) * KNIGHT_CHECK_QUEEN;

    BitBoard bishopQueenHits = GetBishopAttacks(oppQueenSquare, OccBB(BOTH)) &
                               data->attacks[side][BISHOP] & ~PieceBB(PAWN, side) & ~covered &
                               data->twoAttacks[side];
    s += BitCount(bishopQueenHits) * BISHOP_CHECK_QUEEN;

    BitBoard rookQueenHits = GetRookAttacks(oppQueenSquare, OccBB(BOTH)) & data->attacks[side][ROOK] &
                             ~PieceBB(PAWN, side) & ~covered & data->twoAttacks[side];
    s += BitCount(rookQueenHits) * ROOK_CHECK_QUEEN;

    if (T) {
      C.knightCheckQueen += cs[side] * BitCount(knightQueenHits);
      C.bishopCheckQueen += cs[side] * BitCount(bishopQueenHits);
      C.rookCheckQueen += cs[side] * BitCount(rookQueenHits);
    }
  }

  return s;
}

INLINE Score KingSafety(Board* board, EvalData* data, const int side) {
  Score s = S(0, 0);
  Score shelter = S(0, 0);

  const int xside = side ^ 1;

  const BitBoard ourPawns = PieceBB(PAWN, side) & ~data->attacks[xside][PAWN] &
                            ~FORWARD_RANK_MASKS[xside][Rank(data->kingSq[side])];
  const BitBoard opponentPawns = PieceBB(PAWN, xside) & ~FORWARD_RANK_MASKS[xside][Rank(data->kingSq[side])];

  // pawn shelter includes, pawns in front of king/enemy pawn storm (blocked/moving)
  for (int file = SHELTER_STORM_FILES[File(data->kingSq[side])][0];
       file <= SHELTER_STORM_FILES[File(data->kingSq[side])][1]; file++) {
    int adjustedFile = file > 3 ? 7 - file : file;

    BitBoard ourPawnFile = ourPawns & FILE_MASKS[file];
    int pawnRank = ourPawnFile ? (side ? 7 - Rank(LSB(ourPawnFile)) : Rank(MSB(ourPawnFile))) : 0;
    shelter += PAWN_SHELTER[adjustedFile][pawnRank];
    if (T)
      C.pawnShelter[adjustedFile][pawnRank] += cs[side];

    BitBoard opponentPawnFile = opponentPawns & FILE_MASKS[file];
    int theirRank = opponentPawnFile ? (side ? 7 - Rank(LSB(opponentPawnFile)) : Rank(MSB(opponentPawnFile))) : 0;
    if (pawnRank && pawnRank == theirRank + 1) {
      shelter += BLOCKED_PAWN_STORM[theirRank];

      if (T)
        C.blockedPawnStorm[theirRank] += cs[side];
    } else {
      shelter += PAWN_STORM[adjustedFile][theirRank];

      if (T)
        C.pawnStorm[adjustedFile][theirRank] += cs[side];
    }
  }

  uint8_t rights = side == WHITE ? (board->castling & 0xC) : (board->castling & 0x3);

  shelter += CAN_CASTLE * BitCount((uint64_t)rights);
  if (T)
    C.castlingRights += cs[side] * BitCount((uint64_t)rights);

  BitBoard kingArea = data->kingArea[side];
  BitBoard weak = data->allAttacks[xside] & ~data->twoAttacks[side] &
                  (~data->allAttacks[side] | data->attacks[side][QUEEN] | data->attacks[side][KING]);
  BitBoard vulnerable = (~data->allAttacks[side] | (weak & data->twoAttacks[xside])) & ~OccBB(xside);

  BitBoard possibleKnightChecks =
      GetKnightAttacks(data->kingSq[side]) & data->attacks[xside][KNIGHT] & ~OccBB(xside);
  BitBoard possibleBishopChecks = GetBishopAttacks(data->kingSq[side], OccBB(BOTH)) &
                                  data->attacks[xside][BISHOP] & ~OccBB(xside);
  BitBoard possibleRookChecks = GetRookAttacks(data->kingSq[side], OccBB(BOTH)) &
                                data->attacks[xside][ROOK] & ~OccBB(xside);
  BitBoard possibleQueenChecks = GetQueenAttacks(data->kingSq[side], OccBB(BOTH)) &
                                 data->attacks[xside][QUEEN] & ~OccBB(xside);

  int unsafeChecks = BitCount((possibleKnightChecks | possibleBishopChecks | possibleRookChecks) & ~vulnerable);

  Score danger = data->ksAttackWeight[xside] * data->ksAttackerCount[xside]
                 + (KS_KNIGHT_CHECK * BitCount(possibleKnightChecks & vulnerable))
                 + (KS_BISHOP_CHECK * BitCount(possibleBishopChecks & vulnerable))
                 + (KS_ROOK_CHECK * BitCount(possibleRookChecks & vulnerable))
                 + (KS_QUEEN_CHECK * BitCount(possibleQueenChecks & vulnerable))
                 + (KS_UNSAFE_CHECK * unsafeChecks)
                 + (KS_WEAK_SQS * BitCount(weak & kingArea))
                 + (KS_PINNED * BitCount(board->pinned & OccBB(side)))
                 + (KS_ENEMY_QUEEN * !PieceBB(QUEEN, xside))
                 + (KS_KNIGHT_DEFENSE * !!(data->attacks[side][KNIGHT] & kingArea));

  // only include this if in danger
  if (danger > 0)
    s += S(-danger * danger / 1024, -danger / 32);

  if (T) {
    C.ks += s * cs[side];
    C.danger[side] = danger;

    C.ksKnightCheck[side] = BitCount(possibleKnightChecks & vulnerable);
    C.ksBishopCheck[side] = BitCount(possibleBishopChecks & vulnerable);
    C.ksRookCheck[side] = BitCount(possibleRookChecks & vulnerable);
    C.ksQueenCheck[side] = BitCount(possibleQueenChecks & vulnerable);
    C.ksUnsafeCheck[side] = unsafeChecks;
    C.ksWeakSqs[side] = BitCount(weak & kingArea);
    C.ksPinned[side] = BitCount(board->pinned & OccBB(side));
    C.ksEnemyQueen[side] = !PieceBB(QUEEN, xside);
    C.ksKnightDefense[side] = !!(data->attacks[side][KNIGHT] & kingArea);
  }

  s += shelter;
  return s;
}

// Space evaluation
INLINE Score SpaceEval(Board* board, EvalData* data, const int side) {
  static const BitBoard CENTER_FILES = (C_FILE | D_FILE | E_FILE | F_FILE) & ~(RANK_1 | RANK_8);

  Score s = S(0, 0);
  const int xside = side ^ 1;

  BitBoard space = side == WHITE ? ShiftS(PieceBB(PAWN, side)) : ShiftN(PieceBB(PAWN, side));
  space |= side == WHITE ? (ShiftS(space) | ShiftSS(space)) : (ShiftN(space) | ShiftNN(space));
  space &= ~(PieceBB(PAWN, side) | data->attacks[xside][PAWN] |
             (data->twoAttacks[xside] & ~data->twoAttacks[side]));
  space &= CENTER_FILES;

  int pieces = BitCount(OccBB(side));
  int openFiles = 8 - BitCount(Fill(PPieceBB(WHITE_PAWN) | PPieceBB(BLACK_PAWN), N) & 0xFULL);
  int scalar = BitCount(space) * Max(0, pieces - openFiles) * Max(0, pieces - openFiles);

  s += S((SPACE * scalar) / 1024, 0);

  if (T)
    C.space += cs[side] * scalar;

  return s;
}

INLINE Score Imbalance(Board* board, const int side) {
  Score s = S(0, 0);
  const int xside = side ^ 1;

  for (int i = KNIGHT; i < KING; i++) {
    for (int j = PAWN; j < i; j++) {
      s += IMBALANCE[i][j] * BitCount(board->pieces[2 * i + side]) * BitCount(board->pieces[2 * j + xside]);

      if (T)
        C.imbalance[i][j] += BitCount(board->pieces[2 * i + side]) * BitCount(board->pieces[2 * j + xside]) * cs[side];
    }
  }

  return s;
}

INLINE Score Complexity(Board* board, Score eg) {
  int sign = (eg > 0) - (eg < 0);
  if (!sign)
    return S(0, 0);

  BitBoard allPawns = PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK);
  int pawns = BitCount(allPawns);
  int pawnsBothSides = !!(allPawns & (A_FILE | B_FILE | C_FILE | D_FILE)) &&
                       !!(allPawns & (E_FILE | F_FILE | G_FILE | H_FILE));

  Score complexity = pawns * COMPLEXITY_PAWNS
                     + pawnsBothSides * COMPLEXITY_PAWNS_BOTH_SIDES
                     + COMPLEXITY_OFFSET;

  if (T) {
    C.complexPawns = pawns;
    C.complexPawnsBothSides = pawnsBothSides;
    C.complexOffset = 1;
  }

  int bound = sign * Max(complexity, -abs(eg));

  return S(0, bound);
}

void SetContempt(int* dest, int stm) {
  int contempt = CONTEMPT;
  dest[stm]     = contempt;
  dest[stm ^ 1] = -contempt;
}

// Main evaluation method
Score Evaluate(Board* board, ThreadData* thread) {
  if (IsMaterialDraw(board))
    return 0;

  Score eval = UNKNOWN;
  if (BitCount(OccBB(BOTH)) == 3) {
    eval = EvaluateKXK(board);
  } else if (!(PPieceBB(WHITE_PAWN) | PPieceBB(BLACK_PAWN))) {
    eval = EvaluateMaterialOnlyEndgame(board);
  }

  // A specific endgame calculation returned a score
  if (eval != UNKNOWN)
    return eval;

  EvalData data;
  InitEvalData(&data, board);

  Score s;
  if (!T) {
    s = board->stm == WHITE ? board->mat : -board->mat;
  } else {
    s = MaterialValue(board, WHITE) - MaterialValue(board, BLACK);
  }

  if (!T) {
    PawnHashEntry* pawnEntry = TTPawnProbe(board->pawnZobrist, thread);
    if (pawnEntry != NULL) {
      s += pawnEntry->s;
      data.passedPawns = pawnEntry->passedPawns;
    } else {
      Score pawnS = PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
      TTPawnPut(board->pawnZobrist, pawnS, data.passedPawns, thread);
      s += pawnS;
    }
  } else {
    s += PawnEval(board, &data, WHITE) - PawnEval(board, &data, BLACK);
  }

  s += Imbalance(board, WHITE) - Imbalance(board, BLACK);

  if (T || abs(scoreMG(s) + scoreEG(s)) / 2 < 1024) {
    s += PieceEval(board, &data, WHITE_KNIGHT) - PieceEval(board, &data, BLACK_KNIGHT);
    s += PieceEval(board, &data, WHITE_BISHOP) - PieceEval(board, &data, BLACK_BISHOP);
    s += PieceEval(board, &data, WHITE_ROOK) - PieceEval(board, &data, BLACK_ROOK);
    s += PieceEval(board, &data, WHITE_QUEEN) - PieceEval(board, &data, BLACK_QUEEN);
    s += PieceEval(board, &data, WHITE_KING) - PieceEval(board, &data, BLACK_KING);

    s += PasserEval(board, &data, WHITE) - PasserEval(board, &data, BLACK);
    s += Threats(board, &data, WHITE) - Threats(board, &data, BLACK);
    s += KingSafety(board, &data, WHITE) - KingSafety(board, &data, BLACK);
    s += SpaceEval(board, &data, WHITE) - SpaceEval(board, &data, BLACK);
  }

  // contempt
  s += makeScore(board->phase * thread->contempt[board->stm] / 64,
                 board->phase * thread->contempt[board->stm] / 64);
  s += Complexity(board, scoreEG(s));

  // taper
  int phase = GetPhase(board);
  Score res = (phase * scoreMG(s) + (128 - phase) * scoreEG(s)) / 128;

  if (T)
    C.ss = res >= 0 ? WHITE : BLACK;

  // scale the score
  res = (res * Scale(board, res >= 0 ? WHITE : BLACK)) / MAX_SCALE;
  return ClampEval(TEMPO + (board->stm == WHITE ? res : -res));
}

void EvaluateTrace(Board* board) {
  (void)board;
  printf("HCE eval trace not yet implemented\n");
}
