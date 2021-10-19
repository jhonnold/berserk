CC = gcc
SRC = *.c pyrrhic/tbprobe.c noobprobe/noobprobe.c
EXE = berserk
VERSION = 7-dev
EVALFILE = default.nn

DEFS = -DVERSION=\"$(VERSION)\" -DEVALFILE=\"$(EVALFILE)\" -DNDEBUG

WFLAGS = -std=gnu17 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -g -flto -march=native
RFLAGS = -O3 $(WFLAGS) -g -flto -static

LIBS = -lm -lpthread
ifeq ($(OS), Windows_NT)
	LIBS += -lwsock32
	EXT = .exe
endif

SSSE = -mssse3 -msse3 -msse2 -msse -mpopcnt
AVX2 = -mavx2 -mavx -mfma -msse4.1 $(SSSE)
PEXT = -mbmi2 -DUSE_PEXT

all:
	$(CC) $(CFLAGS) $(DEFS) $(SRC) $(LIBS) -o $(EXE)

clean:
	rm -rf $(EXE)

ssse:
	$(CC) $(RFLAGS) $(DEFS) $(SRC) $(LIBS) $(SSSE) -o $(EXE)-$(VERSION)-x64$(EXT)

pext:
	$(CC) $(RFLAGS) $(DEFS) $(SRC) $(LIBS) $(PEXT) -o $(EXE)-$(VERSION)-x64-pext$(EXT)

avx2:
	$(CC) $(RFLAGS) $(DEFS) $(SRC) $(LIBS) $(AVX2) -o $(EXE)-$(VERSION)-x64-avx2$(EXT)

avx2-pext:
	$(CC) $(RFLAGS) $(DEFS) $(SRC) $(LIBS) $(AVX2) $(PEXT) -o $(EXE)-$(VERSION)-x64-avx2-pext$(EXT)

release: ssse pext avx2 avx2-pext
