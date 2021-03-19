CC = gcc
SRC = *.c
EXE = berserk
VERSION = 3.0.0

LIBS = -lm
WFLAGS = -std=gnu11 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -flto -march=native
RFLAGS = -O3 $(WFLAGS) -flto -static

POPCOUNT = -DPOPCOUNT -msse3 -mpopcnt

all:
	$(CC) $(CFLAGS) $(LIBS) $(SRC) -o $(EXE)

popcount:
	$(CC) $(CFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o $(EXE)-x64-popcnt

release-win:
	md ..\dist
	$(CC) $(RFLAGS) $(LIBS) $(SRC) -o ../dist/$(EXE)-$(VERSION)-x64.exe
	$(CC) $(RFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt.exe

release:
	mkdir ../dist
	$(CC) $(RFLAGS) $(LIBS) $(SRC) -o ../dist/$(EXE)-$(VERSION)-x64
	$(CC) $(RFLAGS) $(LIBS) $(POPCOUNT) $(SRC) -o ../dist/$(EXE)-$(VERSION)-x64-popcnt

clean:
	rm -rf $(EXE)