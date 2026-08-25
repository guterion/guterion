/*
 * tools/img.h
 * @guterion
 * CC-BY-SA-4.0
 * Image primitives for the stamp generators: PNG in and out, GIF out
 *
 * Fil-C cannot link against ordinary C objects, so libpng, zlib and
 * giflib are unavailable however they are installed. These routines
 * carry only what the generators need, and they compile as Fil-C.
 */

#ifndef IMG_H
#define IMG_H

#include <stddef.h>

/* An image in straight, non-premultiplied RGBA, eight bits a channel. */
struct image
{
    int w;
    int h;
    unsigned char* px; /* w * h * 4 bytes */
};

/*
 * The byte offset of pixel (x, y) in an image `w` pixels wide. Each
 * caller holds x and y inside the image, so the conversion is exact.
 * Writing it once keeps the arithmetic out of the expression that
 * indexes the pixel.
 */
static inline size_t img_offset(
    int w,
    int x,
    int y
)
{
    return ((size_t)y * (size_t)w + (size_t)x) * 4;
}

struct image* img_new(
    int w,
    int h
);
void img_free(
    struct image* im
);

/* Read a PNG file. Returns NULL and prints the reason on failure. */
struct image* png_read(
    const char* path
);

/* Write a PNG file. Returns 0 on success. */
int png_write(
    const char* path,
    const struct image* im
);

/*
 * Scale with a Lanczos-3 kernel, which is what keeps a logotype legible
 * once it lands in a canton twenty pixels wide.
 */
struct image* img_scale(
    const struct image* src,
    int w,
    int h
);

/* Composite `top` over `dst` at (x, y), honouring the alpha of `top`. */
void img_over(
    struct image* dst,
    const struct image* top,
    int x,
    int y
);

/* Fill a rectangle, and draw a one-pixel outline. */
void img_fill(
    struct image* im,
    int x0,
    int y0,
    int x1,
    int y1,
    unsigned int rgb
);
void img_rect(
    struct image* im,
    int x0,
    int y0,
    int x1,
    int y1,
    unsigned int rgb
);

/*
 * Write an animated GIF. `frames` holds `count` images of one size, and
 * `delay_cs` is the pause between them in hundredths of a second.
 */
int gif_write(
    const char* path,
    struct image* const* frames,
    int count,
    int delay_cs
);

#endif /* IMG_H */
