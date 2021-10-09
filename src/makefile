CC = gcc
SRC = *.c pyrrhic/tbprobe.c noobprobe/noobprobe.c
EXE = berserk
VERSION = 5.0.0-dev
EVALFILE = default.nn

WFLAGS = -std=gnu17 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -flto -march=native -DEVALFILE=\"$(EVALFILE)\" -g -DNDEBUG
RFLAGS = -O3 $(WFLAGS) -flto -static -DEVALFILE=\"$(EVALFILE)\" -g -DNDEBUG

LIBS = -lm -lpthread
ifeq ($(OS), Windows_NT)
	LIBS += -lwsock32
endif

POPCOUNT = -DPOPCOUNT -msse3 -mpopcnt
AVX = -mavx -msse4.1 -mssse3 -msse3 -msse2 -msse
PEXT = -DPEXT -mbmi2

all:
	$(CC) $(CFLAGS) -DPOPCOUNT $(SRC) $(LIBS) -o $(EXE)

release-win:
	md ..\dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64-nopopcnt.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(PEXT) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-pext.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-avx.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX) $(PEXT) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-avx-pext.exe

release:
	mkdir ../dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64-nopopcnt
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(PEXT) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-pext
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-avx
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX) $(PEXT) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-avx-pext

clean:
	rm -rf $(EXE)