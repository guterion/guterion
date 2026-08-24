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
 * in length. Widening the quiet zone brings them all to one width, so
 * the row reads evenly and no browser has to resample anything: the
 * quiet zone is white margin the standard already requires, and more
 * of it costs a reader nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "img.h"
#include "qr.h"

#define SCALE 4          /* Pixels for each module. */
#define WIDTH 53         /* Modules across, symbol and quiet zone. */

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
		int quiet, side = WIDTH * SCALE;

		if (!q) {
			fprintf(stderr, "codes: %s needs a version this "
					"encoder does not write\n",
				CODES[i].name);
			return 1;
		}
		if (q->size > WIDTH - 8) {
			fprintf(stderr, "codes: %s spans %d modules, which "
					"leaves no quiet zone at %d\n",
				CODES[i].name, q->size, WIDTH);
			return 1;
		}
		quiet = (WIDTH - q->size) / 2;

		im = img_new(side, side);
		if (!im)
			return 1;
		img_fill(im, 0, 0, side - 1, side - 1, 0xffffff);
		for (int y = 0; y < q->size; y++)
			for (int x = 0; x < q->size; x++) {
				int px, py;

				if (!q->m[y * q->size + x])
					continue;
				px = (x + quiet) * SCALE;
				py = (y + quiet) * SCALE;
				img_fill(im, px, py, px + SCALE - 1,
					 py + SCALE - 1, 0x000000);
			}

		snprintf(path, sizeof path, "assets/qr/%s.png", CODES[i].name);
		if (png_write(path, im)) {
			fprintf(stderr, "codes: cannot write %s\n", path);
			return 1;
		}
		printf("  %-12s version %d, %dx%d px\n", CODES[i].name,
		       (q->size - 17) / 4, side, side);
		img_free(im);
		qr_free(q);
	}
	return 0;
}
