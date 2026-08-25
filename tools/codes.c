/*
 * tools/codes.c
 * @guterion
 * CC-BY-SA-4.0
 * Draw the QR code that carries each payment address
 *
 * The addresses live here rather than in the page, so the code and the
 * text below it come from one source and cannot drift apart.
 *
 * Each symbol takes a different version, because the addresses differ
 * in length. Two things have to hold at once: the canvas must be one
 * size, or the page scales each image by a different factor and the
 * symbols come out uneven again, and the symbols themselves must be
 * near enough in size that the row reads level.
 *
 * So each symbol takes the scale that brings it closest to the target
 * width, and the margin takes whatever is left over. The margin is
 * measured in pixels rather than in modules, which lets the canvas
 * stay fixed; the check below keeps it at the four modules that the
 * standard asks for.
 */

#include <stdio.h>
#include <sys/stat.h>

#include "img.h"
#include "qr.h"

#define TARGET 148       /* The width each symbol aims for, in pixels. */
#define CANVAS 198       /* One size for every image, margin included. */
#define QUIET 4          /* The least white margin, counted in modules. */

static const struct {
	const char *name;
	const char *text;
} CODES[] = {
	{ "monero",
	  "48okbGH4M5HNhaSEyVTYqAQu8cLK6D9PmAWKRwCeyYx9RsFcbpjhnP5WaYoQ"
	  "YPwPNWS49xwqHsPrfP8U5zKmFoUYTrF6NNL" },
	{ "lightning", "guterion@cake.cash" },
	{ "ethereum", "0xe9D6A00754FF22B092A7e9bC1b397517c2281291" },
	{ NULL, NULL }
};

int main(void)
{
	mkdir("assets", 0755);
	mkdir("assets/qr", 0755);

	for (int i = 0; CODES[i].name; i++) {
		struct qr *q = qr_make(CODES[i].text);
		struct image *im;
		char path[128];
		int scale, side, margin;

		if (!q) {
			fprintf(stderr, "codes: %s needs a version this "
					"encoder does not write\n",
				CODES[i].name);
			return 1;
		}
		/* The scale that puts this symbol nearest the target. */
		scale = (TARGET + q->size / 2) / q->size;
		if (scale < 1)
			scale = 1;
		side = q->size * scale;
		margin = (CANVAS - side) / 2;
		if (margin < QUIET * scale) {
			fprintf(stderr, "codes: %s leaves %d px of margin, "
					"under the %d that four modules need\n",
				CODES[i].name, margin, QUIET * scale);
			return 1;
		}

		im = img_new(CANVAS, CANVAS);
		if (!im)
			return 1;
		img_fill(im, 0, 0, CANVAS - 1, CANVAS - 1, 0xffffff);
		for (int y = 0; y < q->size; y++)
			for (int x = 0; x < q->size; x++) {
				int px, py;

				if (!q->m[y * q->size + x])
					continue;
				px = margin + x * scale;
				py = margin + y * scale;
				img_fill(im, px, py, px + scale - 1,
					 py + scale - 1, 0x000000);
			}

		snprintf(path, sizeof path, "assets/qr/%s.png", CODES[i].name);
		if (png_write(path, im)) {
			fprintf(stderr, "codes: cannot write %s\n", path);
			return 1;
		}
		printf("  %-12s version %d, symbol %d px, margin %d px\n",
		       CODES[i].name, (q->size - 17) / 4, side, margin);
		img_free(im);
		qr_free(q);
	}
	return 0;
}
