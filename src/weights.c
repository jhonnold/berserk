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

// This is an auto-generated file.
// Generated: 84778000, Epoch: 41, Error: 0.07139228, Dataset: C:\Programming\berserk-testing\texel\lichess-big3-resolved.book

#include "weights.h"

// clang-format off
const Score MATERIAL_VALUES[7] = { S(200, 200), S(650, 650), S(650, 650), S(1100, 1100), S(2000, 2000), S(   0,   0), S(   0,   0) };

const Score BISHOP_PAIR = S(40, 192);

const Score PAWN_PSQT[2][32] = {{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 161, 399), S( 257, 541), S( 210, 514), S( 266, 451),
 S( 165, 355), S( 124, 358), S( 198, 304), S( 213, 237),
 S( 110, 278), S( 130, 244), S( 109, 222), S(  87, 173),
 S(  79, 204), S(  63, 192), S(  78, 171), S(  75, 137),
 S(  66, 183), S(  46, 160), S(  61, 158), S(  61, 163),
 S(  63, 181), S(  79, 167), S(  80, 156), S(  90, 162),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
},{
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
 S( 286, 251), S( 122, 315), S( 448, 300), S( 264, 445),
 S( 198, 222), S( 199, 159), S( 260, 162), S( 223, 187),
 S(  92, 185), S( 141, 171), S( 115, 167), S( 121, 138),
 S(  51, 160), S(  65, 157), S(  76, 154), S(  87, 123),
 S(  19, 163), S(  50, 135), S(  65, 155), S(  80, 165),
 S(  15, 171), S(  93, 151), S(  81, 158), S(  77, 180),
 S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
}};

const Score KNIGHT_PSQT[2][32] = {{
 S(-331, 183), S(-377, 259), S(-308, 280), S(-219, 228),
 S(-192, 267), S(-153, 281), S( -96, 247), S( -84, 249),
 S( -70, 223), S( -84, 248), S( -89, 269), S(-112, 276),
 S( -88, 280), S( -79, 268), S( -55, 269), S( -74, 279),
 S( -89, 267), S( -79, 261), S( -52, 284), S( -66, 288),
 S(-132, 223), S(-113, 224), S( -92, 236), S( -98, 270),
 S(-153, 231), S(-122, 216), S(-112, 231), S(-100, 229),
 S(-196, 240), S(-122, 227), S(-120, 227), S(-102, 247),
},{
 S(-426,-147), S(-495, 179), S(-246, 159), S( -86, 215),
 S(-130, 113), S(  17, 174), S( -64, 236), S(-104, 246),
 S( -35, 152), S(-114, 207), S( -10, 151), S( -45, 231),
 S( -66, 232), S( -62, 227), S( -45, 247), S( -65, 273),
 S( -77, 261), S( -97, 251), S( -41, 273), S( -75, 305),
 S( -91, 212), S( -60, 206), S( -79, 224), S( -69, 263),
 S( -93, 222), S( -70, 215), S( -97, 217), S( -99, 225),
 S(-110, 230), S(-136, 232), S(-101, 217), S( -87, 247),
}};

const Score BISHOP_PSQT[2][32] = {{
 S( -62, 319), S(-118, 367), S( -95, 333), S(-140, 370),
 S( -32, 325), S( -55, 294), S(  22, 315), S( -14, 342),
 S(   3, 341), S(  13, 334), S( -45, 293), S(  19, 320),
 S(  -3, 336), S(  48, 321), S(  24, 334), S(   7, 364),
 S(  39, 306), S(  15, 331), S(  39, 329), S(  58, 322),
 S(  31, 280), S(  70, 314), S(  40, 278), S(  46, 310),
 S(  43, 276), S(  39, 213), S(  67, 263), S(  31, 298),
 S(  28, 223), S(  66, 269), S(  42, 305), S(  48, 302),
},{
 S(-148, 230), S(  58, 295), S(-107, 307), S(-163, 315),
 S( -24, 217), S( -88, 290), S( -40, 329), S(  19, 298),
 S(  49, 304), S( -12, 315), S(  -9, 300), S(  14, 318),
 S( -10, 303), S(  51, 309), S(  53, 321), S(  46, 324),
 S(  72, 256), S(  53, 301), S(  61, 321), S(  49, 320),
 S(  57, 261), S(  89, 287), S(  56, 275), S(  46, 333),
 S(  62, 216), S(  62, 218), S(  61, 287), S(  49, 299),
 S(  62, 172), S(  50, 313), S(  25, 313), S(  63, 296),
}};

