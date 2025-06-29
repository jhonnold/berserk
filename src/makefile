# General
EXE      = berserk
SRC      = attacks.c bench.c berserk.c bits.c board.c eval.c history.c move.c movegen.c movepick.c perft.c random.c \
		   search.c see.c tb.c thread.c transposition.c uci.c util.c zobrist.c nn/accumulator.c nn/evaluate.c pyrrhic/tbprobe.c
CC       = clang
VERSION  = 20250622
MAIN_NETWORK = berserk-9b84c340af7e.nn
EVALFILE = $(MAIN_NETWORK)
DEFS     = -DVERSION=\"$(VERSION)\" -DEVALFILE=\"$(EVALFILE)\" -DNDEBUG

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

openbench: download-network
	$(MAKE) ARCH=avx2 pgo

build: download-network
	$(MAKE) ARCH=$(ARCH) all

pgo: download-network
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

download-network:
	@if [ "$(EVALFILE)" = "$(MAIN_NETWORK)" ]; then \
		echo "Using the current best network: $(EVALFILE)"; \
		if test -f "$(EVALFILE)"; then \
			echo "File already downloaded"; \
		elif hash wget 2>/dev/null; then \
			echo "Downloading $(EVALFILE) with wget"; wget -qO- https://berserk-networks.s3.amazonaws.com/$(EVALFILE) > $(EVALFILE); \
		elif hash curl 2>/dev/null; then \
			echo "Downloading $(EVALFILE) with curl"; curl -skL https://berserk-networks.s3.amazonaws.com/$(EVALFILE) > $(EVALFILE); \
		fi; \
		if test -f "$(EVALFILE)"; then \
			if hash shasum 2>/dev/null; then \
				if [ "$(EVALFILE)" = "berserk-"`shasum -a 256 $(EVALFILE) | cut -c1-12`".nn" ]; then \
					echo "Downloaded network $(EVALFILE) and verified"; \
				else \
					echo "Downloaded network $(EVALFILE) failed validation"; \
					exit 1; \
				fi; \
			elif hash sha256sum 2>/dev/null; then \
				if [ "$(EVALFILE)" = "berserk-"`sha256sum $(EVALFILE) | cut -c1-12`".nn" ]; then \
					echo "Downloaded network $(EVALFILE) and verified"; \
				else \
					echo "Downloaded network $(EVALFILE) failed validation"; \
					exit 1; \
				fi; \
			else \
				echo "Downloaded network $(EVALFILE), but unable to verify"; \
			fi; \
		else \
			echo "Unable to download network: $(EVALFILE)"; \
			exit 1; \
		fi; \
	elif test -f "$(EVALFILE)"; then \
		echo "Using network: $(EVALFILE)"; \
	else \
		echo "Unknown network: $(EVALFILE)"; \
	fi;

clean:
	rm -f $(EXE)
