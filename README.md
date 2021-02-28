# Berserk Chess Engine

A UCI chess engine written in C.

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

Evaluation is tapered from the start to the end of the game using the example on CPW.

- [Tapered](https://www.chessprogramming.org/Tapered_Eval)
- [Material](https://www.chessprogramming.org/Material)
- [Piece Square Tables](https://www.chessprogramming.org/Piece-Square_Tables)
- [Mobility](https://www.chessprogramming.org/Mobility)
- [King Safety](https://www.chessprogramming.org/King_Safety)

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

- [chess22k](https://github.com/sandermvdb/chess22k)
- [bbc](https://github.com/maksimKorzh/chess_programming)
  - [youtube](https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ)
- [Vice](https://github.com/bluefeversoft/Vice_Chess_Engine)
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Ethereal](https://github.com/AndyGrant/Ethereal)
- [CPW](https://www.chessprogramming.org/Main_Page)
- [TalkChess Forum](http://talkchess.com/forum3/viewforum.php?f=7)