const Score ROOK_PSQT[2][32] = {{
 S(-303, 561), S(-292, 576), S(-285, 585), S(-346, 589),
 S(-277, 549), S(-293, 572), S(-258, 570), S(-219, 529),
 S(-314, 571), S(-246, 557), S(-270, 559), S(-260, 542),
 S(-298, 592), S(-274, 586), S(-243, 574), S(-235, 542),
 S(-298, 568), S(-288, 575), S(-272, 561), S(-261, 549),
 S(-306, 551), S(-284, 525), S(-274, 532), S(-274, 529),
 S(-294, 508), S(-285, 523), S(-259, 516), S(-252, 502),
 S(-298, 527), S(-282, 514), S(-279, 526), S(-264, 501),
},{
 S(-188, 533), S(-340, 609), S(-410, 612), S(-292, 569),
 S(-240, 501), S(-219, 524), S(-245, 536), S(-278, 541),
 S(-278, 507), S(-173, 506), S(-236, 492), S(-198, 495),
 S(-268, 528), S(-211, 527), S(-229, 515), S(-220, 522),
 S(-297, 512), S(-265, 518), S(-267, 516), S(-245, 527),
 S(-277, 462), S(-237, 456), S(-250, 475), S(-248, 500),
 S(-264, 457), S(-217, 436), S(-247, 458), S(-241, 480),
 S(-289, 481), S(-247, 473), S(-266, 480), S(-244, 478),
}};

const Score QUEEN_PSQT[2][32] = {{
 S(-164, 826), S(  11, 616), S(  70, 643), S(  69, 682),
 S(  -8, 675), S(  48, 592), S(  66, 662), S(  63, 661),
 S(  56, 615), S(  67, 609), S(  68, 646), S(  99, 659),
 S(  47, 667), S(  59, 701), S(  74, 694), S(  50, 708),
 S(  62, 616), S(  33, 721), S(  42, 693), S(  28, 748),
 S(  46, 580), S(  51, 612), S(  46, 656), S(  34, 660),
 S(  50, 514), S(  48, 543), S(  59, 551), S(  56, 587),
 S(  23, 560), S(  21, 562), S(  27, 568), S(  31, 581),
},{
 S( 136, 599), S( 251, 563), S(-101, 781), S(  83, 616),
 S( 106, 580), S(  76, 582), S(  92, 618), S(  50, 696),
 S( 123, 556), S(  99, 525), S( 128, 589), S(  98, 646),
 S(  54, 641), S(  74, 654), S( 113, 588), S(  69, 704),
 S(  72, 580), S(  64, 637), S(  78, 646), S(  39, 734),
 S(  60, 516), S(  77, 566), S(  78, 617), S(  44, 669),
 S(  93, 432), S(  90, 446), S(  80, 499), S(  66, 580),
 S(  76, 499), S(  11, 561), S(  24, 520), S(  42, 545),
}};

const Score KING_PSQT[2][32] = {{
 S( 460,-390), S( 229,-131), S(  16,-133), S( 176,-227),
 S(-235, 143), S(-212, 217), S( -94, 105), S( -60,  32),
 S( -16,  24), S(-180, 146), S(  -9, 107), S(-118, 114),
 S( -29,  46), S( -75,  98), S(-108,  86), S( -89,  50),
 S( -48,  47), S(  -6,  89), S(  44,  66), S( -83,  76),
 S(  -8, -28), S(   4,  32), S(  25,  46), S(  -8,  58),
 S(  73, -53), S( -19,  33), S( -15,  13), S( -36,   8),
 S(  63,-189), S(  73, -93), S(   8,-122), S( -97, -94),
},{
 S(-113,-309), S(-513, 106), S(  22,-130), S( -99,-193),
 S(-396,  67), S(-239, 232), S(-298, 131), S( 264,  -5),
 S(-244,  34), S( -15, 160), S( 230,  81), S(  33, 103),
 S(-218,  19), S( -53,  97), S( -12,  92), S( -56,  56),
 S(-161,  37), S(-137,  82), S(  22,  66), S( -24,  66),
 S(  -2, -16), S(  28,  43), S(  25,  58), S(  33,  66),
 S(  63,  -4), S(   7,  53), S( -11,  38), S( -53,  30),
 S(  42,-122), S(  44, -23), S( -24, -66), S( -88, -74),
}};

const Score KNIGHT_POST_PSQT[12] = {
 S( -97,  22), S(  12,  42), S(  54,  60), S( 124,  90),
 S(  33,  -7), S(  77,  51), S(  61,  60), S(  80,  89),
 S(  42,   0), S(  76,  23), S(  46,  37), S(  41,  49),
};

