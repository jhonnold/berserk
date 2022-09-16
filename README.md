# Berserk Chess Engine

<img src="resources/berserk.jpg" alt="Berserk" width="400" />

A UCI chess engine written in C. Feel free to challenge me on [Lichess](https://lichess.org/@/BerserkEngine)!

## Elo History

### CCRL (40/15)

#### Rank [#5](https://ccrl.chessdom.com/ccrl/4040/rating_list_pure_single_cpu.html)

| **Version** | **Elo** | **TC** |
| ----------- | ------- | ------ |
| 3.2.1       | 2935    | 40/15  |
| 4.2.0       | 3131    | 40/15  |
| 4.2.0 (4CPU)| 3224    | 40/15  |
| 4.5.1       | 3294    | 40/15  |
| 6           | 3327    | 40/15  |
| 6 (4CPU)    | 3395    | 40/15  |
| 7           | 3387    | 40/15  |
| 7 (4CPU)    | 3435    | 40/15  |
| 8           | 3399    | 40/15  |
| 8 (4CPU)    | 3453    | 40/15  |
| 8.5.1       | 3422    | 40/15  |
| 8.5.1 (4CPU)| 3467    | 40/15  |
| 9           | 3434    | 40/15  |
| 9 (4CPU)    | 3472    | 40/15  |

### CCRL (Blitz)

#### Rank [#7](https://ccrl.chessdom.com/ccrl/404/)

| **Version** | **Elo** | **TC** |
| ----------- | ------- | ------ |
| 1.2.2       | 2160    | 2'+1"  |
| 2.0.0       | 2546    | 2'+1"  |
| 3.2.0       | 2896    | 2'+1"  |
| 4.1.0       | 3117    | 2'+1"  |
| 4.4.0       | 3316    | 2'+1"  |
| 4.4.0 (8CPU)| 3467    | 2'+1"  |
| 6           | 3436    | 2'+1"  |
| 6 (8CPU)    | 3559    | 2'+1"  |
| 7           | 3488    | 2'+1"  |
| 7 (8CPU)    | 3600    | 2'+1"  |
| 8           | 3518    | 2'+1"  |
| 8 (8CPU)    | 3626    | 2'+1"  |
| 8.5.1       | 3540    | 2'+1"  |
| 8.5.1 (8CPU)| 3638    | 2'+1"  |
| 9           | 3589    | 2'+1"  |
| 9 (8 CPU)   | 3667    | 2'+1"  |

### Lars No SMP

#### Rank #2

| **Version** | **Elo** | **TC** |
| ----------- | ------- | ------ |
| 2.0.0       | ~2600   | 15'    |
| 3.0.0       | 2818    | 15'    |
| 3.2.0       | 2901    | 15'    |
| 4.0.0       | 3027    | 15'    |
| 4.1.0       | 3085    | 15'    |
| 4.2.0       | 3143    | 15'    |
| 4.3.0       | 3248    | 15'    |
| 4.4.0       | 3314    | 15'    |
| 4.5.0       | 3344    | 15'    |
| 5           | 3371    | 15'    |
| 6           | 3446    | 15'    |
| 9           | 3619    | 15'    |

### Other Lists with Berserk

- [Ipman Chess](https://ipmanchess.yolasite.com/i9-7980xe.php)
- [CEGT](http://www.cegt.net/)
- [Stefan Pohl Computer Chess](https://www.sp-cc.de/)
- [FGRL](http://www.fastgm.de/)
- [Gambit Rating List](https://rebel13.nl/grl-best-40-2.html)

### Events with Berserk

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
- [Countermove Heuristic](https://www.chessprogramming.org/Countermove_Heuristic)
- [Extensions](https://www.chessprogramming.org/Extensions)
  - [Singular](https://www.chessprogramming.org/Singular_Extensions)
  - [Check](https://www.chessprogramming.org/Check_Extensions)
  - [Recapture](https://www.chessprogramming.org/Recapture_Extensions)
  - History

### Evaluation

- [NNUE](https://www.chessprogramming.org/NNUE)
  - Horizontally Mirrored 8 Buckets
  - 2x(6144 -> 512) -> 1
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

- [Chess22k](https://github.com/sandermvdb/chess22k)
- [BBC](https://github.com/maksimKorzh/chess_programming)
- [Cheng](https://www.chessprogramming.org/Cheng)
- [Vice](https://github.com/bluefeversoft/Vice_Chess_Engine)
- [Weiss](https://github.com/TerjeKir/weiss)
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Ethereal](https://github.com/AndyGrant/Ethereal)
- [Koivisto](https://github.com/Luecx/Koivisto)


### Additional Resources

- [Koivisto's CUDA Trainer](https://github.com/Luecx/CudAD)
- [Open Bench](https://github.com/AndyGrant/OpenBench)
- [TalkChess Forum](http://talkchess.com/forum3/viewforum.php?f=7)
- [CCRL](https://kirill-kryukov.com/chess/discussion-board/viewforum.php?f=7)
- [JCER](https://chessengines.blogspot.com/p/rating-jcer.html)
- [Cute Chess](https://cutechess.com/)
- [Arena](http://www.playwitharena.de/)
- [CPW](https://www.chessprogramming.org/Main_Page)
- Lars in Grahams Broadcast rooms

