# Berserk Chess Engine

<img src="resources/berserk.jpg" alt="Berserk" width="400" />

A UCI chess engine written in C.

## Elo History

### CCRL

| **Version** | **Elo** | **TC** |
| ----------- | ------- | ------ |
| 1.2.2       | 2160    | 2'+1"  |
| 2.0.0       | 2546    | 2'+1"  |
| 3.2.0       | 2896    | 2'+1"  |
| 3.2.1       | 2933    | 40/15  |
| 4.1.0       | 3117    | 2'+1"  |
| 4.2.0       |         |        |

### Lars No SMP

| **Version** | **Elo** | **TC** |
| ----------- | ------- | ------ |
| 2.0.0       | ~2600   | 15'    |
| 3.0.0       | 2818    | 15'    |
| 3.2.0       | 2901    | 15'    |
| 4.0.0       | 3027    | 15'    |
| 4.1.0       | 3085    | 15'    |
| 4.2.0       | 3143    | 15'    |

## Features

### Board Representation

Utilizes bitboards for piece representation and magic bitboards for move generation. Uses a pinning concept (from chess22k) to generate mostly legal moves, removes illegal king and EP moves after the fact (like stockfish).

- [Bitboards](https://www.chessprogramming.org/Bitboards) with [Magic bitboards](https://www.chessprogramming.org/Magic_Bitboards) for move generation

### Search

- [Negamax](https://www.chessprogramming.org/Negamax) and [Quiescence](https://www.chessprogramming.org/Quiescence_Search)
- [PVS](https://www.chessprogramming.org/Principal_Variation_Search)
- [Transposition Table](https://www.chessprogramming.org/Transposition_Table)
- [Iterative Deepening](https://www.chessprogramming.org/Iterative_Deepening)
- [Aspiration Windows](https://www.chessprogramming.org/Aspiration_Windows)
- [Null Move Pruning](https://www.chessprogramming.org/Null_Move_Pruning)
- [Delta Pruning](https://www.chessprogramming.org/Delta_Pruning)
- [Reverse Futility Pruning](https://www.chessprogramming.org/Reverse_Futility_Pruning)
- [LMR](https://www.chessprogramming.org/Late_Move_Reductions)
- [MVV-LVA](https://www.chessprogramming.org/MVV-LVA)
- [SEE](https://www.chessprogramming.org/Static_Exchange_Evaluation)
- [Killer Heuristic](https://www.chessprogramming.org/Killer_Heuristic)
- [Countermove Heuristic](https://www.chessprogramming.org/Countermove_Heuristic)

### Evaluation

Evaluation is tapered.

- [Tapered](https://www.chessprogramming.org/Tapered_Eval)
- [Material](https://www.chessprogramming.org/Material)
- [Piece Square Tables](https://www.chessprogramming.org/Piece-Square_Tables)
- [Mobility](https://www.chessprogramming.org/Mobility)
- [Pawn Structure](https://www.chessprogramming.org/Pawn_Structure)
- [King Safety](https://www.chessprogramming.org/King_Safety)
- [Texel Tuned](https://www.chessprogramming.org/Texel's_Tuning_Method)

### Future Improvements

- Phased move generated
  - As well as other general speed improvements
- Candidate passed pawns
- Space evaluation
- Improved king and pawn endgame evaluation
- Tune KS with Texel tuning
- Other things...

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
