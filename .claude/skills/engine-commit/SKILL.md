---
name: engine-commit
description: Commit a change to the Berserk chess engine (github.com/jhonnold/berserk) so it passes CI and follows the house rules. Covers branching off fresh main, the avx2 build, `./berserk bench` and the mandatory `Bench: N` commit line (scripts/bench_line.sh parses it exactly as CI does), equal-bench proof for non-functional changes, the VERSION bump, clang-format and perft checks, shipping a new network as MAIN_NETWORK (multi-ISA and NEON compile checks), and updating an outside contributor's PR. Use it for "commit this to berserk", "commit with the bench line", "what's the bench", "make <net> the main network", "ship network berserk-<sha>", "bump VERSION", "update the version to 20261101b", "push this branch and open the PR" (berserk), "merge main into contributor PR 602". It is not for berserk-datagen commits or PRs (Gitea, no Bench line; use gitea-pr), not for opening, editing, merging or cleaning up the PR itself (use github-pr), not for proving speed (use speedtest), not for OpenBench tests (use ob), and not for local matches (use fastchess).
---

# engine-commit: committing to Berserk

The repo is `~/projects/berserk`. `origin` is github.com/jhonnold/berserk (GitHub, `gh`). Feature worktrees go in
`~/projects/berserk-worktrees/<name>`. Branch prefixes: `feat/`, `perf/`, `refactor/`, `fix/`, `exp/`.

## Procedure

1. **Branch off fresh main.**
   ```sh
   git -C ~/projects/berserk fetch -q origin main
   git -C ~/projects/berserk worktree add -b perf/<name> ~/projects/berserk-worktrees/<name> origin/main
   ```
   Make one behavioural change per branch. If there are two ideas (for example share plus resize), use two branches and test the base idea first.

2. **Build** the way CI does (`.github/workflows/berserk.yml`, "Compile"):
   ```sh
   cd src && make build ARCH=avx2 CC=clang -j16
   ```
   - `build` runs `download-network` and then `all`.
   - A fresh worktree has no `.nn`. Either copy it from `~/projects/berserk/src/` (all released nets are there), or let `download-network` fetch and sha256-check it.
   - For PGO (the `openbench` / `pgo` targets), clang needs `llvm-profdata` on PATH: `PATH=/usr/lib/llvm-19/bin:$PATH make pgo ARCH=avx2 CC=clang`.
   - Other targets are `clean` and `all` (no network step; pass `EVALFILE=` explicitly).
   - ARCH values: `x86-64 sse41 avx2 avx512 avx512vnni arm64 native`.
   - coder has AVX2 only, so avx512 binaries build here but can't run.

3. **Bench.** `./berserk bench` uses the default depth, equal to `bench 13`. CI runs `./src/berserk bench`.
   ```sh
   ../.claude/skills/engine-commit/scripts/bench_line.sh ./berserk      # prints "Bench: 2674956"
   ```
   - **Non-functional change** (refactor, cleanup, speed work): the node count must equal main's. Read main's count with `scripts/bench_line.sh --msg origin/main`. Then prove speed with the **speedtest** skill, or prove identical `.text` (strip debug info first). A bench change means the patch is functional, so stop and say so.
   - **Functional change going to a timed OpenBench test:** no speed test is needed; the timed result already includes any NPS cost.

4. **Local CI parity** (each step mirrors a CI step):
   ```sh
   clang-format --dry-run --Werror $(git diff --name-only origin/main -- 'src/*.c' 'src/*.h' | grep -v -e pyrrhic/ -e incbin.h)
   cd src && clang -std=gnu11 -march=x86-64-v3 -fsyntax-only -Werror -Wall -Wextra -Wshadow -Wstrict-prototypes \
     -Wmissing-prototypes -Wmissing-variable-declarations -Wwrite-strings -Wvla -Wcast-qual \
     -DVERSION='"ci"' -DEVALFILE='"none"' *.c nn/*.c && cd ..
   ./tests/perft.sh >/dev/null && echo perft-ok      # run from the repo root; needs expect (installed)
   ./tests/mate-in-1.sh                               # not in CI; run it for search changes
   ```
   - `~/.local/bin/clang-format` is 21.1.8, the same version CI pins.
   - The system clang is 19; CI uses clang-21. A warning that only clang-21 raises can still fail CI.

5. **Commit** with a conventional subject, a blank line, then the bench line on its own line. Run this from the worktree root:
   ```sh
   BL=.claude/skills/engine-commit/scripts/bench_line.sh
   B=$($BL src/berserk)                                  # "Bench: N"
   git commit -q -F - <<EOF
   perf(nn): <what the change does>

   $B
   EOF
   $BL --check src/berserk HEAD                          # the same check CI runs; OK or FAIL
   ```
   - Keep the bench line when you amend, rebase or cherry-pick.
   - The squash-merge body on main is written by Jay. It often adds the OpenBench result block. Don't pre-write it.

