# tools/Makefile
# @guterion
# CC-BY-SA-4.0
# Build the generators, then rebuild the assets and the pages

# --- TOOLCHAIN --------------------------------------------------------
# Fil-C gives these programs memory safety while they run: every pointer
# carries its bounds, so an overrun traps instead of corrupting the
# heap. Point FILC at another installation to build against a different
# version of it.
FILC ?= $(HOME)/.local/opt/filc-0.683-linux-x86_64
CC = $(FILC)/build/bin/clang

# --- FLAGS ------------------------------------------------------------
# The safety envelope comes first; the optimisation fits inside it.
# Fil-C already refuses the memory errors that the stack protector and
# FORTIFY answer, so those two stay only as defence in depth. Two flags
# still carry their own weight here: the automatic zero initialisation
# makes an uninitialised read deterministic, which is a correctness
# property, and the trapping sanitiser stops signed overflow and bad
# shifts, which memory safety says nothing about.
HARDEN = -D_FORTIFY_SOURCE=3 -fstack-protector-strong \
	 -fstack-clash-protection -fcf-protection=full \
	 -ftrivial-auto-var-init=zero -fPIE -pie \
	 -Wl,-z,relro,-z,now -Wl,-z,noexecstack

# This Fil-C distribution ships no gold plugin, so link-time
# optimisation goes through lld. The floating point here feeds pixel
# values and font metrics, which both round to integers with a wide
# margin, so the reassociation that -ffast-math permits changes no
# result: a build with these flags and a build at -O0 write the same
# bytes into every asset.
#
# With -march=native, lld reports the host features that the Fil-C
# target does not carry, such as avx10 and amx. Those lines are
# advisory and the link completes.
OPTIMISE = -O3 -ffast-math -fno-finite-math-only -march=native \
	   -mprefer-vector-width=256 -fno-semantic-interposition \
	   -falign-functions=64 -flto -fuse-ld=lld

CFLAGS ?= $(HARDEN) $(OPTIMISE) -Wall -Wextra
LDLIBS = -lm

PROGRAMS = stamps blinkies badges codes localise check outline

# --- BUILD ------------------------------------------------------------
all: $(PROGRAMS)

stamps: stamps.c img.c img.h
blinkies: blinkies.c img.c img.h
badges: badges.c text.c img.c text.h img.h leaguemono.h
codes: codes.c qr.c img.c qr.h img.h
localise: localise.c
check: check.c
outline: outline.c

%: %.c
	$(CC) $(CFLAGS) -I. -o $@ $(filter %.c,$^) $(LDLIBS)

# --- GENERATION -------------------------------------------------------
# Each program resolves its paths from the repository root, so every
# recipe below runs there. The stamps come first: the animation reads
# the faces that the stamp program writes.
assets: stamps blinkies badges codes
	cd .. && tools/stamps && tools/blinkies && tools/badges && tools/codes

pages: localise
	cd .. && tools/localise

verify: check
	cd .. && tools/check

# The badge letterforms. `leaguemono.h` is committed, so this runs by
# hand, when the face changes. It needs the font file, which the
# repository does not carry.
font: outline
	@test -n "$(FONT)" || { echo "name a face: make font FONT=/path/LeagueMono-NarrowBold.ttf"; exit 1; }
	./outline "$(FONT)" > leaguemono.h

# --- ANALYSIS ---------------------------------------------------------
# Four readers look at the same sources, and each one sees what the
# others miss. The compiler catches the shape of a statement, the GCC
# analyser walks the paths through a function, clang-tidy holds the
# checks that `.clang-tidy` selects, and cppcheck reads the whole set
# at once, which is what lets it name a function that nobody calls.
#
# These use the system toolchain rather than Fil-C, because they read
# the sources and build nothing. Fil-C answers a different question:
# it watches the programs while they run.
#
# The cppcheck suppressions below name reports that a bound elsewhere
# already holds. A glyph carries at least one point, a symbol at least
# one codeword, and the QR mask loop always takes its first grid, so
# the arrays those three reports name fill before anything reads them.
# The remaining two are shapes: an accumulator that starts at zero,
# and two locals that could sit one block deeper.
CPPCHECK_SUPPRESS = --suppress=missingIncludeSystem \
		    --suppress=normalCheckLevelMaxBranches \
		    --suppress=uninitvar:outline.c \
		    --suppress=uninitvar:qr.c \
		    --suppress=negativeIndex:qr.c \
		    --suppress=badBitmaskCheck:qr.c \
		    --suppress=variableScope:qr.c

# Add -Wanalyzer-too-complex and -Wanalyzer-symbol-too-complex here to
# see where the GCC analyser gave up. They report coverage rather than
# faults, and they run to hundreds of kilobytes, so they stay off.
ANALYSER ?=

LINT_CC ?= clang
LINT_WARN = -std=c17 -Wall -Wextra -Wpedantic -Wshadow \
	    -Wstrict-prototypes -Wmissing-prototypes -Wundef \
	    -Wformat=2 -Wcast-qual -Wwrite-strings -Wdouble-promotion \
	    -Wunreachable-code-aggressive -Wvla -Wswitch-enum

lint:
	@echo "== compiler warnings =="
	@$(LINT_CC) $(LINT_WARN) -I. -fsyntax-only *.c
	@echo "== GCC analyser =="
	@for f in *.c; do \
		gcc -std=c17 -Wall -Wextra -fanalyzer $(ANALYSER) \
		    -I. -c -o /dev/null $$f || exit 1; \
	done
	@echo "== clang-tidy =="
	@clang-tidy --quiet *.c -- -std=c17 -I.
	@echo "== cppcheck =="
	@cppcheck --enable=warning,style,performance,portability,unusedFunction \
		--inconclusive --std=c17 --language=c -I. --quiet \
		--error-exitcode=1 $(CPPCHECK_SUPPRESS) .
	@echo "== all four clean =="

clean:
	rm -f $(PROGRAMS)

.PHONY: all assets pages verify font lint clean
