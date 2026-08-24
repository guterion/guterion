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
 * in length. What the eye compares across the row is the size of the
 * symbol itself, so each one takes the scale that brings it closest to
 * one width. The canvas around it then differs a little, which reads
 * evenly, where a fixed canvas would leave the short address as a
 * small symbol floating in white.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "img.h"
#include "qr.h"

#define TARGET 148       /* The width each symbol aims for, in pixels. */
#define QUIET 4          /* Modules of white margin, as the standard asks. */

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
		int scale, side;

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
		side = (q->size + QUIET * 2) * scale;

		im = img_new(side, side);
		if (!im)
			return 1;
		img_fill(im, 0, 0, side - 1, side - 1, 0xffffff);
		for (int y = 0; y < q->size; y++)
			for (int x = 0; x < q->size; x++) {
				int px, py;

				if (!q->m[y * q->size + x])
					continue;
				px = (x + QUIET) * scale;
				py = (y + QUIET) * scale;
				img_fill(im, px, py, px + scale - 1,
					 py + scale - 1, 0x000000);
			}

		snprintf(path, sizeof path, "assets/qr/%s.png", CODES[i].name);
		if (png_write(path, im)) {
			fprintf(stderr, "codes: cannot write %s\n", path);
			return 1;
		}
		printf("  %-12s version %d, symbol %d px, canvas %d px\n",
		       CODES[i].name, (q->size - 17) / 4,
		       q->size * scale, side);
		img_free(im);
		qr_free(q);
	}
	return 0;
}
