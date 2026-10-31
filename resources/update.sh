#!/bin/bash

set -uex

# Select a compiler, prioritize Clang
# clang's pgo build needs llvm-profdata (through xcrun on Mac)
if hash clang 2>/dev/null && { hash llvm-profdata 2>/dev/null || xcrun -f llvm-profdata >/dev/null 2>&1; }; then
    cc=clang
elif hash gcc 2>/dev/null; then
    cc=gcc
else
    echo "Please install clang or gcc to compile Berserk!"
    exit 1
fi

# Clone Berserk
git clone --depth 1 --branch main https://github.com/jhonnold/berserk.git
success=$?

if [[ success -eq 0 ]]; then
    echo "Cloning completed successfully"
else
    echo "Unable to clone Berserk!"
    exit 1
fi;

cd berserk/src
make pgo CC=$cc ARCH=native

if test -f berserk; then
    echo "Compilation complete..."
else
    echo "Compilation failed!"
    exit 1
fi

EXE=$PWD/berserk

$EXE bench 13 > bench.log 2>&1
grep Results bench.log
