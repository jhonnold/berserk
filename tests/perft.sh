#!/bin/bash
# verify perft numbers (positions from www.chessprogramming.org/Perft_Results)
# ripped from stockfish

error() {
  echo "perft testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "perft testing started"

cat << EOF > perft.exp
   set timeout 10
   lassign \$argv pos depth result
   spawn ./src/berserk
   send "position fen \$pos\\ngo perft \$depth\\n"
   expect "Nodes: \$result" {} timeout {exit 1}
   send "quit\\n"
   expect eof
EOF

expect perft.exp "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 5 4865609
expect perft.exp "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1" 5 4865609
expect perft.exp "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -" 5 193690690
expect perft.exp "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -" 6 11030083
expect perft.exp "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" 5 15833292
expect perft.exp "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" 5 89941194
expect perft.exp "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" 5 164075551
expect perft.exp "4k3/8/8/8/8/8/8/4K2R w K - 0 1" 5 133987
expect perft.exp "4k3/8/8/8/8/8/8/R3K3 w Q - 0 1" 5 145232
expect perft.exp "4k2r/8/8/8/8/8/8/4K3 w k - 0 1" 5 47635
expect perft.exp "r3k3/8/8/8/8/8/8/4K3 w q - 0 1" 5 52710
expect perft.exp "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1" 5 532933
expect perft.exp "r3k2r/8/8/8/8/8/8/4K3 w kq - 0 1" 5 118882
expect perft.exp "8/8/8/8/8/8/6k1/4K2R w K - 0 1" 5 37735
expect perft.exp "8/8/8/8/8/8/1k6/R3K3 w Q - 0 1" 5 80619
expect perft.exp "4k2r/6K1/8/8/8/8/8/8 w k - 0 1" 5 10485
expect perft.exp "r3k3/1K6/8/8/8/8/8/8 w q - 0 1" 5 20780
expect perft.exp "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 5 7594526
expect perft.exp "r3k2r/8/8/8/8/8/8/1R2K2R w Kkq - 0 1" 5 8153719
expect perft.exp "r3k2r/8/8/8/8/8/8/2R1K2R w Kkq - 0 1" 5 7736373
expect perft.exp "r3k2r/8/8/8/8/8/8/R3K1R1 w Qkq - 0 1" 5 7878456
expect perft.exp "1r2k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1" 5 8198901
expect perft.exp "2r1k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1" 5 7710115
expect perft.exp "r3k1r1/8/8/8/8/8/8/R3K2R w KQq - 0 1" 5 7848606
expect perft.exp "4k3/8/8/8/8/8/8/4K2R b K - 0 1" 5 47635
expect perft.exp "4k3/8/8/8/8/8/8/R3K3 b Q - 0 1" 5 52710
expect perft.exp "4k2r/8/8/8/8/8/8/4K3 b k - 0 1" 5 133987
expect perft.exp "r3k3/8/8/8/8/8/8/4K3 b q - 0 1" 5 145232
expect perft.exp "4k3/8/8/8/8/8/8/R3K2R b KQ - 0 1" 5 118882
expect perft.exp "r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1" 5 532933
expect perft.exp "8/8/8/8/8/8/6k1/4K2R b K - 0 1" 5 10485
expect perft.exp "8/8/8/8/8/8/1k6/R3K3 b Q - 0 1" 5 20780
expect perft.exp "4k2r/6K1/8/8/8/8/8/8 b k - 0 1" 5 37735
expect perft.exp "r3k3/1K6/8/8/8/8/8/8 b q - 0 1" 5 80619
expect perft.exp "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1" 5 7594526
expect perft.exp "r3k2r/8/8/8/8/8/8/1R2K2R b Kkq - 0 1" 5 8198901
expect perft.exp "r3k2r/8/8/8/8/8/8/2R1K2R b Kkq - 0 1" 5 7710115
expect perft.exp "r3k2r/8/8/8/8/8/8/R3K1R1 b Qkq - 0 1" 5 7848606
expect perft.exp "1r2k2r/8/8/8/8/8/8/R3K2R b KQk - 0 1" 5 8153719
expect perft.exp "2r1k2r/8/8/8/8/8/8/R3K2R b KQk - 0 1" 5 7736373
expect perft.exp "r3k1r1/8/8/8/8/8/8/R3K2R b KQq - 0 1" 5 7878456
expect perft.exp "8/1n4N1/2k5/8/8/5K2/1N4n1/8 w - - 0 1" 5 570726
expect perft.exp "8/1k6/8/5N2/8/4n3/8/2K5 w - - 0 1" 5 223507
expect perft.exp "8/8/4k3/3Nn3/3nN3/4K3/8/8 w - - 0 1" 5 1198299
expect perft.exp "K7/8/2n5/1n6/8/8/8/k6N w - - 0 1" 5 38348
expect perft.exp "k7/8/2N5/1N6/8/8/8/K6n w - - 0 1" 5 92250
expect perft.exp "8/1n4N1/2k5/8/8/5K2/1N4n1/8 b - - 0 1" 5 582642
expect perft.exp "8/1k6/8/5N2/8/4n3/8/2K5 b - - 0 1" 5 288141
expect perft.exp "8/8/3K4/3Nn3/3nN3/4k3/8/8 b - - 0 1" 5 281190
expect perft.exp "K7/8/2n5/1n6/8/8/8/k6N b - - 0 1" 5 92250
expect perft.exp "k7/8/2N5/1N6/8/8/8/K6n b - - 0 1" 5 38348
expect perft.exp "B6b/8/8/8/2K5/4k3/8/b6B w - - 0 1" 5 1320507
expect perft.exp "8/8/1B6/7b/7k/8/2B1b3/7K w - - 0 1" 5 1713368
expect perft.exp "k7/B7/1B6/1B6/8/8/8/K6b w - - 0 1" 5 787524
expect perft.exp "K7/b7/1b6/1b6/8/8/8/k6B w - - 0 1" 5 310862
expect perft.exp "B6b/8/8/8/2K5/5k2/8/b6B b - - 0 1" 5 530585
expect perft.exp "8/8/1B6/7b/7k/8/2B1b3/7K b - - 0 1" 5 1591064
expect perft.exp "k7/B7/1B6/1B6/8/8/8/K6b b - - 0 1" 5 310862
expect perft.exp "K7/b7/1b6/1b6/8/8/8/k6B b - - 0 1" 5 787524
expect perft.exp "7k/RR6/8/8/8/8/rr6/7K w - - 0 1" 5 2161211
expect perft.exp "R6r/8/8/2K5/5k2/8/8/r6R w - - 0 1" 5 20506480
expect perft.exp "7k/RR6/8/8/8/8/rr6/7K b - - 0 1" 5 2161211
expect perft.exp "R6r/8/8/2K5/5k2/8/8/r6R b - - 0 1" 5 20521342
expect perft.exp "6kq/8/8/8/8/8/8/7K w - - 0 1" 5 14893
expect perft.exp "6KQ/8/8/8/8/8/8/7k b - - 0 1" 5 14893
expect perft.exp "K7/8/8/3Q4/4q3/8/8/7k w - - 0 1" 5 166741
expect perft.exp "6qk/8/8/8/8/8/8/7K b - - 0 1" 5 105749
expect perft.exp "6KQ/8/8/8/8/8/8/7k b - - 0 1" 5 14893
expect perft.exp "K7/8/8/3Q4/4q3/8/8/7k b - - 0 1" 5 166741
expect perft.exp "8/8/8/8/8/K7/P7/k7 w - - 0 1" 5 1347
expect perft.exp "8/8/8/8/8/7K/7P/7k w - - 0 1" 5 1347
expect perft.exp "K7/p7/k7/8/8/8/8/8 w - - 0 1" 5 342
expect perft.exp "7K/7p/7k/8/8/8/8/8 w - - 0 1" 5 342
expect perft.exp "8/2k1p3/3pP3/3P2K1/8/8/8/8 w - - 0 1" 5 7028
expect perft.exp "8/8/8/8/8/K7/P7/k7 b - - 0 1" 5 342
expect perft.exp "8/8/8/8/8/7K/7P/7k b - - 0 1" 5 342
expect perft.exp "K7/p7/k7/8/8/8/8/8 b - - 0 1" 5 1347
expect perft.exp "7K/7p/7k/8/8/8/8/8 b - - 0 1" 5 1347
expect perft.exp "8/2k1p3/3pP3/3P2K1/8/8/8/8 b - - 0 1" 5 5408
expect perft.exp "8/8/8/8/8/4k3/4P3/4K3 w - - 0 1" 5 1814
expect perft.exp "4k3/4p3/4K3/8/8/8/8/8 b - - 0 1" 5 1814
expect perft.exp "8/8/7k/7p/7P/7K/8/8 w - - 0 1" 5 1969
expect perft.exp "8/8/k7/p7/P7/K7/8/8 w - - 0 1" 5 1969
expect perft.exp "8/8/3k4/3p4/3P4/3K4/8/8 w - - 0 1" 5 8296
expect perft.exp "8/3k4/3p4/8/3P4/3K4/8/8 w - - 0 1" 5 23599
expect perft.exp "8/8/3k4/3p4/8/3P4/3K4/8 w - - 0 1" 5 21637
expect perft.exp "k7/8/3p4/8/3P4/8/8/7K w - - 0 1" 5 3450
expect perft.exp "8/8/7k/7p/7P/7K/8/8 b - - 0 1" 5 1969
expect perft.exp "8/8/k7/p7/P7/K7/8/8 b - - 0 1" 5 1969
expect perft.exp "8/8/3k4/3p4/3P4/3K4/8/8 b - - 0 1" 5 8296
expect perft.exp "8/3k4/3p4/8/3P4/3K4/8/8 b - - 0 1" 5 21637
expect perft.exp "8/8/3k4/3p4/8/3P4/3K4/8 b - - 0 1" 5 23599
expect perft.exp "k7/8/3p4/8/3P4/8/8/7K b - - 0 1" 5 3309
expect perft.exp "7k/3p4/8/8/3P4/8/8/K7 w - - 0 1" 5 4661
expect perft.exp "7k/8/8/3p4/8/8/3P4/K7 w - - 0 1" 5 4786
expect perft.exp "k7/8/8/7p/6P1/8/8/K7 w - - 0 1" 5 6112
expect perft.exp "k7/8/7p/8/8/6P1/8/K7 w - - 0 1" 5 4354
expect perft.exp "k7/8/8/6p1/7P/8/8/K7 w - - 0 1" 5 6112
expect perft.exp "k7/8/6p1/8/8/7P/8/K7 w - - 0 1" 5 4354
expect perft.exp "k7/8/8/3p4/4p3/8/8/7K w - - 0 1" 5 3013
expect perft.exp "k7/8/3p4/8/8/4P3/8/7K w - - 0 1" 5 4271
expect perft.exp "7k/3p4/8/8/3P4/8/8/K7 b - - 0 1" 5 5014
expect perft.exp "7k/8/8/3p4/8/8/3P4/K7 b - - 0 1" 5 4658
expect perft.exp "k7/8/8/7p/6P1/8/8/K7 b - - 0 1" 5 6112
expect perft.exp "k7/8/7p/8/8/6P1/8/K7 b - - 0 1" 5 4354
expect perft.exp "k7/8/8/6p1/7P/8/8/K7 b - - 0 1" 5 6112
expect perft.exp "k7/8/6p1/8/8/7P/8/K7 b - - 0 1" 5 4354
expect perft.exp "k7/8/8/3p4/4p3/8/8/7K b - - 0 1" 5 4337
expect perft.exp "k7/8/3p4/8/8/4P3/8/7K b - - 0 1" 5 4271
expect perft.exp "7k/8/8/p7/1P6/8/8/7K w - - 0 1" 5 6112
expect perft.exp "7k/8/8/p7/1P6/8/8/7K b - - 0 1" 5 6112
expect perft.exp "7k/8/8/1p6/P7/8/8/7K w - - 0 1" 5 6112
expect perft.exp "7k/8/8/1p6/P7/8/8/7K b - - 0 1" 5 6112
expect perft.exp "7k/8/p7/8/8/1P6/8/7K w - - 0 1" 5 4354
expect perft.exp "7k/8/p7/8/8/1P6/8/7K b - - 0 1" 5 4354
expect perft.exp "7k/8/1p6/8/8/P7/8/7K w - - 0 1" 5 4354
expect perft.exp "7k/8/1p6/8/8/P7/8/7K b - - 0 1" 5 4354
expect perft.exp "k7/7p/8/8/8/8/6P1/K7 w - - 0 1" 5 7574
expect perft.exp "k7/7p/8/8/8/8/6P1/K7 b - - 0 1" 5 7574
expect perft.exp "k7/6p1/8/8/8/8/7P/K7 w - - 0 1" 5 7574
expect perft.exp "k7/6p1/8/8/8/8/7P/K7 b - - 0 1" 5 7574
expect perft.exp "8/Pk6/8/8/8/8/6Kp/8 w - - 0 1" 5 90606
expect perft.exp "8/Pk6/8/8/8/8/6Kp/8 b - - 0 1" 5 90606
expect perft.exp "3k4/3pp3/8/8/8/8/3PP3/3K4 w - - 0 1" 5 24122
expect perft.exp "3k4/3pp3/8/8/8/8/3PP3/3K4 b - - 0 1" 5 24122
expect perft.exp "8/PPPk4/8/8/8/8/4Kppp/8 w - - 0 1" 5 1533145
expect perft.exp "8/PPPk4/8/8/8/8/4Kppp/8 b - - 0 1" 5 1533145
expect perft.exp "n1n5/1Pk5/8/8/8/8/5Kp1/5N1N w - - 0 1" 5 2193768
expect perft.exp "n1n5/1Pk5/8/8/8/8/5Kp1/5N1N b - - 0 1" 5 2193768
expect perft.exp "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N w - - 0 1" 5 3605103
expect perft.exp "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1" 5 3605103

rm perft.exp

echo "perft testing OK"