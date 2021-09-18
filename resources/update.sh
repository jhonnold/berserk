#!/bin/bash

REPO_URL=https://github.com/jhonnold/berserk.git
VERSION=4.6.0

echo "Downloading Berserk version $VERSION"
git clone --depth 1 --branch main
cd berserk
read -p "Download complete, please enter to continue..."

echo "Compiling Berserk..."
cd src
sed -i.bak "s/ -static/ /" makefile
make release
cd ../dist

echo "Compilation complete..."

ln -s berserk-$VERSION-x64-avx2-pext berserk-x64-avx2-pext
EXE=$PWD/berserk-x64-avx2-pext
echo "Setting EXE to $EXE"

$EXE bench
