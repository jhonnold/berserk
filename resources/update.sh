#!/bin/bash

REPO_URL=https://github.com/jhonnold/berserk.git

echo "Downloading Berserk"
git clone --depth 1 --branch main $REPO_URL
cd berserk
echo "Download complete"

echo "Compiling Berserk..."
cd src
make
echo "Compilation complete..."

EXE=$PWD/berserk
echo "Setting EXE to $EXE"

$EXE bench
