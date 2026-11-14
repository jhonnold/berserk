#!/usr/bin/env bash
# Bench-line helper for berserk commits. Parses exactly as CI does
# (.github/workflows/berserk.yml, "Bench" step).
#
#   bench_line.sh [BIN]                    run "BIN bench", print "Bench: N"
#   bench_line.sh --parse FILE             print "Bench: N" from saved bench output
#   bench_line.sh --msg [REV|--file F]     print N from a commit message (default HEAD)
#   bench_line.sh --check [BIN] [REV|--file F]
#                                          compare message N with BIN bench; exit 1 on mismatch
# BIN defaults to ./berserk if present, else src/berserk.
set -euo pipefail

nodes_from_output() {  # CI: grep 'Results:' | grep -Po '(?<=[\s]{5})[0-9]+?(?= nodes)'
  grep 'Results:' | grep -Po '(?<=[\s]{5})[0-9]+?(?= nodes)' || true
}
bench_from_msg() {     # CI: git show --summary | grep -Po '(?<=Bench: )[0-9]+?(?=$)'
  grep -Po '(?<=Bench: )[0-9]+?(?=$)' || true
}
default_bin() { if [ -x ./berserk ]; then echo ./berserk; else echo src/berserk; fi; }
msg_text() {           # $1: REV or --file F
  if [ "${1:-}" = --file ]; then cat "$2"; else git show --summary "${1:-HEAD}"; fi
}
die() { echo "$*" >&2; exit 1; }

case "${1:-}" in
  --parse)
    n=$(nodes_from_output < "$2"); [ -n "$n" ] || die "no Results: line in $2"
    echo "Bench: $n" ;;
  --msg)
    shift; n=$(msg_text "$@" | bench_from_msg); [ -n "$n" ] || die "no CI-parsable 'Bench: N' line (check for trailing spaces)"
    echo "$n" ;;
  --check)
    shift; bin=$(default_bin)
    if [ -n "${1:-}" ] && [ "$1" != --file ] && [ -x "$1" ]; then bin=$1; shift; fi
    want=$(msg_text "$@" | bench_from_msg)
    got=$("$bin" bench 2>&1 | nodes_from_output)
    [ -n "$want" ] || die "FAIL: message has no CI-parsable 'Bench: N' line"
    [ -n "$got" ] || die "FAIL: '$bin bench' printed no Results: line"
    [ "$want" = "$got" ] || die "FAIL: got $got, expected $want"
    echo "OK: Bench $got" ;;
  -h|--help) sed -n '2,11p' "$0" ;;
  *)
    bin=${1:-$(default_bin)}
    n=$("$bin" bench 2>&1 | nodes_from_output); [ -n "$n" ] || die "'$bin bench' printed no Results: line"
    echo "Bench: $n" ;;
esac
