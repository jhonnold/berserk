# Berserk Chess Engine

<img src="resources/berserk.jpg" alt="Berserk" width="400" />

A UCI chess engine written in C. Feel free to challenge me on [Lichess](https://lichess.org/@/BerserkEngine)!

## Strength

### Rating Lists + Elo

Many websites use an [Elo rating system](https://en.wikipedia.org/wiki/Elo_rating_system) to present relative skill amongst engines.
Below is a list of many chess engine lists throughout the web (*variance in Elo is due to different conditions for each list*)

* [CCRL 40/15](https://ccrl.chessdom.com/ccrl/4040/) - **3477 4CPU, 3433 1CPU**
* [CCRL 40/2](https://ccrl.chessdom.com/ccrl/404/) - **3651 4CPU, 3578 1CPU**
* [IpMan Chess](https://ipmanchess.yolasite.com/i9-7980xe.php) - **3422 1CPU**
* [CEGT](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html) - **3532 1CPU**
* [SPCC](https://www.sp-cc.de/) - **3659 1CPU**
* [FGRL](www.fastgm.de/60-0.60.html) - **3518 1CPU**

### Tournaments/Events with Berserk

- [TCEC](https://tcec-chess.com/)
- [CCC](https://www.chess.com/computer-chess-championship)
- [Graham's Broadcasts](https://tlcv.net)

## Funcational Details 

### Board Representation and Move Generation

- [Bitboards](https://www.chessprogramming.org/Bitboards)
  - [Magic Bitboards](https://www.chessprogramming.org/Magic_Bitboards)
- [Staged Move Gen](https://www.chessprogramming.org/Move_Generation#Staged_move_generation)

### Search

- [Negamax](https://www.chessprogramming.org/Negamax)
  - [PVS](https://www.chessprogramming.org/Principal_Variation_Search)
- [Quiescence](https://www.chessprogramming.org/Quiescence_Search)
- [Iterative Deepening](https://www.chessprogramming.org/Iterative_Deepening)
- [Transposition Table](https://www.chessprogramming.org/Transposition_Table)
- [Aspiration Windows](https://www.chessprogramming.org/Aspiration_Windows)
- [Internal Iterative Reductions](https://www.talkchess.com/forum3/viewtopic.php?f=7&t=74769)
- [Reverse Futility Pruning](https://www.chessprogramming.org/Reverse_Futility_Pruning)
- [Razoring](https://www.chessprogramming.org/Razoring)
- [Null Move Pruning](https://www.chessprogramming.org/Null_Move_Pruning)
- [ProbCut](https://www.chessprogramming.org/ProbCut)
- [FutilityPruning](https://www.chessprogramming.org/Futility_Pruning)
  - [LMP](https://www.chessprogramming.org/Futility_Pruning#MoveCountBasedPruning)
- History Pruning
- [SEE](https://www.chessprogramming.org/Static_Exchange_Evaluation)
  - Static Exchange Evaluation Pruning
- [LMR](https://www.chessprogramming.org/Late_Move_Reductions)
- [Killer Heuristic](https://www.chessprogramming.org/Killer_Heuristic)
- [Extensions](https://www.chessprogramming.org/Extensions)
  - [Singular](https://www.chessprogramming.org/Singular_Extensions)
  - [Check](https://www.chessprogramming.org/Check_Extensions)
  - [Recapture](https://www.chessprogramming.org/Recapture_Extensions)
  - History

### Evaluation

- [NNUE](https://www.chessprogramming.org/NNUE)
  - Horizontally Mirrored 16 Buckets
  - 2x(12288 -> 512) -> 1
- [Berserk FenGen](https://github.com/jhonnold/berserk/tree/fen-gen)
- [Koivisto's CUDA Trainer](https://github.com/Luecx/CudAD)
- ~~[Berserk Trainer](https://github.com/jhonnold/berserk-trainer)~~
  - This has been deprecated in favor of Koivisto's trainer, but trained all networks through Berserk 8.5.1+

## Building

```bash
$ git clone https://github.com/jhonnold/berserk
$ cd berserk/src
$ make basic
$ ./berserk
```

## Credit

This engine could not be written without some influence and they are...

### Engine Influences

- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Ethereal](https://github.com/AndyGrant/Ethereal)
- [Koivisto](https://github.com/Luecx/Koivisto)
- [Weiss](https://github.com/TerjeKir/weiss)
- [Chess22k](https://github.com/sandermvdb/chess22k)
- [BBC](https://github.com/maksimKorzh/chess_programming)
- [Cheng](https://www.chessprogramming.org/Cheng)

### Additional Resources

- [Koivisto's CUDA Trainer](https://github.com/Luecx/CudAD)
- [OpenBench](https://github.com/AndyGrant/OpenBench)
- [TalkChess Forum](http://talkchess.com/forum3/viewforum.php?f=7)
- [CCRL](https://kirill-kryukov.com/chess/discussion-board/viewforum.php?f=7)
- [JCER](https://chessengines.blogspot.com/p/rating-jcer.html)
- [Cute Chess](https://cutechess.com/)
- [Arena](http://www.playwitharena.de/)
- [CPW](https://www.chessprogramming.org/Main_Page)
- Lars in Grahams Broadcast rooms

