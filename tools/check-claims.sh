#!/bin/sh
# tools/check-claims.sh — a README number is a claim; fail when one drifts.
#
# Builds the three suites, runs them, and compares each count with the number the
# README states. Exit 1 on a mismatch, so CI goes red rather than the
# documentation going quietly stale.
#
#   CC=cc sh tools/check-claims.sh
set -e
CC=${CC:-cc}
CFLAGS=${CFLAGS:-"-std=c11 -O2 -Wall -Wextra -Werror"}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_suite() {   # name, readme-pattern, build command...
    name=$1; pattern=$2; shift 2
    "$@" >/dev/null 2>&1 || { echo "FAIL: $name did not build"; exit 1; }
    got=$("$tmp/$name" | sed -n 's/^\([0-9][0-9]*\) passed.*/\1/p' | tail -1)
    want=$(grep -oE "$pattern" README.md | head -1 | grep -oE '^[0-9]+')
    [ -n "$got" ] || { echo "FAIL: $name printed no count"; exit 1; }
    [ "$got" = "$want" ] || { echo "FAIL: $name — README says $want, suite ran $got"; exit 1; }
    echo "OK: $name — $got assertions"
}

run_suite demo  '[0-9]+ passed, 0 failed    \(the demo game\)' \
    $CC $CFLAGS -Wno-unused-function -DGK_NO_MAIN -I. -Istub -o "$tmp/demo" main.c stub/raylib_stub.c tests/test_gk.c -lm
run_suite snake '[0-9]+ passed, 0 failed     \(snake\)' \
    $CC $CFLAGS -Wno-unused-function -DSNAKE_HEADLESS -I. -Istub -o "$tmp/snake" examples/snake.c stub/raylib_stub.c tests/test_snake.c -lm
run_suite audio '[0-9]+ passed, 0 failed     \(audio\)' \
    $CC $CFLAGS -Wno-unused-function -I. -Istub -o "$tmp/audio" tests/test_audio.c stub/raylib_stub.c -lm
