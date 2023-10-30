#!/bin/bash

error() {
  echo "mate testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "mate testing started"

cat << EOF > mate.exp
   set timeout 2
   lassign \$argv pos result
   spawn ./src/berserk
   send "position fen \$pos\\ngo\\n"
   expect "bestmove \$result" {} timeout {exit 1}
   send "quit\\n"
   expect eof
EOF

expect mate.exp "3k3B/7p/p1Q1p3/2n5/6P1/K3b3/PP5q/R7 w - -" h8f6
expect mate.exp "4bk2/ppp3p1/2np3p/2b5/2B2Bnq/2N5/PP4PP/4RR1K w - -" f4d6
expect mate.exp "4rkr1/1p1Rn1pp/p3p2B/4Qp2/8/8/PPq2PPP/3R2K1 w - -" e5f6
expect mate.exp "5r2/p1n3k1/1p3qr1/7R/8/1BP1Q3/P5R1/6K1 w - -" e3h6
expect mate.exp "5rkr/ppp2p1p/8/3qp3/2pN4/8/PPPQ1PPP/4R1K1 w - -" d2g5
expect mate.exp "6n1/5P1k/7p/np4b1/3B4/1pP4P/5PP1/1b4K1 w - -" f7f8n
expect mate.exp "6q1/R2Q3p/1p1p1ppk/1P1N4/1P2rP2/6P1/7P/6K1 w - -" d7h3
expect mate.exp "8/6P1/5K1k/6N1/5N2/8/8/8 w - -" g7g8n
expect mate.exp "r1b2rk1/pppp2p1/8/3qPN1Q/8/8/P5PP/b1B2R1K w - -" f5e7
expect mate.exp "r1bk3r/p1q1b1p1/7p/nB1pp1N1/8/3PB3/PPP2PPP/R3K2R w KQ -" g5f7
expect mate.exp "r1bknb1r/pppnp1p1/3Np3/3p4/3P1B2/2P5/P3KPPP/7q w - -" d6f7
expect mate.exp "r1bq2kr/pnpp3p/2pP1ppB/8/3Q4/8/PPP2PPP/RN2R1K1 w - -" d4c4
expect mate.exp "r1bqkb1r/pp1npppp/2p2n2/8/3PN3/8/PPP1QPPP/R1B1KBNR w KQkq -" e4d6
expect mate.exp "r1bqr3/pp1nbk1p/2p2ppB/8/3P4/5Q2/PPP1NPPP/R3K2R w KQ -" f3b3
expect mate.exp "r1q1r3/ppp1bpp1/2np4/5b1P/2k1NQP1/2P1B3/PPP2P2/2KR3R w - -" e4d6
expect mate.exp "r2q1bnr/pp1bk1pp/4p3/3pPp1B/3n4/6Q1/PPP2PPP/R1B1K2R w KQ -" g3a3
expect mate.exp "r2q1nr1/1b5k/p5p1/2pP1BPp/8/1P3N1Q/PB5P/4R1K1 w - -" h3h5
expect mate.exp "r2qk2r/pp1n2p1/2p1pn1p/3p4/3P4/B1PB1N2/P1P2PPP/R2Q2K1 w kq -" d3g6
expect mate.exp "r2qkb1r/1bp2ppp/p4n2/3p4/8/5p2/PPP1BPPP/RNBQR1K1 w kq -" e2b5
expect mate.exp "r3kb1r/1p3ppp/8/3np1B1/1p6/8/PP3PPP/R3KB1R w KQkq -" f1b5
expect mate.exp "r3rqkb/pp1b1pnp/2p1p1p1/4P1B1/2B1N1P1/5N1P/PPP2P2/2KR3R w - -" e4f6
expect mate.exp "r5r1/pQ5p/1qp2R2/2k1p3/P3P3/2PP4/2P3PP/6K1 w - -" b7e7
expect mate.exp "r5r1/pppb1p2/3npkNp/8/3P2P1/2PB4/P1P1Q2P/6K1 w - -" e2e5
expect mate.exp "r7/1p4b1/p3Bp2/6pp/1PNN4/1P1k4/KB4P1/6q1 w - -" e6f5
expect mate.exp "rb6/k1p4R/P1P5/PpK5/8/8/8/5B2 w - b6" a5b6
expect mate.exp "rk5r/p1q2ppp/Qp1B1n2/2p5/2P5/6P1/PP3PBP/4R1K1 w - -" a6b7
expect mate.exp "rn1qkbnr/ppp2ppp/8/8/4Np2/5b2/PPPPQ1PP/R1B1KB1R w KQkq -" e4f6
expect mate.exp "rn6/pQ5p/6r1/k1N1P3/3P4/4b1p1/1PP1K1P1/8 w - -" b2b4
expect mate.exp "rnb3r1/pp1pb2p/2pk1nq1/6BQ/8/8/PPP3PP/4RRK1 w - -" g5f4
expect mate.exp "rnbq3r/pppp2pp/1b6/8/1P2k3/8/PBPP1PPP/R2QK2R w KQ -" d1f3
expect mate.exp "rnbqkb1r/ppp2ppp/8/3p4/8/2n2N2/PP2BPPP/R1B1R1K1 w kq -" e2b5
expect mate.exp "rnbqkr2/pp1pbN1p/8/3p4/2B5/2p5/P4PPP/R3R1K1 w q -" f7d6
expect mate.exp "1k1r4/2p2ppp/8/8/Qb6/2R1Pn2/PP2KPPP/3r4 b - -" f3g1
expect mate.exp "1n4rk/1bp2Q1p/p2p4/1p2p3/5N1N/1P1P3P/1PP2p1K/8 b - -" f2f1n
expect mate.exp "1r3r1k/6pp/6b1/pBp3B1/Pn1N2P1/4p2P/1P6/2KR3R b - -" b4a2
expect mate.exp "2B1nrk1/p5bp/1p1p4/4p3/8/1NPKnq1P/PP1Q4/R6R b - -" e5e4
expect mate.exp "2k2r2/1pp4P/p2n4/2Nn2R1/1P1P4/P1RK2Q1/1r4b1/8 b - -" g2f1
expect mate.exp "2k5/pp4pp/1b6/2nP4/5pb1/P7/1P2QKPP/5R2 b - -" c5d3
expect mate.exp "2k5/ppp2p2/7q/6p1/2Nb1p2/1B3Kn1/PP2Q1P1/8 b - -" h6h5
expect mate.exp "3r2k1/ppp2ppp/6Q1/b7/3n1B2/2p3n1/P4PPP/RN3RK1 b - -" d4e2
expect mate.exp "3rkr2/5p2/b1p2p2/4pP1P/p3P1Q1/b1P5/B1K2RP1/2RNq3 b - -" a6d3
expect mate.exp "4r1k1/pp3ppp/6q1/3p4/2bP1n2/P1Q2B2/1P3PPP/6KR b - -" f4h3
expect mate.exp "5rk1/5ppp/p7/1pb1P3/7R/7P/PP2b2P/R1B4K b - -" e2f3
expect mate.exp "6k1/5qpp/pn1p2N1/B1p2p1P/Q3p3/2K1P2R/1r2BPP1/1r5R b - -" b6a4
expect mate.exp "6rk/8/8/8/6q1/6Qb/7P/5BKR b - -" g4d4
expect mate.exp "7r/6k1/8/7q/6p1/6P1/5PK1/3R2Q1 b - -" h5h3
expect mate.exp "8/1k5q/8/4Q3/8/1n1P4/NP6/1K6 b - -" h7d3
expect mate.exp "8/2N3p1/5b2/k1B2P2/pP4R1/8/K1nn4/8 b - b3" a4b3
expect mate.exp "8/8/pp3Q2/7k/5Pp1/P1P3K1/3r3p/8 b - -" h2h1n
expect mate.exp "8/p2k4/1p5R/2pp2R1/4n3/P2K4/1PP1N3/5r2 b - -" f1f3
expect mate.exp "8/p4pkp/8/3B1b2/3b1ppP/P1N1r1n1/1PP3PR/R4QK1 b - -" e3e1
expect mate.exp "r1b1k2r/ppp1qppp/5B2/3Pn3/8/8/PPP2PPP/RN1QKB1R b KQkq -" e5f3
expect mate.exp "r1b1kbnr/pppp1Npp/8/8/3nq3/8/PPPPBP1P/RNBQKR2 b Qkq -" d4f3
expect mate.exp "r1b1q1kr/ppNnb1pp/5n2/8/3P4/8/PPP2PPP/R1BQKB1R b KQ -" e7b4
expect mate.exp "r1b3k1/pppn3p/3p2rb/3P1K2/2P1P3/2N2P2/PP1QB3/R4R2 b - -" d7b8
expect mate.exp "r1b3r1/5k2/1nn1p1p1/3pPp1P/p4P2/Kp3BQN/P1PBN1P1/3R3R b - -" b6c4
expect mate.exp "r1bqk1nr/pppp1ppp/8/2b1P3/3nP3/6P1/PPP1N2P/RNBQKB1R b KQkq -" d4f3
expect mate.exp "r2Bk2r/ppp2p2/3b3p/8/1n1PK1b1/4P3/PPP2pPP/RN1Q1B1R b kq -" f7f5
expect mate.exp "r2qk2r/pp3ppp/2p1p3/5P2/2Qn4/2n5/P2N1PPP/R1B1KB1R b KQkq -" d4c2
expect mate.exp "r2r2k1/ppp2pp1/5q1p/4p3/4bn2/2PB2N1/P1PQ1P1P/R4RK1 b - -" f4h3
expect mate.exp "r3k1nr/p1p2p1p/2pP4/8/7q/7b/PPPP3P/RNBQ2KR b kq -" h4d4
expect mate.exp "r3k3/bppbq2r/p2p3p/3Pp2n/P1N1Pp2/2P2P1P/1PB3PN/R2QR2K b q -" h5g3
expect mate.exp "r4k1N/2p3pp/p7/1pbPn3/6b1/1P1P3P/1PP2qPK/RNB4Q b - -" e5f3
expect mate.exp "r6r/pppk1ppp/8/2b5/2P5/2Nb1N2/PPnK1nPP/1RB2B1R b - -" c5e3

rm mate.exp

echo "mate testing OK"