const Score BISHOP_POST_PSQT[12] = {
 S(  -3,  11), S(  50,   0), S( 116,  29), S( 126,  17),
 S(   7,  -8), S(  49,  26), S(  88,   7), S( 104,  18),
 S(  -9,  43), S(  92,  13), S(  63,  26), S(  73,  41),
};

const Score KNIGHT_MOBILITIES[9] = {
 S(-263, -51), S(-181, 123), S(-143, 206), S(-118, 236),
 S( -99, 263), S( -84, 291), S( -67, 298), S( -51, 304),
 S( -36, 288),};

const Score BISHOP_MOBILITIES[14] = {
 S( -53, 136), S(  -1, 207), S(  26, 256), S(  41, 300),
 S(  54, 319), S(  62, 341), S(  65, 358), S(  73, 365),
 S(  73, 375), S(  81, 377), S(  94, 371), S( 116, 364),
 S( 116, 376), S( 141, 342),};

const Score ROOK_MOBILITIES[15] = {
 S(-272, -70), S(-292, 392), S(-261, 468), S(-251, 470),
 S(-256, 512), S(-246, 527), S(-256, 545), S(-247, 545),
 S(-240, 557), S(-230, 568), S(-228, 579), S(-238, 597),
 S(-233, 604), S(-223, 615), S(-206, 601),};

const Score QUEEN_MOBILITIES[28] = {
 S(-2283,-2399), S(-151,-342), S( -40, 591), S(  -1, 892),
 S(  24, 971), S(  38, 993), S(  37,1060), S(  42,1103),
 S(  49,1133), S(  56,1140), S(  62,1152), S(  70,1153),
 S(  70,1164), S(  79,1163), S(  79,1165), S(  94,1150),
 S(  92,1152), S(  88,1160), S(  99,1126), S( 140,1061),
 S( 170, 995), S( 188, 930), S( 250, 868), S( 312, 698),
 S( 285, 685), S( 300, 564), S( -45, 631), S( 802,-105),
};

const Score KING_MOBILITIES[9] = {
 S(   9, -37), S(   6,  35), S(   1,  44),
 S(  -4,  45), S(  -4,  30), S(  -8,   9),
 S( -15,   4), S(  -9, -17), S(   0, -78),
};

const Score MINOR_BEHIND_PAWN = S(11, 30);

const Score KNIGHT_OUTPOST_REACHABLE = S(20, 44);

const Score BISHOP_OUTPOST_REACHABLE = S(12, 14);

const Score BISHOP_TRAPPED = S(-235, -536);

const Score ROOK_TRAPPED = S(-77, -56);

const Score BAD_BISHOP_PAWNS = S(-2, -9);

const Score DRAGON_BISHOP = S(44, 40);

const Score ROOK_OPEN_FILE_OFFSET = S(29, 13);

const Score ROOK_OPEN_FILE = S(58, 33);

const Score ROOK_SEMI_OPEN = S(34, 16);

const Score ROOK_TO_OPEN = S(29, 41);

const Score QUEEN_OPPOSITE_ROOK = S(-30, 8);

const Score QUEEN_ROOK_BATTERY = S(9, 107);

const Score DEFENDED_PAWN = S(22, 14);

const Score DOUBLED_PAWN = S(27, -87);

const Score ISOLATED_PAWN[4] = {
 S(  -5, -33), S(  -3, -30), S( -13, -18), S(  -3, -22),
};

const Score OPEN_ISOLATED_PAWN = S(-7, -24);

const Score BACKWARDS_PAWN = S(-17, -36);

const Score CONNECTED_PAWN[4][8] = {
 { S(0, 0), S(93, 34), S(0, 61), S(3, 22), S(3, -1), S(5, -2), S(7, -10), S(0, 0),},
 { S(0, 0), S(161, 82), S(39, 85), S(10, 27), S(20, 6), S(13, -4), S(-1, 3), S(0, 0),},
 { S(0, 0), S(127, 149), S(47, 83), S(27, 29), S(13, 8), S(10, 3), S(-1, -11), S(0, 0),},
 { S(0, 0), S(119, 162), S(43, 92), S(20, 36), S(12, 15), S(11, 7), S(9, 4), S(0, 0),},
};

const Score CANDIDATE_PASSER[8] = {
 S(   0,   0), S(   0,   0), S( 389, 265), S(  15, 153),
 S( -30, 137), S( -47,  97), S( -55,  53), S(   0,   0),
};

const Score CANDIDATE_EDGE_DISTANCE = S(10, -32);