6. **VERSION** (`src/makefile`, `VERSION  = YYYYMMDD`, two spaces before `=`):
   - Every PR merged to main from #603 to #613 changed it to that day's date.
   - A second bump on the same day takes a letter suffix (`20261101b`).
   - Set it to today's date when the PR is headed to main, or to the value Jay names.
   ```sh
   sed -i 's/^\(VERSION  = \)[^[:space:]]*/\120261115/' src/makefile && git diff --numstat src/makefile   # expect "1 1"
   ```

7. **Push and open the PR:** `git push -u origin <branch>`, then hand off to **github-pr**. Keep the body minimal (title plus the bare minimum) and offer anything extra in chat. Use `--draft` for a stack, and open stacked PRs against `main` ("Open the stacked PR against Main so I can review").

## Ship a network (MAIN_NETWORK)

This is the pattern of #609 and #610. Only `src/makefile` changes, and the subject is `feat(nn): network berserk-<sha12>`.

```sh
N=berserk-6fd12bbd95c8.nn
sed -i "s/^\(MAIN_NETWORK = \)[^[:space:]]*/\1$N/; s/^\(VERSION  = \)[^[:space:]]*/\1$(date +%Y%m%d)/" src/makefile   # [^[:space:]]* keeps a trailing \r on an old CRLF branch
git diff --numstat src/makefile        # expect "2 2"
cd src && make build ARCH=avx2 CC=clang -j16 && ../.claude/skills/engine-commit/scripts/bench_line.sh ./berserk
```

- **Network file.** `download-network` fetches `$(NETWORK_BASE)/$N` from the berserk-networks `networks` release and rejects a file whose sha256 doesn't start with the 12 hex digits in the name. The release must exist before CI can pass.
- **Unreleased net.** Build with `make all ARCH=avx2 EVALFILE=<path>`; when EVALFILE differs from MAIN_NETWORK the download step is skipped. If the net's hidden size differs from the current default, the bench must use `EVALFILE=<net>`, or it runs the wrong-size net.
- **Multi-ISA build check** (from #604/#606 sessions; run bench only on what coder can run):
  ```sh
  for a in x86-64 sse41 avx512 avx512vnni; do make -s ARCH=$a EVALFILE=$N EXE=b-$a all 2>&1 | grep -E 'error|warning'; done
  ./b-x86-64 bench 6 | grep Results; ./b-sse41 bench 6 | grep Results; rm -f b-*
  ```
- **NEON, syntax only.** This is the form used in e5ed37b8 for the SIMD files:
  ```sh
  cd src && for f in nn/accumulator.c nn/evaluate.c search.c board.c; do
    clang --target=aarch64-linux-gnu -isystem /usr/include/x86_64-linux-gnu -std=gnu11 -Wall -Wextra -Wshadow -O3 \
      -DNDEBUG -DEVALFILE=\"x\" -DVERSION=\"x\" -fsyntax-only $f 2>&1 | grep -E 'error|warning'; done
  ```
  Jay runs the real NEON and avx512 checks on his own machines: "compile-only, i'll do the checks on other systems that have neon and avx512."

## Outside contributor's PR (as with #602)

```sh
gh pr view 602 -R jhonnold/berserk --json headRepositoryOwner,headRepository,maintainerCanModify,commits
git fetch origin main pull/602/head:pr602 && git worktree add ../berserk-pr602 pr602
# merge or fix there, rebuild, bench, keep the Bench line in the new commit
git push https://github.com/<owner>/berserk.git HEAD:<their-branch>   # only when maintainerCanModify is true
```

## Gotchas

- **Every commit needs `Bench: <N>`, including experiment branches.** CI runs `git show --summary | grep -Po '(?<=Bench: )[0-9]+?(?=$)'`; a missing or wrong line fails CI and emails Jay ("critical").
- **No trailing space after the number.** The CI regex anchors digits to end of line, so `Bench: 2674956 ` is a miss. `bench_line.sh --msg` catches it.
- **Never touch bench when the task is a no-op.** "Don't make change that impacts bench" means an unchanged node count is how a no-op is proved.
- **Speed matters more than readability.** Keep hot-path duplication unless speedtest proves the cleanup costs nothing ("VERY VERY important").
- **Line endings.** `.gitattributes` (#606, 2026-11-03) forces LF for `*.c *.h makefile`. A branch cut before that may still have CRLF files. Check `git ls-files --eol <file>` before a scripted edit; afterwards, `git diff --numstat` should show only the lines you meant to change.
- **Don't commit build output or local nets** (`berserk`, `*.nn`, `config.json`). The main checkout has an untracked `config.json`; leave it alone.
- **The PR body stays minimal.** Jay writes the squash body; results go in chat.

## Links

- CI: `.github/workflows/berserk.yml` (Compile, Strict warnings, Bench, Perft, clang-format).
- Build: `src/makefile`. Tests: `tests/perft.sh`, `tests/mate-in-1.sh`.
- Script: `scripts/bench_line.sh`, tested by `tests/run.sh` with recorded bench output.
