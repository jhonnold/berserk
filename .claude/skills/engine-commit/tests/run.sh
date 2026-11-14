#!/usr/bin/env bash
# Fixtures: bench-2674956.txt and bench-2731912.txt are verbatim bench output
# recorded in berserk sessions (main 4bc3a0e = Bench: 2674956; e0ba53c = 2731912).
# The msg-*.txt files are reconstructed in the shape of `git show --summary`;
# their Bench numbers are real main commits (#613 2674956, #610 3014570).
set -u
cd "$(dirname "$0")"
S=../scripts/bench_line.sh; F=fixtures; pass=0; fail=0
t() { # name, expected-exit, expected-stdout, cmd...
  local name=$1 want_rc=$2 want_out=$3; shift 3
  out=$("$@" 2>/dev/null); rc=$?
  if [ "$rc" = "$want_rc" ] && [ "$out" = "$want_out" ]; then pass=$((pass+1)); echo "ok   $name"
  else fail=$((fail+1)); echo "FAIL $name: rc=$rc out='$out' (want rc=$want_rc out='$want_out')"; fi
}
t parse-main          0 'Bench: 2674956' $S --parse $F/bench-2674956.txt
t parse-e0ba53c       0 'Bench: 2731912' $S --parse $F/bench-2731912.txt
t parse-no-results    1 ''               $S --parse $F/bench-no-results.txt
t msg-good            0 '2674956'        $S --msg --file $F/msg-good.txt
t msg-trailing-space  1 ''               $S --msg --file $F/msg-trailing-space.txt
t msg-missing         1 ''               $S --msg --file $F/msg-missing.txt
t run-fake-bin        0 'Bench: 2674956' $S $F/fake-berserk
t check-match         0 'OK: Bench 2674956' $S --check $F/fake-berserk --file $F/msg-good.txt
t check-mismatch      1 ''               $S --check $F/fake-berserk --file $F/msg-3014570.txt
t check-missing-line  1 ''               $S --check $F/fake-berserk --file $F/msg-missing.txt
echo "passed $pass, failed $fail"; [ $fail = 0 ]
