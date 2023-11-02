#!/bin/bash
# verify perft numbers (positions from www.chessprogramming.org/Perft_Results)
# ripped from stockfish

error() {
  >&2 echo "perft testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

echo "perft testing started"

cat << EOF > perft.exp
   set timeout 10
   lassign \$argv pos depth result
   spawn ./src/berserk
   send "setoption name UCI_Chess960 value true\n"
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
expect perft.exp "bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9" 5 8146062
expect perft.exp "n1brq1kr/bppppppp/p7/8/4P1Pn/8/PPPP1P2/NBBRQNKR w HDhd - 0 9" 5 9919113
expect perft.exp "qn1rbbkr/ppp2p1p/1n1pp1p1/8/3P4/P6P/1PP1PPPK/QNNRBB1R w hd - 2 9" 5 19836606
expect perft.exp "bbrnkrqn/1ppp1p2/6pp/p3p3/5PP1/2PB4/PP1PP2P/B1RNKRQN w FCfc - 0 9" 5 20138432
expect perft.exp "qnbnr1kr/ppp1b1pp/4p3/3p1p2/8/2NPP3/PPP1BPPP/QNB1R1KR w HEhe - 1 9" 5 24851983
expect perft.exp "qrnkbnrb/pp1p1p2/2p1p1pp/4N3/P4P2/8/1PPPP1PP/QR1KBNRB w GBgb - 0 9" 5 15037464
expect perft.exp "bq1b1krn/pp1ppppp/3n4/2r5/3p3N/6N1/PPP1PPPP/BQRB1KR1 w GCg - 2 9" 5 17546069
expect perft.exp "nrbknqrb/2p1ppp1/1p6/p2p2Bp/1P6/3P1P2/P1P1P1PP/NR1KNQRB w GBgb - 0 9" 5 12225742
expect perft.exp "1rbbnkrn/p1p1pp1p/2q5/1p1p2p1/8/2P3P1/PP1PPP1P/QRBBNKRN w GBgb - 2 9" 5 24328258
expect perft.exp "brqknbr1/pp3ppp/3p2n1/2p1p3/2P5/5P2/PPKPP1PP/BRQ1NBRN w gb - 0 9" 5 9581695
expect perft.exp "rkbrqbnn/1p2ppp1/B1p5/p2p3p/4P2P/8/PPPP1PP1/RKBRQ1NN w DAda - 0 9" 5 17597073
expect perft.exp "nrk1b1qb/pppn1ppp/3rp3/3p4/2P3P1/3P4/PPN1PP1P/1RKRBNQB w DBb - 3 9" 5 33150360
expect perft.exp "rn1qkbnr/p1p1pp1p/bp4p1/3p4/1P6/4P3/P1PP1PPP/RNBQKBNR w HAha - 0 9" 5 21657601
expect perft.exp "nqrbnkbr/2p1p1pp/3p4/pp3p2/6PP/3P1N2/PPP1PP2/NQRB1KBR w HChc - 0 9" 5 9698432
expect perft.exp "bbr1nkrn/ppp1pppp/3q4/3p4/8/P7/1PPPPPPP/BBRQNRKN w gc - 5 9" 5 10870247
expect perft.exp "1rbkrb1q/1pppp1pp/1n5n/p4p2/P3P3/1P6/2PPNPPP/NRBKRB1Q w EBeb - 1 9" 5 5735644
expect perft.exp "nbrkn1bq/p1pppr1p/1p6/5pp1/8/1N2PP2/PPPP2PP/1BKRNRBQ w c - 1 9" 5 6230616
expect perft.exp "nrkr1bnq/1p2pppp/p2p4/1bp5/PP6/1R5N/2PPPPPP/N1KRBB1Q w Ddb - 2 9" 5 16188945
expect perft.exp "qr2krnb/p1p1pppp/b1np4/1p6/3NP3/7P/PPPP1PP1/QRBNKR1B w FBfb - 2 9" 5 12458875
expect perft.exp "rbqnb1rn/p1pp1kpp/1p2pp2/8/4P2P/P5P1/1PPP1P2/RBQNBKRN w GA - 0 9" 5 5282124
expect perft.exp "nrk2bbr/pppqpppp/3p4/8/1P3nP1/3P4/P1P1PP1P/NRKNQBBR w HBhb - 1 9" 5 17603960
expect perft.exp "b1rkrbnq/1pp1pppp/2np4/p5N1/8/1P2P3/P1PP1PPP/BNRKRB1Q w ECec - 0 9" 5 21156664
expect perft.exp "nb1r1nkr/ppp1ppp1/2bp4/7p/3P2qP/P6R/1PP1PPP1/NBQRBNK1 w Dhd - 1 9" 5 88557078
expect perft.exp "qrkb1nbr/1pppppQp/3n4/p7/5p2/1P1N4/P1PPP1PP/1RKB1NBR w HBhb - 0 9" 5 39736157
expect perft.exp "rbkq1rbn/2p1pppp/pp3n2/3p4/5P2/3N2N1/PPPPP1PP/RBKQR1B1 w Afa - 2 9" 5 13643783
expect perft.exp "rnbkqbr1/ppp2ppp/4p1n1/3p4/P7/2N1P3/1PPP1PPP/R1BKQBRN w KQkq - 0 4" 5 29209623
expect perft.exp "r1nknqbr/pp2p1pp/2p2p2/3p4/6P1/PP1P4/2P1PP1b/RBNKNQBR w HAha - 0 9" 5 10708639
expect perft.exp "nrknbbqr/pp3p1p/B3p1p1/2pp4/4P3/2N3P1/PPPP1P1P/NRK1B1QR w HBhb - 0 9" 5 14684565
expect perft.exp "brkr1qnb/pppp2pp/2B1p3/5p2/2n5/6PP/PPPPPPN1/BRKR1QN1 w DBdb - 1 9" 5 20558538
expect perft.exp "rnknbrqb/p1p1pp1p/3p4/1p1N2p1/8/N7/PPPPPPPP/1RK1BRQB w Ffa - 0 9" 5 15257204
expect perft.exp "1qbbn1kr/1ppppppp/r3n3/8/p1P5/P7/1P1PPPPP/RQBBNNKR w HAh - 1 9" 5 22147642
expect perft.exp "rqbknbrn/2pppppp/6Q1/pp6/8/2P5/PP1PPPPP/R1BKNBRN w GAga - 2 9" 5 31296485
expect perft.exp "rqnbbrk1/ppppppp1/8/5n1p/3P3P/2B3P1/PPP1PP2/RQNB1KNR w HA - 0 9" 5 7055215
expect perft.exp "rbbq1k1r/ppp1pppp/7n/1n1p4/5P2/P2P4/1PPBP1PP/RB1QNKNR w HAha - 1 9" 5 17438715
expect perft.exp "nrbkr1qb/1pp1pppp/6n1/p2p4/2P1P3/1N4N1/PP1P1PPP/1RBKR1QB w EBeb - 0 9" 5 14192779
expect perft.exp "rbk1rnbq/pppp1npp/4p3/5p2/4P1P1/7P/PPPP1P1N/RBKNR1BQ w EAea - 1 9" 5 10251465
expect perft.exp "br1knbnr/1qp1pppp/pp1p4/8/8/PP6/2PPPPPP/BRQKNBNR w HBhb - 2 9" 5 15280079
expect perft.exp "bq1rn1kr/1pppppbp/Nn4p1/8/8/P7/1PPPPPPP/BQ1RNBKR w HDhd - 1 9" 5 14692779
expect perft.exp "1brnqknr/2p1pppp/p2p4/1P6/6P1/4Nb2/PP1PPP1P/BBR1QKNR w HChc - 1 9" 5 33322477
expect perft.exp "rkbbn1nr/ppppp1pp/8/6N1/5p2/1q6/P1PPPPPP/RKBBN1QR w HAha - 0 9" 5 1400832
expect perft.exp "bqrb2k1/pppppppr/5nnp/8/3P1P2/4P1N1/PPP3PP/BQRBN1KR w HCc - 1 9" 5 11162476
expect perft.exp "r1bnnkrb/q1ppp1pp/p7/1p3pB1/2P1P3/3P4/PP3PPP/RQ1NNKRB w GAga - 2 9" 5 26316355
expect perft.exp "nr1bqrk1/ppp1pppp/6n1/3pP3/8/5PQb/PPPP2PP/NRBB1KRN w GB - 3 9" 5 17199594
expect perft.exp "b1rbkrqn/ppp2ppp/1n2p3/3p4/6P1/2PP4/PP2PP1P/BRNBKRQN w FBf - 1 9" 5 6307276
expect perft.exp "bnrbkqr1/1p2pppp/6n1/p1pp4/7P/P3P3/1PPPKPP1/BNRB1QRN w gc - 0 9" 5 5356253
expect perft.exp "rkn1bqrb/pnp1pppp/3p4/8/Pp6/1N2NP2/1PPPP1PP/RK2BQRB w GAga - 0 9" 5 12182448
expect perft.exp "r1bkrn1q/ppbppppp/5n2/2p5/3P4/P6N/1PP1PPPP/RBBKRNQ1 w EAea - 3 9" 5 19115128
expect perft.exp "rkrnqnb1/1ppppp2/p5p1/7p/8/P1bPP3/1PP1QPPP/RKRN1NBB w CAca - 0 9" 5 11614241
expect perft.exp "rqnbnkbr/1ppppp2/p5p1/8/1P4p1/4PP2/P1PP3P/RQNBNKBR w HAha - 0 9" 5 16013189
expect perft.exp "1bbrkq1r/pppp2pp/1n2pp1n/8/2PP4/1N4P1/PP2PP1P/1BBRKQNR w HDhd - 1 9" 5 26970098
expect perft.exp "nn1brkqr/pp1bpppp/8/2pp4/P4P2/1PN5/2PPP1PP/N1BBRKQR w HEhe - 1 9" 5 13242252
expect perft.exp "nrbnkrqb/pppp1p1p/4p1p1/8/7P/2P1P3/PPNP1PP1/1RBNKRQB w FBfb - 0 9" 5 5760165
expect perft.exp "2rnbkrb/pqppppp1/1pn5/7p/2P5/P1R5/QP1PPPPP/1N1NBKRB w Ggc - 4 9" 5 11856964
expect perft.exp "rnqbkr1n/1p1ppbpp/3p1p2/p7/8/1P6/P1PPPPPP/R1QBKRBN w FAfa - 0 9" 5 11843134
expect perft.exp "nnrkrbbq/pppp2pp/8/4pp2/4P3/P7/1PPPBPPP/NNKRR1BQ w c - 0 9" 5 16473376
expect perft.exp "bbrk2qr/pp1p1ppp/3n2n1/2p1p3/3P1P2/6N1/PPP1P1PP/BBRKN1QR w HChc - 0 9" 5 19259490
expect perft.exp "brnq1b1r/ppp1ppkp/3p1np1/8/8/5P1P/PPPPPKPR/BRNQNB2 w - - 0 9" 5 6372681
expect perft.exp "bbkr1qnr/2pppppp/2n5/pp6/8/PPN5/1BPPPPPP/1BR1KQNR w HC - 2 9" 5 10554668
expect perft.exp "brkqnrnb/1p1pp1p1/p4p2/2p4p/8/P2PP3/1PP1QPPP/BRK1NRNB w FBfb - 0 9" 5 7830230
expect perft.exp "brn1r1nb/ppppkppp/4p3/8/2PP1P2/8/PP1KP1PP/BRN1RQNB w - - 1 9" 5 12290985
expect perft.exp "nr1qknr1/p1pppp1p/b5p1/1p6/8/P4PP1/1bPPP1RP/NRBQKN1B w Bgb - 0 9" 5 7777833
expect perft.exp "bqnb1rkr/p1p1pppp/3p1n2/1p6/7P/5N1R/PPPPPPP1/BQNB1RK1 w Qkq - 2 4" 5 16300203
expect perft.exp "nrknrbbq/p4ppp/2p1p3/1p1p4/1P2P3/2P5/P1NP1PPP/1RKNRBBQ w EBeb - 0 9" 5 18231199
expect perft.exp "nbnrbk1r/pppppppq/8/7p/8/1N2QPP1/PPPPP2P/NB1RBK1R w HDhd - 2 9" 5 37143354
expect perft.exp "b1nbrknr/1qppp1pp/p4p2/1p6/6P1/P2NP3/1PPP1P1P/BQ1BRKNR w HEhe - 1 9" 5 13157826
expect perft.exp "rkb1nrnb/pppp1pp1/5q1p/8/P3p3/4R1P1/1PPPPP1P/1KBQNRNB w Ffa - 0 9" 5 20209424
expect perft.exp "nrbkn2r/pppp1pqp/4p1p1/8/3P2P1/P3B3/P1P1PP1P/NR1KNBQR w HBhb - 1 9" 5 22094260
expect perft.exp "bqkrnnrb/pppp2p1/4pp2/4P2p/6P1/7P/PPPP1P2/BQRKNNRB w GC - 1 9" 5 8786998
expect perft.exp "bqrkrbnn/1pp1ppp1/8/p6p/3p4/P3P2P/QPPP1PP1/B1RKRBNN w ECec - 0 9" 5 12607528
expect perft.exp "1rnqbkrb/ppp1p1p1/1n3p2/3p3p/P6P/4P3/1PPP1PP1/NRNQBRKB w gb - 0 9" 5 9968830
expect perft.exp "bqrnkbrn/2pp1pp1/p7/1p2p2p/1P6/4N3/P1PPPPPP/BQR1KBRN w GCgc - 0 9" 5 20059741
expect perft.exp "rn1kqbbr/p1pppp1p/1p4p1/1n6/1P2P3/4Q2P/P1PP1PP1/RNNK1BBR w HAha - 1 9" 5 25594662
expect perft.exp "brkbnrq1/1pppp1p1/6np/p4p2/4P3/1PP5/P1KP1PPP/BR1BNRQN w fb - 1 9" 5 15156662
expect perft.exp "bnqbnr1r/p1p1ppkp/3p4/1p4p1/P7/3NP2P/1PPP1PP1/BNQB1RKR w HF - 0 9" 5 23701014
expect perft.exp "1nbnkr1b/rppppppq/p7/7p/1P5P/3P2P1/P1P1PP2/RNBNKRQB w FAf - 1 9" 5 23266182
expect perft.exp "1bbnkqrn/rppppp2/p5p1/7p/7P/P1P1P3/1P1P1PP1/RBBNKQRN w GAg - 1 9" 5 7752404
expect perft.exp "rbbkq1rn/pppppppp/7n/8/P7/3P3P/1PPKPPP1/RBB1QRNN w a - 3 9" 5 5505063
expect perft.exp "1rnbkrbn/1qp1pppp/3p4/pp6/4P3/1NP4P/PP1P1PP1/QR1BKRBN w FBfb - 0 9" 5 10832084
expect perft.exp "qrbbknrn/p1pppp1p/8/6p1/pP6/6N1/2PPPPPP/QRBBK1RN w KQkq - 0 4" 5 36308341
expect perft.exp "brkbqn1r/p2ppppp/7n/1p6/P1p3PP/8/1PPPPP1N/BRKBQ1NR w HBhb - 0 9" 5 8858385
expect perft.exp "1rknnbqr/3ppppp/p7/1pp5/4b2P/P4P2/1PPPP1PR/BRKNNBQ1 w Bhb - 1 9" 5 17275100
expect perft.exp "rbnnq1br/pppp1kp1/4pp2/7p/PP6/2PP4/4PPPP/RBNNQKBR w HA - 0 9" 5 8239159
expect perft.exp "rnknbbrq/ppp2p1p/4p1p1/3p4/3P2P1/2B5/PPP1PP1P/RNKN1BRQ w KQkq - 0 4" 5 25436065
expect perft.exp "rnq1bbkr/1p1ppp1p/4n3/p1p3p1/P1PP4/8/RP2PPPP/1NQNBBKR w Hha - 0 9" 5 17597398
expect perft.exp "rnqkrb1n/ppppp3/6p1/5p1p/2b2P2/P1N5/1PPPP1PP/RQ1KRBBN w EAea - 1 9" 5 15379233
expect perft.exp "rbb1k1rn/p1pqpppp/6n1/1p1p4/5P2/3PP3/PPP1K1PP/RBBQ1NRN w ga - 3 9" 5 13094823
expect perft.exp "rnbkqr1b/1p1pp1pp/p4p1n/2p5/1P5P/N4P2/P1PPP1P1/R1BKQRNB w FAfa - 0 9" 5 7808375
expect perft.exp "rbbn1kqr/pp1pp1p1/2pn3p/5p2/5P2/1P1N4/PNPPP1PP/RBB2KQR w HAha - 1 9" 5 19239812
expect perft.exp "r1nnkrbb/pp1pppp1/2p3q1/7p/8/1PPP3P/P3PPP1/RQNNKRBB w FAfa - 1 9" 5 7508201
expect perft.exp "r1nkbrqb/pppp1p2/n3p1p1/7p/2P2P2/1P6/P2PPQPP/RNNKBR1B w FAfa - 0 9" 5 18742426
expect perft.exp "brqkr1nb/2ppp1pp/1p2np2/p7/2P1PN2/8/PP1P1PPP/BRQKRN1B w EBeb - 0 9" 5 15516491
expect perft.exp "rn1bnqkr/p1ppppp1/8/1p5p/P4P1P/3N4/1PPPP1b1/RNBB1QKR w HAha - 0 9" 5 18862234
expect perft.exp "rknqbbr1/p1pp1pp1/1p4n1/4p2p/4P1P1/6RB/PPPP1P1P/RKNQB2N w Aga - 0 9" 5 17318772
expect perft.exp "bnr1kqrb/pp1pppp1/2n5/2p5/1P4Pp/4N3/P1PPPP1P/BNKR1QRB w gc - 0 9" 5 27792175
expect perft.exp "rknrqbbn/1pp1pp2/p5p1/3p3p/6P1/PN5P/1PPPPP2/RK1RQBBN w DAda - 0 9" 5 11917036
expect perft.exp "qrkbb1r1/ppp1pnpp/3p2n1/5p2/1P3P2/2Q3N1/P1PPP1PP/1RKBB1RN w GBgb - 0 9" 5 30933394
expect perft.exp "r1bkrnqb/pp3ppp/n1ppp3/8/1P5P/P7/R1PPPPP1/1NBKRNQB w Eea - 0 9" 5 7112890
expect perft.exp "1rbbnqkr/1pnppp1p/p5p1/2p5/2P4P/5P2/PP1PP1PR/NRBBNQK1 w Bhb - 1 9" 5 9863080
expect perft.exp "rnbkn1rq/ppppppb1/6p1/7p/2B2P2/1P2P3/P1PP2PP/RNBKN1RQ w GAga - 1 9" 5 20755098
expect perft.exp "nbrn1qkr/ppp1pp2/3p2p1/3Q3P/b7/8/PPPPPP1P/NBRNB1KR w HChc - 2 9" 5 42201531

rm perft.exp

echo "perft testing OK"