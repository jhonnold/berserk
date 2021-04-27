CC = gcc
SRC = *.c
EXE = berserk
VERSION = 4.0.0

LIBS = -lm -lpthread
WFLAGS = -std=gnu11 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -flto -march=native -DNDEBUG
TFLAGS = -O3 $(WFLAGS) -flto -fopenmp -march=native -DTUNE -DNDEBUG 
RFLAGS = -O3 $(WFLAGS) -flto -static -DNDEBUG
DFLAGS = -O0 $(WFLAGS) -g -DNDEBUG

POPCOUNT = -DPOPCOUNT -msse3 -mpopcnt

all:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(EXE)

popcount:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)-x64-popcnt

release-win:
	md ..\dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt.exe

release:
	mkdir ../dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt

debug:
	$(CC) $(DFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)

tune:
	$(CC) $(TFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o $(EXE)

clean:
	rm -rf $(EXE)