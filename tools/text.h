/*
 * tools/text.h
 * @guterion
 * CC-BY-SA-4.0
 * Set a label in League Mono Narrow Bold, as an SVG path or as pixels
 */

#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>

#include "img.h"

/*
 * The factor that carries a path in font units to pixels. The SVG
 * places the path with a transform, so it needs this number rather
 * than the em that produced it.
 */
double text_scale(
    double size
);

/* The width that a label occupies, in pixels, at the given size. */
double text_advance(
    const char* s,
    double size
);

/*
 * Write the label as SVG path data, in font units, with the pen at the
 * origin and y rising. The caller places and scales it with a
 * transform, which keeps every coordinate a short integer. Returns the
 * number of bytes the path needs, which may exceed n; the caller then
 * knows that the buffer was short.
 */
size_t text_path(
    char* buf,
    size_t n,
    const char* s
);

/*
 * Draw the label into the image, in the given colour, antialiased.
 * The pen starts at (x, y), where y is the baseline.
 */
void text_draw(
    struct image* im,
    const char* s,
    double x,
    double y,
    double size,
    unsigned char r,
    unsigned char g,
    unsigned char b
);

#endif
