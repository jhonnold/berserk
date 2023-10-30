# Credit to Weiss for this simple and elegant makefile

# General
EXE      = berserk
SRC      = *.c nn/*.c pyrrhic/tbprobe.c
CC       = gcc
VERSION  = 12
MAIN_NETWORK = berserk-fb675dad41b4.nn
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
	BENCH = $(EXE) bench 12 > nul 2>&1
	CLEAN = rmdir /s /q $(PGODIR)
else
	BENCH = ./$(EXE) bench 12 > /dev/null 2>&1
	CLEAN = $(RM) -rf $(PGODIR)
endif

# Compilations
BASIC   = $(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(EXE)
RELEASE = $(CC) $(RFLAGS) $(SRC) $(LIBS) -o $(EXE)

# Targets
basic: download-network
	$(BASIC)

pgo: download-network
	$(BASIC) $(PGOGEN)
	$(BENCH)
	$(PGOMERGE)
	$(BASIC) $(PGOUSE)
	$(CLEAN)

release: download-network
	$(RELEASE)-$(VERSION)-x64-ssse3$(EXT)        -march=core2
	$(RELEASE)-$(VERSION)-x64-avx2$(EXT)         -march=core-avx2
	$(RELEASE)-$(VERSION)-x64-avx2-pext$(EXT)    -march=core-avx2 -DUSE_PEXT
	$(RELEASE)-$(VERSION)-x64-avx512$(EXT)       -march=core-avx2 -mavx512f -mavx512bw
	$(RELEASE)-$(VERSION)-x64-avx512-pext$(EXT)  -march=core-avx2 -mavx512f -mavx512bw -DUSE_PEXT

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
				fi; \
			elif hash sha256sum 2>/dev/null; then \
				if [ "$(EVALFILE)" = "berserk-"`sha256sum $(EVALFILE) | cut -c1-12`".nn" ]; then \
					echo "Downloaded network $(EVALFILE) and verified"; \
				else \
					echo "Downloaded network $(EVALFILE) failed validation"; \
				fi; \
			else \
				echo "Downloaded network $(EVALFILE), but unable to verify"; \
			fi; \
		else \
			echo "Unable to downlaod network: $(EVALFILE)"; \
		fi; \
	elif test -f "$(EVALFILE)"; then \
		echo "Using network: $(EVALFILE)"; \
	else \
		echo "Unknown network: $(EVALFILE)"; \
	fi;
