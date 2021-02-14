CC = gcc
SRC = *.c
EXE = berserk

WFLAGS = -std=gnu11 -Wall -Wextra -Wshadow
CFLAGS = -O3 $(WFLAGS) -DNDEBUG -flto -march=native

all:
	$(CC) $(CFLAGS) $(SRC) -o $(EXE)

clean:
	rm -rf berserk