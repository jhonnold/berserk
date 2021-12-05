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

### CCRL (Blitz)

#### Rank [#14](https://ccrl.chessdom.com/ccrl/404/)

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

### Lars No SMP

#### Rank #6

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

## Features

### Board Representation and Move Generation

- [Bitboards](https://www.chessprogramming.org/Bitboards)
  - In combiniation with [Magic bitboards](https://www.chessprogramming.org/Magic_Bitboards)
- [Legal Move Gen](https://www.chessprogramming.org/Move_Generation)
  - It is [Staged](https://www.chessprogramming.org/Move_Generation#Staged_move_generation)

### Search

- [Negamax](https://www.chessprogramming.org/Negamax) and [Quiescence](https://www.chessprogramming.org/Quiescence_Search)
- [PVS](https://www.chessprogramming.org/Principal_Variation_Search)
- [Transposition Table](https://www.chessprogramming.org/Transposition_Table)
- [Iterative Deepening](https://www.chessprogramming.org/Iterative_Deepening)
- [Aspiration Windows](https://www.chessprogramming.org/Aspiration_Windows)
- [Null Move Pruning](https://www.chessprogramming.org/Null_Move_Pruning)
- [Delta Pruning](https://www.chessprogramming.org/Delta_Pruning)
  - The version in Berserk is a mix of Delta Pruning and Futility Pruning
- [Reverse Futility Pruning](https://www.chessprogramming.org/Reverse_Futility_Pruning)
- [LMR](https://www.chessprogramming.org/Late_Move_Reductions)
- ~~[MVV-LVA](https://www.chessprogramming.org/MVV-LVA)~~
  - Berserk uses history for sorting
- [SEE](https://www.chessprogramming.org/Static_Exchange_Evaluation)
- [Killer Heuristic](https://www.chessprogramming.org/Killer_Heuristic)
- [Countermove Heuristic](https://www.chessprogramming.org/Countermove_Heuristic)
- [Extensions](https://www.chessprogramming.org/Extensions)
  - [Singular](https://www.chessprogramming.org/Singular_Extensions)
  - [Check](https://www.chessprogramming.org/Check_Extensions)
  - [Recapture](https://www.chessprogramming.org/Recapture_Extensions)
  - History

### Evaluation

- [NNUE](https://www.chessprogramming.org/NNUE)
  - "Half KS" shallow network
  - 2x(768 -> 512) -> 1

## Building

At this time Berserk only supports gcc

```bash
$ git clone https://github.com/jhonnold/berserk
$ cd src
$ make
$ ./berserk
```

## Credit

This engine could not be written without some influence and they are...

### Engine Influences

- [chess22k](https://github.com/sandermvdb/chess22k)
- [bbc](https://github.com/maksimKorzh/chess_programming)
  - [youtube](https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ)
- [Martin Sedlak](https://www.chessprogramming.org/Cheng)
  - Very nice and helpful in later development of Berserk
- [Vice](https://github.com/bluefeversoft/Vice_Chess_Engine)
- [Weiss](https://github.com/TerjeKir/weiss)
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Ethereal](https://github.com/AndyGrant/Ethereal)
  - This has been especially helpful as it introduced me to [OpenBench](https://github.com/AndyGrant/OpenBench)
- [CPW](https://www.chessprogramming.org/Main_Page)

### Additional Resources

- [Open Bench](https://github.com/AndyGrant/OpenBench)
- [TalkChess Forum](http://talkchess.com/forum3/viewforum.php?f=7)
- [CCRL](https://kirill-kryukov.com/chess/discussion-board/viewforum.php?f=7)
- [JCER](https://chessengines.blogspot.com/p/rating-jcer.html)
- [Cute Chess](https://cutechess.com/)
- [Arena](http://www.playwitharena.de/)
- Lars in Graham's CCRL rooms for a nice engine list
- [Koivisto](https://github.com/Luecx/Koivisto) authors for advice and assistance on OpenBench
