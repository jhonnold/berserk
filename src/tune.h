// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#ifndef TUNE_H
#define TUNE_H

// Every SPSA-tunable search constant: INT/FLOAT(NAME, default, min, max).
// Normal builds compile each one to a constant. With SPSA=1 each becomes a
// global variable exposed as a UCI option, and the "spsa" command prints the
// OpenBench SPSA inputs. Ranges are 0..2v (-2|v|..2|v| when negative) unless
// a value must stay inside a bound (divisors > 0, time fractions < 1).
#define TUNABLES(INT, FLOAT)                     \
  /* reduction and pruning tables */             \
  FLOAT(LMR_BASE, 0.1694, 0.0, 0.3388)           \
  FLOAT(LMR_DIV, 2.1350, 1.0, 4.2700)            \
  FLOAT(LMP_BASE, 1.4309, 0.0, 2.8618)           \
  FLOAT(LMP_SCALE, 0.3339, 0.0, 0.6678)          \
  FLOAT(LMP_IMP_BASE, 1.7721, 0.0, 3.5442)       \
  FLOAT(LMP_IMP_SCALE, 0.9801, 0.0, 1.9602)      \
  FLOAT(SEE_QUIET, 13.7791, 0.0, 27.5582)        \
  FLOAT(SEE_NOISY, 108.3466, 0.0, 216.6932)      \
                                                 \
  /* time management */                          \
  FLOAT(TM_ALLOC_TIME, 0.4193, 0.0, 0.8386)      \
  FLOAT(TM_ALLOC_TOTAL, 0.0575, 0.0, 0.1150)     \
  FLOAT(TM_MAX_TIME, 0.9221, 0.0, 0.9500)        \
  FLOAT(TM_MAX_ALLOC, 5.9280, 0.0, 11.8560)      \
  FLOAT(TM_STABILITY_BASE, 1.3110, 0.0, 2.6220)  \
  FLOAT(TM_STABILITY_SCALE, 0.0533, 0.0, 0.1066) \
  FLOAT(TM_SCORE_BASE, 0.1127, 0.0, 0.2254)      \
  FLOAT(TM_SCORE_SEARCH, 0.0262, 0.0, 0.0524)    \
  FLOAT(TM_SCORE_PREV, 0.0261, 0.0, 0.0522)      \
  FLOAT(TM_SCORE_MIN, 0.5028, 0.0, 1.0056)       \
  FLOAT(TM_SCORE_MAX, 1.6561, 0.0, 3.3122)       \
  FLOAT(TM_NODES_MIN, 0.5630, 0.0, 1.1260)       \
  FLOAT(TM_NODES_SCALE, 2.2669, 0.0, 4.5338)     \
  FLOAT(TM_NODES_BASE, 0.4499, 0.0, 0.8998)      \
                                                 \
  /* search */                                   \
  INT(EVAL_FMR_DIV, 200, 101, 400)               \
  INT(ASP_DELTA, 8, 1, 16)                       \
  INT(ASP_GROWTH, 16, 0, 32)                     \
  INT(IIR_DEPTH, 4, 0, 8)                        \
  INT(DECLINE_MARGIN, 79, 0, 158)                \
  INT(RFP_DEPTH, 10, 0, 20)                      \
  INT(RFP_MARGIN, 83, 0, 166)                    \
  INT(RFP_IMPROVING, 148, 0, 296)                \
  INT(RFP_DECLINING, 25, 0, 50)                  \
  INT(RFP_HISTORY, 10790, 0, 21580)              \
  INT(RAZOR_DEPTH, 5, 0, 10)                     \
  INT(RAZOR_MARGIN, 146, 0, 292)                 \
  INT(NMP_DEPTH, 5, 0, 10)                       \
  INT(NMP_BASE, 4, 0, 8)                         \
  INT(NMP_DEPTH_SCALE, 367, 0, 734)              \
  INT(NMP_EVAL_SCALE, 9, 0, 18)                  \
  INT(NMP_EVAL_MAX, 4, 0, 8)                     \
  INT(NMP_VERIFY_DEPTH, 14, 0, 28)               \
  INT(PROBCUT_DEPTH, 6, 0, 12)                   \
  INT(PROBCUT_MARGIN, 168, 0, 336)               \
  INT(LMR_HISTORY, 6144, 0, 12288)               \
  INT(LMR_NOISY_HASH, 1024, 0, 2048)             \
  INT(LMR_NOT_PV, 2048, 0, 4096)                 \
  INT(LMR_NOT_IMPROVING, 1024, 0, 2048)          \
  INT(LMR_KILLER, 2048, 0, 4096)                 \
  INT(LMR_CHECK, 1024, 0, 2048)                  \
  INT(LMR_CUTNODE, 1024, 0, 2048)                \
  INT(LMR_CUTNODE_QUIET, 1024, 0, 2048)          \
  INT(LMR_TT_DEPTH, 1024, 0, 2048)               \
  INT(HP_DEPTH, 4, 0, 8)                         \
  INT(HP_MARGIN, 2850, 0, 5700)                  \
  INT(FP_DEPTH, 10, 0, 20)                       \
  INT(FP_BASE, 66, 0, 132)                       \
  INT(FP_SCALE, 44, 0, 88)                       \
  INT(SE_DEPTH, 6, 0, 12)                        \
  INT(SE_MARGIN, 6, 0, 12)                       \
  INT(SE_TRIPLE_MARGIN, 43, 0, 86)               \
  INT(SE_TRIPLE_LIMIT, 7, 0, 14)                 \
  INT(SE_DOUBLE_MARGIN, 12, 0, 24)               \
  INT(SE_DOUBLE_LIMIT, 6, 0, 12)                 \
  INT(DEEPER_MARGIN, 61, 0, 122)                 \
  INT(HIST_BETA_MARGIN, 75, 0, 150)              \
  INT(QS_FUTILITY, 79, 0, 158)                   \
  INT(SEE_PAWN, 86, 0, 172)                      \
  INT(SEE_KNIGHT, 326, 0, 652)                   \
  INT(SEE_BISHOP, 394, 0, 788)                   \
  INT(SEE_ROOK, 593, 0, 1186)                    \
  INT(SEE_QUEEN, 1047, 0, 2094)                  \
                                                 \
  /* history */                                  \
  INT(HIST_BONUS_QUAD, 4, 0, 8)                  \
  INT(HIST_BONUS_LINEAR, 191, 0, 382)            \
  INT(HIST_BONUS_BASE, -118, -236, 236)          \
  INT(HIST_BONUS_MAX, 1708, 0, 3416)             \
  INT(HIST_MALUS_SCALE, 36, 0, 72)               \
  INT(HIST_UPDATE_DEPTH, 5, 0, 10)               \
  INT(CORR_UPDATE, 3, 0, 6)                      \
  INT(CONT_CORR_UPDATE, 4, 0, 8)                 \
  INT(CORR_MAX, 4096, 0, 8192)                   \
  INT(CONT_CORR_MAX, 4096, 0, 8192)              \
  INT(CORR_PAWN, 31, 0, 62)                      \
  INT(CORR_NON_PAWN, 26, 0, 52)                  \
  INT(CORR_CONT1, 17, 0, 34)                     \
  INT(CORR_CONT2, 46, 0, 92)                     \
                                                 \
  /* move ordering */                            \
  INT(MO_HH, 26, 0, 52)                          \
  INT(MO_CH1, 36, 0, 72)                         \
  INT(MO_CH2, 35, 0, 70)                         \
  INT(MO_CH4, 19, 0, 38)                         \
  INT(MO_CH6, 17, 0, 34)                         \
  INT(MO_THREAT_FROM, 16863, 0, 33726)           \
  INT(MO_THREAT_TO, 13955, 0, 27910)             \
  INT(MO_CAPHIST, 16, 0, 32)                     \
  INT(MO_GOOD_CAPTURE, 32, 0, 64)

#ifdef SPSA

#define TUNE_INT(n, v, lo, hi)   extern int n;
#define TUNE_FLOAT(n, v, lo, hi) extern double n;

typedef struct {
  const char* name;
  int isFloat;
  void* value;
  const char *def, *min, *max; // as written in the list, for exact printing
  double defValue, minValue, maxValue;
} Tunable;

extern const Tunable TUNABLES_TABLE[];
extern const int TUNABLES_COUNT;

#else

#define TUNE_INT(n, v, lo, hi) \
  enum {                       \
    n = (v)                    \
  };
#define TUNE_FLOAT(n, v, lo, hi) static const double n = (v);

#endif

TUNABLES(TUNE_INT, TUNE_FLOAT)

#undef TUNE_INT
#undef TUNE_FLOAT

#endif
