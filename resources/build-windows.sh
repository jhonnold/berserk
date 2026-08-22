#!/bin/bash
# Cross-compile Berserk release binaries for Windows from Linux.
#
# Requires the mingw-w64 POSIX toolchain (the win32 variant lacks pthread.h):
#   sudo apt install mingw-w64
#
# Produces self-contained, statically-linked .exe files (libgcc + libwinpthread
# bundled) with the network embedded. These are NOT PGO-optimized; users should
# prefer a native `make pgo` build for best performance.
set -uex
cd "$(dirname "$0")/../src"

CC=x86_64-w64-mingw32-gcc-posix
VER=14
ARCHES="x86-64 sse41 avx2 avx512"

for arch in $ARCHES; do
  make clean
  make build ARCH="$arch" CC="$CC" \
       EXE="berserk-$VER-$arch.exe" \
       LIBS="-static -pthread -lm"
done

make clean   # leave the tree clean; keep the .exe files
ls -lh berserk-$VER-*.exe
