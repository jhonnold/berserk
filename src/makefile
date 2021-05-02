CC = gcc
SRC = *.c
EXE = berserk
VERSION = 4.1.0-dev

LIBS = -lm -lpthread
WFLAGS = -std=gnu11 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -flto -march=native -DNDEBUG
TFLAGS = -O3 $(WFLAGS) -flto -fopenmp -march=native -DTUNE -DNDEBUG 
RFLAGS = -O3 $(WFLAGS) -flto -static -DNDEBUG
DFLAGS = -O0 $(WFLAGS) -g -DNDEBUG

POPCOUNT = -DPOPCOUNT -msse -msse3 -mpopcnt
AVX2 = $(POPCOUNT) -mavx2 -msse4.1
PEXT = $(POPCOUNT) -DPEXT -mbmi2

all:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(EXE)

popcount:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)-x64-popcnt

avx2:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) $(AVX2) -o $(EXE)-x64-avx2

pext:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) $(PEXT) -o $(EXE)-x64-bmi2

release-win:
	md ..\dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(PEXT) -o ../dist/$(EXE)-$(VERSION)-x64-pext.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX2) -o ../dist/$(EXE)-$(VERSION)-x64-avx2.exe

release:
	mkdir ../dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(PEXT) -o ../dist/$(EXE)-$(VERSION)-x64-pext
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(AVX2) -o ../dist/$(EXE)-$(VERSION)-x64-avx2

debug:
	$(CC) $(DFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)

tune:
	$(CC) $(TFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)

clean:
	rm -rf $(EXE)