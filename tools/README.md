<!--
tools/README.md
@guterion
CC-BY-SA-4.0
How to rebuild the assets and the translated pages
-->

# Tools

These programs write everything in `assets/` and the pages under
`i18n/`. They are C, and they build with Fil-C, which gives them memory
safety while they run: a pointer carries its bounds, so an overrun
traps. `tools/Makefile` expects Fil-C at
`~/opt/filc-0.683-linux-x86_64`; set `FILC` to use another one.

```sh
make -C tools           # build the five programs
make -C tools assets    # stamps, then the animation, then the badges
make -C tools pages     # i18n/{en,es,la}/ from README.md
make -C tools verify    # links, alt text, box widths
```

Each program resolves its paths from the repository root, and the
recipes above run there. The programs read no font and no image
library: the Terminus glyphs and the DejaVu label widths sit in the
source as tables, and `img.c` carries the PNG reader, the PNG writer,
the Lanczos resampler and the GIF encoder.

## Stamps

`stamps.c` holds the wall as a grid. It gives each stamp a border
colour that no neighbour holds, diagonals included, and it stops if it
cannot. The canton ground comes from the logotype: the program averages
the mark and darkens it. A pale mark averages to grey, so name its
ground in `FIXED`.

`stamps.c` writes `assets/stamps/*.png`, and `blinkies.c` turns those
faces into the animated `*.gif` that the pages carry. The PNG stay out
of the repository, so `make -C tools assets` writes them again each
time.

## Pages

Edit `README.md`. Then run `make -C tools pages`, which writes the
other three pages. A page edited under `i18n/` loses that edit on the
next run.

A new phrase needs an entry in the `ES` and `LA` tables inside
`localise.c`. The program names each entry that the page no longer
holds.

## Badges

`badges.c` carries the width of every label, measured once with
FreeType. DejaVu kerns pairs such as "AY", so a whole label is
measured, never the sum of its characters. A new label needs a new
measurement, and the program says so and stops when one is absent. The
note above `LABELS` holds the command that takes it.

## Marks

`tools/marks/` holds the marks that the badges embed. Most marks are
white silhouettes. The ORCID mark keeps its official colours, at the
minimum size that their display rules permit. `assets/logos/` holds the
logotypes. `LICENCE.md` records the source of each one.
