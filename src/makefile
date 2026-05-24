# General
EXE      = berserk
SRC      = attacks.c bench.c berserk.c bits.c board.c endgame.c eval.c history.c move.c movegen.c movepick.c pawns.c perft.c random.c \
		   search.c see.c tb.c thread.c transposition.c tuner/tune.c tuner/util.c uci.c util.c weights.c zobrist.c pyrrhic/tbprobe.c
CC       = clang
VERSION  = 4.7.0
DEFS     = -DVERSION=\"$(VERSION)\" -DNDEBUG

# Flags
STD    = -std=gnu11
LIBS   = -pthread -lm
WARN   = -Wall -Wextra -Wshadow

FLAGS   = $(STD) $(WARN) -g -O3 -flto $(PGOFLAGS) $(DEFS)
M64     = -m64 -mpopcnt
MSSE41  = $(M64) -msse -msse2 -mssse3 -msse4.1
MAVX2   = $(MSSE41) -mbmi -mfma -mavx2
MAVX512 = $(MAVX2) -mavx512f -mavx512bw
ARM64   = -arch arm64

XCRUN   =

# Detecting windows
ifeq ($(shell echo "test"), "test")
	FLAGS += -static
endif

# Detecting Mac
KERNEL := $(shell uname -s)
ifeq ($(KERNEL),Darwin)
	XCRUN = xcrun
endif

# Detecting Apple Silicon (ARM64)
UNAME := $(shell uname -m)
ifeq ($(UNAME), arm64)
    ARCH = arm64
endif

# Setup arch
ifeq ($(ARCH), )
   ARCH = native
endif

ifeq ($(ARCH), native)
	CFLAGS = $(FLAGS) -march=native
else ifeq ($(ARCH), arm64)
	CFLAGS = $(FLAGS) $(ARM64)
else ifeq ($(findstring x86-64, $(ARCH)), x86-64)
	CFLAGS = $(FLAGS) $(M64)
else ifeq ($(findstring sse41, $(ARCH)), sse41)
	CFLAGS = $(FLAGS) $(MSSE41)
else ifeq ($(findstring avx2, $(ARCH)), avx2)
	CFLAGS = $(FLAGS) $(MAVX2)
else ifeq ($(findstring avx512, $(ARCH)), avx512)
	CFLAGS = $(FLAGS) $(MAVX512)
endif

ifeq ($(ARCH), native)
	PROPS = $(shell echo | $(CC) -march=native -E -dM -)
	ifneq ($(findstring __BMI2__, $(PROPS)),)
		ifeq ($(findstring __znver1, $(PROPS)),)
			ifeq ($(findstring __znver2, $(PROPS)),)
				CFLAGS += -DUSE_PEXT
			endif
		endif
	endif
else ifeq ($(findstring -pext, $(ARCH)), -pext)
	CFLAGS += -DUSE_PEXT -mbmi2
endif

openbench:
	$(MAKE) ARCH=avx2 pgo

build:
	$(MAKE) ARCH=$(ARCH) all

pgo:
ifeq ($(findstring gcc, $(CC)), gcc)
	$(MAKE) ARCH=$(ARCH) PGOFLAGS="-fprofile-generate=pgo" all

	./$(EXE) bench 13 > pgo.out 2>&1
	grep Results pgo.out

	$(MAKE) ARCH=$(ARCH) PGOFLAGS="-fprofile-use=pgo" all

	@rm -rf pgo pgo.out
else ifeq ($(findstring clang, $(CC)), clang)
	$(MAKE) ARCH=$(ARCH) PGOFLAGS="-fprofile-instr-generate" all

	./$(EXE) bench 13 > pgo.out 2>&1
	grep Results pgo.out

	$(XCRUN) llvm-profdata merge -output=berserk.profdata *.profraw
	$(MAKE) ARCH=$(ARCH) PGOFLAGS="-fprofile-instr-use=berserk.profdata" all

	@rm -rf pgo pgo.out berserk.profdata *.profraw
else
	@echo "PGO builds not supported for $(CC)"
endif

all:
	$(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(EXE)

clean:
	rm -f $(EXE)