const Score PASSED_PAWN[8] = {
 S(   0,   0), S( 311, 486), S( 125, 388), S(  24, 236),
 S( -21, 156), S( -20,  84), S( -14,  75), S(   0,   0),
};

const Score PASSED_PAWN_EDGE_DISTANCE = S(5, -20);

const Score PASSED_PAWN_KING_PROXIMITY = S(-12, 48);

const Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {
 S(   0,   0), S( 184, 545), S(  22, 292), S(  14, 107), S(  20,  29),
};

const Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = S(54, -249);

const Score PASSED_PAWN_SQ_RULE = S(0, 833);

const Score PASSED_PAWN_UNSUPPORTED = S(-44, -11);

const Score PASSED_PAWN_OUTSIDE_V_KNIGHT = S(-3, 172);

const Score KNIGHT_THREATS[6] = { S(-1, 46), S(-6, 104), S(72, 85), S(176, 28), S(145, -104), S(0, 0),};

const Score BISHOP_THREATS[6] = { S(3, 45), S(47, 83), S(0, 0), S(148, 46), S(128, 66), S(206, 3034),};

const Score ROOK_THREATS[6] = { S(-1, 49), S(65, 97), S(69, 118), S(14, 49), S(100, -70), S(403, 2224),};

const Score KING_THREAT = S(26, 71);

const Score PAWN_THREAT = S(173, 89);

const Score PAWN_PUSH_THREAT = S(37, 41);

const Score PAWN_PUSH_THREAT_PINNED = S(109, 273);

const Score HANGING_THREAT = S(23, 45);

const Score KNIGHT_CHECK_QUEEN = S(27, 0);

const Score BISHOP_CHECK_QUEEN = S(46, 48);

const Score ROOK_CHECK_QUEEN = S(41, 14);

const Score SPACE = 191;

const Score IMBALANCE[5][5] = {
 { S(0, 0),},
 { S(19, 59), S(0, 0),},
 { S(11, 49), S(-6, -46), S(0, 0),},
 { S(13, 80), S(-54, -5), S(-17, 6), S(0, 0),},
 { S(101, 108), S(-119, 103), S(-65, 193), S(-496, 377), S(0, 0),},
};

const Score PAWN_SHELTER[4][8] = {
 { S(-31, -12), S(29, 223), S(-16, 97), S(-31, -3), S(22, -26), S(94, -73), S(78, -114), S(0, 0),},
 { S(-85, 22), S(-67, 173), S(-75, 111), S(-66, 54), S(-52, 30), S(57, -33), S(77, -60), S(0, 0),},
 { S(-61, 9), S(-15, 189), S(-97, 93), S(-51, 35), S(-49, 21), S(-24, 9), S(47, -21), S(0, 0),},
 { S(-94, 21), S(104, 113), S(-96, 48), S(-81, 37), S(-81, 25), S(-51, 7), S(-71, 27), S(0, 0),},
};

const Score PAWN_STORM[4][8] = {
 { S(-41, -14), S(-40, 1), S(-39, -8), S(-53, 14), S(-111, 74), S(124, 163), S(561, 167), S(0, 0),},
 { S(-23, 0), S(14, -11), S(16, -8), S(-18, 22), S(-46, 44), S(-43, 145), S(178, 256), S(0, 0),},
 { S(57, -8), S(59, -16), S(50, -5), S(26, 10), S(-14, 28), S(-86, 113), S(137, 174), S(0, 0),},
 { S(6, -4), S(13, -22), S(33, -17), S(9, -25), S(-27, 5), S(-43, 95), S(-146, 278), S(0, 0),},
};

const Score BLOCKED_PAWN_STORM[8] = {
 S(-26, -79), S(84, -115), S(42, -72), S(41, -68), S(23, -61), S(12, -200), S(0, 0), S(0, 0),
};

const Score CAN_CASTLE = S(80, -49);

const Score COMPLEXITY_PAWNS = 4;

const Score COMPLEXITY_PAWNS_BOTH_SIDES = 150;

const Score COMPLEXITY_OFFSET = -197;

const Score KS_ATTACKER_WEIGHTS[5] = {
 0, 33, 32, 19, 25
};

const Score KS_WEAK_SQS = 78;

const Score KS_PINNED = 74;

const Score KS_KNIGHT_CHECK = 279;

const Score KS_BISHOP_CHECK = 311;

const Score KS_ROOK_CHECK = 272;

const Score KS_QUEEN_CHECK = 213;

const Score KS_UNSAFE_CHECK = 57;

const Score KS_ENEMY_QUEEN = -190;

const Score KS_KNIGHT_DEFENSE = -87;

const Score TEMPO = 78;
// clang-format on
