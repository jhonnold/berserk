---
ingested: 2026-08-22
source_type: plan
author: Claude Agent
synthesis: done
---

# Remove `setjmp`/`longjmp` from the Berserk search

> Implemented 2026-08-22. Bench verified bit-identical (2,811,728 nodes @ depth 13).

## Context

Berserk aborted a search (time out, node limit, `stop` command) with a non-local
jump. `Search()` armed a `jmp_buf` once per iterative-deepening iteration and
`Negamax`/`Quiesce` fired `longjmp(thread->exit, 1)` from the top of any node.

Three problems:

- **It skipped every pending `UndoMove`.** `thread->board` was left at an
  arbitrary interior node and `board->accumulators` partway up the NNUE
  accumulator stack. Two workarounds existed only to paper over this — a FEN
  reload before the ponder-move probe in `MainSearch`, and a pointer reset at the
  top of `Search()` — both explicitly commented as jmp-abort mitigations.
  Instrumenting the pre-change build showed this firing on **56 of 81** aborted
  searches, with the accumulator pointer off by up to 15 slots.
- **It was formally undefined behavior.** `Search()` has non-`volatile`
  automatics (`searchStability`, `previousBestMove`, `scores[]`, `ss`) modified
  after the `setjmp` and relied upon afterwards, under `-O3 -flto`.
- **It cost a portability `#if`** — MinGW needs `_setjmp(buf, NULL)`, POSIX needs
  `setjmp(buf)`.

The standard alternative is one shared atomic stop flag, polled at the top of
every node, plus an early return immediately after the move is undone that
discards the untrusted value before it can touch root moves, PV, TT, or history.
Berserk already had the flag (`Threads.stop`) and already polled in the right
places; only the unwind mechanism changed.

## What changed

### `CheckLimits` raises `Threads.stop` itself — `src/search.c`

The poll itself now raises the flag, so the unwind condition everywhere else is
just `LoadRlx(Threads.stop)`; no new `ThreadData` field was needed.

```c
  long elapsed = GetTimeMS() - Limits.start;
  if ((Limits.timeset && elapsed >= Limits.max) || //
      (Limits.nodes && NodesSearched() >= Limits.nodes)) {
    Threads.stop = 1;
    return 1;
  }

  return 0;
```

### Node-top checks

The predicate and its position are unchanged in both `Negamax` and `Quiesce`;
only the body became `return 0;` in place of the `longjmp`. The check was kept in
`Quiesce` — dropping it would change when aborts are detected.

### Unwind checks after every recursive call

`if (LoadRlx(Threads.stop)) return 0;` inserted after the board is restored and
before any persistent state is read or written, at seven sites: razoring, null
move, NMP verification, ProbCut, singular, the `Negamax` main move loop, and the
`Quiesce` move loop.

The main-move-loop entry is the critical one. Placing it after `UndoMove` and
before the `if (isRoot)` block is what keeps `rootMoves[i].score`/`.pv`/`.nodes`
holding the last completed iteration's data, and returning from inside the loop
is what keeps the trailing `TTPut`, `UpdateHistories`, and correction updates
from ever running with a garbage score.

### Iterative-deepening loop

The `setjmp` block was deleted and replaced with three explicit breaks that
unwind the aspiration loop, the multiPV loop, and the ID loop in turn. The first
sits **before** `SortRootMoves(thread, thread->multiPV)`, since the longjmp
reached neither the sort nor `PrintUCI`.

The checks were deliberately **not** folded into the `while (++thread->depth ...)`
or `for (thread->multiPV ...)` conditions: that would let the aborted iteration
fall through into the soft-TM / `PrintUCI` block, which the longjmp skipped.
`thread->depth` retains the aborted (pre-incremented) depth either way, so thread
voting via `ThreadValue` is unaffected.

### Removed workarounds

- `#include <setjmp.h>` and `jmp_buf exit;` from `ThreadData` (`src/types.h`)
- `startFen` / `BoardToFen` capture and the `ParseFen(startFen, board)` reload in
  `MainSearch`
- `board->accumulators = thread->accumulators;` at the top of `Search()`

`SearchClearThread`'s copy of that last assignment was left alone — redundant now,
but out of scope and carrying no misleading comment.

## Deliberate deltas

Both follow from `CheckLimits` raising `Threads.stop`:

