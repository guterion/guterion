# tools/Makefile
# @guterion
# CC-BY-SA-4.0
# Build the generators, then rebuild the assets and the pages.

# --- TOOLCHAIN --------------------------------------------------------
# Fil-C gives these programs memory safety while they run. Every pointer
# carries its bounds, so an overrun traps instead of corrupting the
# heap. Point FILC at another installation to build against a different
# version of it.
FILC ?= $(HOME)/.local/opt/filc-0.683-linux-x86_64
CC = $(FILC)/build/bin/filcc

# --- FLAGS ------------------------------------------------------------
# The safety envelope comes first, and the optimisation fits inside it.
# Fil-C already refuses the memory errors that the stack protector and
# FORTIFY answer, so those two stay only as defence in depth. Two flags
# still carry their own weight here. The automatic zero initialisation
# makes an uninitialised read deterministic, which is a correctness
# property. The trapping sanitiser stops signed overflow and bad
# shifts, which memory safety says nothing about.
#
# The undefine comes first because the Gentoo Hardened toolchain on
# this machine already defines the macro, and a redefinition without it
# is a hard error rather than a warning.
HARDEN = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	 -fstack-protector-strong \
	 -fstack-clash-protection -fcf-protection=full \
	 -ftrivial-auto-var-init=zero -fPIE -pie \
	 -Wl,-z,relro,-z,now -Wl,-z,noexecstack

# The floating point keeps the standard semantics. -ffp-contract=on
# permits a fused multiply-add inside one expression, where C permits
# one, and refuses to fuse across statements. The pixel values and the
# font metrics both round to integers with a wide margin, so the
# reassociation that -ffast-math would add buys nothing measurable and
# costs the guarantee.
#
# The assets in the repository come from this build. Another compiler
# at another level writes slightly different bytes, because the fused
# multiply-add that -ffp-contract=on permits lands in different places.
# The sanitiser build below moves a stamp by a byte or two of its
# compression. The images look the same and their pixels round the
# same, and only the exact file differs. Regenerate with this profile
# whenever an asset is to be committed.
#
# This Fil-C distribution ships no gold plugin, so link-time
# optimisation goes through lld. Both flags are validated on this
# version: the seven programs build, run, and write the assets that the
# repository carries.
#
# With -march=native, lld reports the host features that the Fil-C
# target does not carry, such as avx10 and amx. Those lines are
# advisory and the link completes.
OPTIMISE = -O3 -fno-fast-math -ffp-contract=on \
	   -march=native -mtune=native \
	   -mprefer-vector-width=256 -fno-semantic-interposition \
	   -flto -fuse-ld=lld

CFLAGS ?= -std=c17 $(HARDEN) $(OPTIMISE) -Wall -Wextra
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
# recipe below runs there. The stamps come first, because the
# animation reads the faces that the stamp program writes.
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

# --- SANITIZER --------------------------------------------------------
# Fil-C answers memory safety, and it says nothing about signed
# overflow, a bad shift or a data race. The trapping sanitiser closes
# the first two. It aborts at the point of the fault rather than
# carrying a wrong number forward. The run below is the whole
# generation, which is the only workload these programs have.
#
# That run writes the assets with a different compiler at a different
# level, which moves a few bytes of image compression, so the recipe
# ends by rebuilding the release and generating them again. The tree
# it leaves behind is the tree it started from.
SANITISE = -std=c17 -O1 -g -fno-omit-frame-pointer \
	   -fsanitize=undefined -fsanitize-trap=undefined

sanitize:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory CC=clang CFLAGS="$(SANITISE)" all
	@cd .. && tools/stamps > /dev/null && tools/blinkies > /dev/null \
		&& tools/badges > /dev/null && tools/codes > /dev/null \
		&& tools/localise > /dev/null && tools/check
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory all
	@$(MAKE) --no-print-directory assets > /dev/null
	@echo "no undefined behaviour on the generation run"

