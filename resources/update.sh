#!/bin/bash

REPO_URL=https://github.com/jhonnold/berserk.git
VERSION=4.1.0

echo "Downloading Berserk version $VERSION"
git clone --depth 1 --branch $VERSION $REPO_URL
cd berserk
read -p "Download complete, please enter to continue..."

echo "Compiling Berserk..."
cd src
make release
cd ../dist

echo "Compilation complete..."

EXE=$PWD/berserk-$VERSION-x64-avx2-pext
echo "Setting EXE to $EXE"

$EXE bench
