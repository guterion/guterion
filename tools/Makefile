# tools/Makefile
# @guterion
# CC-BY-SA-4.0
# Build the generators, then rebuild the assets and the pages

# --- TOOLCHAIN --------------------------------------------------------
# Fil-C gives these programs memory safety while they run: every pointer
# carries its bounds, so an overrun traps instead of corrupting the
# heap. Point FILC at another installation to build against a different
# version of it.
FILC ?= $(HOME)/opt/filc-0.683-linux-x86_64
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

PROGRAMS = stamps blinkies badges localise check

# --- BUILD ------------------------------------------------------------
all: $(PROGRAMS)

stamps: stamps.c img.c img.h
blinkies: blinkies.c img.c img.h
badges: badges.c
localise: localise.c
check: check.c

%: %.c
	$(CC) $(CFLAGS) -I. -o $@ $(filter %.c,$^) $(LDLIBS)

# --- GENERATION -------------------------------------------------------
# Each program resolves its paths from the repository root, so every
# recipe below runs there. The stamps come first: the animation reads
# the faces that the stamp program writes.
assets: stamps blinkies badges
	cd .. && tools/stamps && tools/blinkies && tools/badges
	$(MAKE) raster

# The badges also travel as PNG at twice the size, for a mail signature
# and for anywhere else that shows no SVG. Rasterising a badge needs
# DejaVu with antialiasing, which is a font library, so rsvg-convert
# does that step. Run this after `assets`, or the PNG keep the badge
# that the SVG no longer holds.
raster:
	cd ../assets/badges && for f in *.svg; do \
		rsvg-convert -w 198 -h 40 "$$f" -o "email/$${f%.svg}@2x.png"; \
	done

pages: localise
	cd .. && tools/localise

verify: check
	cd .. && tools/check

clean:
	rm -f $(PROGRAMS)

.PHONY: all assets raster pages verify clean
