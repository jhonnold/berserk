CC = gcc
SRC = *.c
EXE = berserk
VERSION = 3.3.0

LIBS = -lm
WFLAGS = -std=gnu11 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -flto -march=native -DNDEBUG
TFLAGS = -O3 $(WFLAGS) -flto -march=native -DTUNE -DNDEBUG
RFLAGS = -O3 $(WFLAGS) -flto -static -DNDEBUG
DFLAGS = -O0 $(WFLAGS) -g

POPCOUNT = -DPOPCOUNT -msse3 -mpopcnt

all:
	$(CC) $(CFLAGS) $(LIBS) $(SRC) -o $(EXE)

popcount:
	$(CC) $(CFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o $(EXE)-x64-popcnt

release-win:
	md ..\dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64.exe
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt.exe

release:
	mkdir ../dist
	$(CC) $(RFLAGS) $(SRC) $(LIBS) -o ../dist/$(EXE)-$(VERSION)-x64
	$(CC) $(RFLAGS) $(SRC) $(LIBS) $(POPCOUNT) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt

debug:
	$(CC) $(DFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o $(EXE)

tune:
	$(CC) $(TFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o $(EXE)

clean:
	rm -rf $(EXE)