# --- FORMAT -----------------------------------------------------------
# Two tools produce the house style, and the order is fixed.
# clang-format does the primary work. uncrustify then opens the short
# signature that clang-format leaves on one line, because clang-format
# breaks a bracket only when the content overflows the column limit.
#
# The pipeline runs twice. From a foreign style it needs two passes to
# reach its fixed point. uncrustify opens the brace of an array
# initialiser as though it were a function, and clang-format closes it
# again on the following pass. From the fixed point the second run
# changes nothing.
SOURCES = badges.c blinkies.c check.c codes.c img.c localise.c \
	  outline.c qr.c stamps.c text.c
HEADERS = img.h qr.h text.h leaguemono.h

format:
	@clang-format -i $(SOURCES) $(HEADERS)
	@uncrustify -c uncrustify.cfg --no-backup -q $(SOURCES) $(HEADERS)
	@clang-format -i $(SOURCES) $(HEADERS)
	@uncrustify -c uncrustify.cfg --no-backup -q $(SOURCES) $(HEADERS)
	@echo "formatted"

# The check runs the same passes to standard output and compares the
# result against the file on disk. `clang-format --dry-run --Werror`
# reports every vertical short signature, so it measures the first pass
# rather than the house style.
format-check:
	@for f in $(SOURCES) $(HEADERS); do \
		clang-format "$$f" | uncrustify -c uncrustify.cfg -q -l C \
			| diff -q "$$f" - > /dev/null \
			|| { echo "unformatted: $$f"; exit 1; }; \
	done
	@echo "format clean"

# --- ANALYSIS ---------------------------------------------------------
# Five readers look at the same sources, and each one sees what the
# others miss. The compiler catches the shape of a statement, the GCC
# analyser walks the paths through a function, clang-tidy holds the
# checks that `.clang-tidy` selects, cppcheck reads the whole set at
# once, which is what lets it name a function that nobody calls, and
# include-what-you-use asks why each #include is there.
#
# These use the system toolchain rather than Fil-C, because they read
# the sources and build nothing. Fil-C answers a different question.
# It watches the programs while they run.
#
# The cppcheck suppressions below name reports that a bound elsewhere
# already holds. A glyph carries at least one point, a symbol at least
# one codeword, and the QR mask loop always takes its first grid, so
# the arrays those three reports name fill before anything reads them.
# The remaining two name shapes rather than faults, an accumulator that
# starts at zero and two locals that could sit one block deeper.
# constParameterPointer asks for const on the four functions that paint
# into an image. They modify no field of the structure, only the buffer
# that px points at, so the request is correct by the letter. The const
# would then say that img_fill leaves the image alone while it fills
# it, so the signature keeps the type that matches the work.
CPPCHECK_SUPPRESS = --suppress=constParameterPointer \
		    --suppress=missingIncludeSystem \
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

# The warning set of the c-style skill. -Wconversion and
# -Wsign-conversion carry their weight. The tree answered 41 of them,
# each one by repairing a type rather than by adding a cast.
LINT_WARN = -std=c17 -Wall -Wextra -Wpedantic \
	    -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
	    -Wconversion -Wsign-conversion -Wshadow -Wcast-qual -Wcast-align \
	    -Wwrite-strings -Wpointer-arith -Wredundant-decls -Wvla \
	    -Wdouble-promotion -Wformat=2 -Wnull-dereference -Wundef \
	    -Werror

lint: format-check
	@echo "== clang warnings =="
	@clang $(LINT_WARN) -I. -fsyntax-only $(SOURCES)
	@echo "== gcc warnings =="
	@gcc $(LINT_WARN) -I. -fsyntax-only $(SOURCES)
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
	@echo "== include-what-you-use =="
	@out=$$(for f in $(SOURCES) $(HEADERS); do \
		include-what-you-use -Xiwyu --no_fwd_decls -std=c17 -I. \
			-x c "$$f" 2>&1 \
			| sed -n '/should add these lines/,/^---/p' \
			| grep -E '^[+-]#include|^- #include' || true; \
	done); \
	if [ -n "$$out" ]; then echo "$$out"; exit 1; fi
	@echo "== all five clean =="

clean:
	rm -f $(PROGRAMS)

.PHONY: all assets pages verify font format format-check lint sanitize clean