1. Helper threads stop at the instant the main thread's poll fires, rather than
   when `MainSearch` sets `Threads.stop = 1` microseconds later.
2. `go infinite nodes N` now prints `bestmove` when the node limit is hit instead
   of parking until a `stop` command arrives. Contradictory-flag edge case.

`go ponder` is unaffected — `CheckLimits` still returns early while
`Threads.ponder` is set, so no limit can raise the flag during ponder.

Not touched: `Limits.stopped` and `Limits.quit` are pre-existing dead fields,
zeroed in `ParseGo` and never read.

## Verification performed

- **Bench bit-identical**: `./berserk bench 13` → 2,811,728 nodes, with all 50
  per-position bestmove/score/node lines matching the pre-change build exactly.
  `Bench` sets `Limits.hitrate = INT_MAX` and never raises `Threads.stop`, so the
  abort path is never taken — this proves nothing on the normal search path moved.
  Confirmed on both the plain and PGO builds.
- **Unwind invariant**: temporary assertions on `board->zobrist`, `board->histPly`,
  `board->stm` and `thread->board.accumulators == thread->accumulators` at the end
  of `MainSearch`. 243 searches across 1/4/8 threads: **0 failures**. The same
  assertions on the pre-change build: **112 failures**, confirming the test is not
  vacuous and that the `ParseFen` reload was doing real work.
- **Abort path coverage**: 81 searches per configuration over `go movetime`
  (1–300ms), `go wtime/btime` with and without increment, `go nodes` (1, 37, 5000,
  250000 — `go nodes 1` aborts at the root before any move), `go depth 40` + `stop`,
  `go infinite` + `stop`, `go ponder` + `ponderhit`, `go ponder` + `stop`,
  `go mate 3`. Each produced exactly one `bestmove`, and the engine stayed healthy
  for a following search.
- **Move legality**: 30 aborted searches across 6 positions; every `bestmove` and
  `ponder` move validated against `go perft 1` from the relevant position.
- **Sanitizers**: `-fsanitize=address,undefined -O1` build, full stress suite at 4
  threads — zero diagnostics.
- **Multi-threaded** (helper threads only ever observe `Threads.stop`, so this is
  the path that matters): the unwind assertions were extended to check *every*
  thread's board, accumulator pointer and root move, not just the main thread.
  Clean across 2/4/8/16 threads over ~2,400 searches, including 60 `go infinite`
  runs per config with the `stop` issued at randomized delays from 0.5ms to 250ms
  (so it lands at the first node, mid-subtree, during aspiration re-searches and
  during multiPV), MultiPV 2/3/5 with aborts, randomized `ponderhit`/`stop`
  churn, `setoption Threads` changes between searches, and a 50-ply simulated
  game at a real time control. No hangs, no missing or duplicated `bestmove`.
  The full hard suite also passes under ASan/UBSan at 8 and 16 threads.
- **ThreadSanitizer**: the reported races are compared against a TSan build of the
  pre-change code rather than read in isolation. Over four runs of each, **every
  signature present in the new build is also present in the baseline** — the
  change neither introduces nor removes a race. What TSan finds is all
  pre-existing: the lockless TT (`TTPut`/`TTProbe`, intentional by design,
  ~99% of reports), the global `Limits` struct written by the UCI thread
  in `ParseGo` while the previous search's threads still read it, and the thread
  pool handshake (`MainSearch` reading `thread->depth` / `rootMoves[0].pv.count` /
  `idx`). That last one is a real latent bug worth a separate look:
  `ThreadIdle` writes `thread->action = THREAD_SLEEP` *outside* `thread->mutex`,
  so `ThreadWaitUntilSleep` establishes no happens-before edge with the helper it
  just joined. Unrelated to this change and left alone.
- **Suites**: `tests/perft.sh` and `tests/mate-in-1.sh` pass.
- **Speed**: PGO `bench 13` nps, 8 samples each. Baseline mean ≈2.20M, new mean
  ≈2.21M; run-to-run noise spans 2.06–2.32M. No measurable regression from the
  added branches.

## Known-acceptable residue

`src/search.c` writes one continuation-history update (`UpdateCH`) with an
untrusted score between the LMR search and its re-search on the abort path. This
was a deliberate call: the cost is roughly one update per live stack frame, once
per search, against tables with millions of entries. If an SPRT ever suggests it
matters, guard it with an `UndoMove` + `return 0` immediately after the LMR
search.
