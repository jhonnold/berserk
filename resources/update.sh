#!/bin/bash

# Select a compiler, prioritize Clang
if hash clang 2>/dev/null; then
    cc=clang
elif hash gcc 2>/dev/null; then
    cc=gcc
else
    echo "Please install clang or gcc to compile Berserk!"
    exit 1
fi

# Clone Berserk
echo "Cloning Berserk"
git clone --depth 1 --branch tcec-swiss6 https://github.com/jhonnold/berserk.git
success=$?

if [[ success -eq 0 ]]; then
    echo "Cloning completed successfully"
else
    echo "Unable to clone Berserk!"
    exit 1
fi;

cd berserk/src
echo "Compiling Berserk with make pgo CC=$cc"
make pgo CC=$cc

if test -f berserk; then
    echo "Compilation complete..."
else
    echo "Compilation failed!"
    exit 1
fi

EXE=$PWD/berserk
echo "Setting EXE to $EXE"

$EXE bench 13 > bench.log 2 >&1
grep Results bench.log
