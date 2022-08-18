# Credit to Weiss for this simple and elegant makefile

# General
EXE      = berserk
SRC      = *.c pyrrhic/tbprobe.c noobprobe/*.c
CC       = gcc
VERSION  = 20220818
MAIN_NETWORK = networks/berserk-11a8ee076cec.nn
EVALFILE = $(MAIN_NETWORK)
DEFS     = -DVERSION=\"$(VERSION)\" -DEVALFILE=\"$(EVALFILE)\" -DNDEBUG

# Flags
STD    = -std=gnu11
LIBS   = -pthread -lm
WARN   = -Wall -Wextra -Wshadow -Werror

FLAGS  = $(STD) $(WARN) -O3 -flto -g $(DEFS)
CFLAGS = $(FLAGS) -march=native
RFLAGS = $(FLAGS) -static

# PGO
ifneq ($(findstring gcc, $(CC)),)
	PGODIR = "pgo"
	PGOGEN = -fprofile-generate=$(PGODIR)
	PGOUSE = -fprofile-use=$(PGODIR)
else ifneq ($(findstring clang, $(CC)),)
	PGOMERGE = llvm-profdata merge -output=berserk.profdata *.profraw
	PGOGEN = -fprofile-instr-generate
	PGOUSE = -fprofile-instr-use=berserk.profdata
endif

# Use pext if supported and not a ryzen 1/2 cpu
PROPS = $(shell echo | $(CC) -march=native -E -dM -)
ifneq ($(findstring __BMI2__, $(PROPS)),)
	ifeq ($(findstring __znver1, $(PROPS)),)
		ifeq ($(findstring __znver2, $(PROPS)),)
			CFLAGS += -DUSE_PEXT
		endif
	endif
endif

# Try to detect windows environment by seeing
# whether the shell filters out " or not.
ifeq ($(shell echo "test"), "test")
	BENCH = $(EXE) bench > nul 2>&1
	CLEAN = rmdir /s /q $(PGODIR)
else
	BENCH = ./$(EXE) bench > /dev/null 2>&1
	CLEAN = $(RM) -rf $(PGODIR)
endif

# Link winsock32 on windows for NoobBook
ifeq ($(OS), Windows_NT)
	LIBS += -lwsock32
	EXT = .exe
endif

# Compilations
BASIC   = $(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(EXE)
RELEASE = $(CC) $(RFLAGS) $(SRC) $(LIBS) -o $(EXE)

# Targets
basic: clone-networks
	$(BASIC)

pgo: clone-networks
	$(BASIC) $(PGOGEN)
	$(BENCH)
	$(PGOMERGE)
	$(BASIC) $(PGOUSE)
	$(CLEAN)

release: clone-networks
	$(RELEASE)-$(VERSION)-x64$(EXT)             -march=core2
	strip $(EXE)-$(VERSION)-x64$(EXT)
	
	$(RELEASE)-$(VERSION)-x64-avx2$(EXT)        -march=core-avx2
	strip $(EXE)-$(VERSION)-x64-avx2$(EXT)

	$(RELEASE)-$(VERSION)-x64-avx2-pext$(EXT)   -march=core-avx2 -DUSE_PEXT
	strip $(EXE)-$(VERSION)-x64-avx2-pext$(EXT)

clone-networks:
ifeq ($(EVALFILE), $(MAIN_NETWORK))
	git submodule update --init
endif