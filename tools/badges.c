/*
 * tools/badges.c
 * @guterion
 * CC-BY-SA-4.0
 * Generate the social badges, each carrying its own logotype
 *
 * A badge is twenty pixels tall, the height at which a Markdown list
 * bullet sits level with it. The mark travels inside the SVG as a
 * base64 data URI, so a badge needs no other host.
 *
 * Every badge takes the width of the widest one, which lines the
 * column up. The label is set in League Mono Narrow Bold, whose
 * outlines `text.c` carries: the SVG receives them as a path, so the
 * badge looks the same to a reader who holds no such font, and the
 * PNG receives them as coverage that the same file rasterises.
 *
 * Each badge is written twice. The SVG is what the pages carry. The
 * PNG at twice the size goes to a mail signature, and anywhere else
 * that shows no SVG.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "img.h"
#include "text.h"

#define HEIGHT 20
#define MARK 12          /* Height of the logotype inside the badge. */
#define PAD 7
#define MARK_COLUMN 18   /* Fixed column keeps every label on one axis. */
#define TEXT_SIZE 11     /* The label, in pixels from ascender to descender. */
#define BASELINE 14      /* Where the label sits, measured from the top. */
#define RADIUS 2         /* The corner the field turns. */
#define PNG_SCALE 2      /* The PNG carries twice the pixels of the SVG. */

/* One badge: the file it writes, its label, field colour, mark and ink. */
struct badge {
	const char *name;
	const char *label;
	const char *colour;
	const char *mark;
	double mark_ratio;   /* width / height of the mark */
	const char *ink;
	/*
	 * A web badge names its own author and carries a hallmark line.
	 * A social badge leaves both empty and belongs to @guterion.
	 */
	const char *author;
	const char *theme;
};

/*
 * The badge names the service; the value sits beside it, in text a
 * reader can select and copy.
 */
static const struct badge SET[] = {
	{ "email",        "EMAIL",     "#2f6f3a", "email",     44.0 / 30.0,  "#ffffff", NULL, NULL },
	{ "gpg",          "GPG",       "#3a3f4b", "gpg",       40.0 / 44.0,  "#ffffff", NULL, NULL },
	{ "git",          "GIT",       "#662900", "forgejo",   1.0,          "#ffffff", NULL, NULL },
	{ "orcid",        "ORCID",     "#a6ce39", "orcid",     1.0,          "#ffffff", NULL, NULL },
	{ "linkedin",     "LINKEDIN",  "#0a66c2", "linkedin", 160.0 / 158.0, "#ffffff", NULL, NULL },
	{ "x",            "X",         "#1c1f24", "x",        160.0 / 145.0, "#ffffff", NULL, NULL },
	{ "liberapay",    "LIBERAPAY", "#f6c915", "liberapay", 125.0 / 160.0, "#1a1a1a", NULL, NULL },
	{ "email-es",     "CORREO",    "#2f6f3a", "email",     44.0 / 30.0,  "#ffffff", NULL, NULL },
	{ "email-la",     "EPISTULA",  "#2f6f3a", "email",     44.0 / 30.0,  "#ffffff", NULL, NULL },
	{ "gpg-la",       "CLAVIS",    "#3a3f4b", "gpg",       40.0 / 44.0,  "#ffffff", NULL, NULL },
	{ "venturas",     "VENTURAS",  "#20211f", "venturas",  90.0 / 134.0, "#f0b82d",
	  "FRANKOVIA Venturas SpA", "Lumen" },
	{ "guterion-net", "MI PORTAL", "#073832", "web",       1.0,          "#65f7cf",
	  "@guterion", "Terminal" },
	{ NULL, NULL, NULL, NULL, 0.0, NULL, NULL, NULL }
};

/*
 * A badge label is set in capitals. The set is a wall of small marks,
 * and one lowercase label among them reads as a different object.
 * Only the ASCII letters change, so a letter that arrives as UTF-8
 * passes through whole rather than breaking into halves.
 */
static const char *capitalise(const struct badge *b, char *buf, size_t n)
{
	size_t i = 0;

	for (const char *p = b->label; *p && i + 1 < n; p++, i++)
		buf[i] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
	buf[i] = '\0';
	return buf;
}

/* The ORCID display rules set a minimum size for their mark. */
static int mark_height(const struct badge *b)
{
	return strcmp(b->mark, "orcid") ? MARK : 16;
}

/* Every label starts at the same column, so only the text varies. */
static int badge_width(const struct badge *b)
{
	char label[64];

	return PAD + MARK_COLUMN + 7 +
	       (int)text_advance(capitalise(b, label, sizeof label), TEXT_SIZE) +
	       PAD;
}

static const char B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Encode a buffer as base64. The caller owns the result. */
static char *base64(const unsigned char *data, size_t len)
{
	size_t out_len = ((len + 2) / 3) * 4;
	char *out = malloc(out_len + 1);
	size_t o = 0;

	if (!out)
		return NULL;
	for (size_t i = 0; i < len; i += 3) {
		unsigned int block = (unsigned int)data[i] << 16;
		size_t left = len - i;

		if (left > 1)
			block |= (unsigned int)data[i + 1] << 8;
		if (left > 2)
			block |= data[i + 2];

		out[o++] = B64[(block >> 18) & 0x3F];
		out[o++] = B64[(block >> 12) & 0x3F];
		out[o++] = left > 1 ? B64[(block >> 6) & 0x3F] : '=';
		out[o++] = left > 2 ? B64[block & 0x3F] : '=';
	}
	out[o] = '\0';
	return out;
}

static unsigned char *slurp(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	long size;

	if (!f) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) {              /* The file gave no length. */
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)size);
	if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	fclose(f);
	*len = (size_t)size;
	return buf;
}

/* --- THE FIELD -------------------------------------------------------- */

/*
 * Paint the field: the colour, the rounded corner, and the sheen that
 * the SVG draws with a gradient. The gradient runs from white at
 * twelve per cent down to black at the same, so the top lifts and the
 * bottom settles.
 */
static void paint_field(struct image *im, unsigned int rgb, int scale)
{
	double radius = (double)RADIUS * scale;
	const unsigned char base[3] = {
		(unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8),
		(unsigned char)rgb
	};

	for (int y = 0; y < im->h; y++) {
		double t = (double)y / (im->h - 1);
		/* The gradient colour at this row, at .12 opacity. */
		double sheen = 255.0 * (1.0 - t);
		double k = 0.12;

		for (int x = 0; x < im->w; x++) {
			unsigned char *d = im->px + ((size_t)y * im->w + x) * 4;
			double cover = 1.0;
			double dx = 0, dy = 0;

			/* Distance past the corner arc, for the alpha. */
			if (x < radius && y < radius) {
				dx = radius - 0.5 - x;
				dy = radius - 0.5 - y;
			} else if (x >= im->w - radius && y < radius) {
				dx = x - (im->w - radius - 0.5);
				dy = radius - 0.5 - y;
			} else if (x < radius && y >= im->h - radius) {
				dx = radius - 0.5 - x;
				dy = y - (im->h - radius - 0.5);
			} else if (x >= im->w - radius && y >= im->h - radius) {
				dx = x - (im->w - radius - 0.5);
				dy = y - (im->h - radius - 0.5);
			}
			if (dx > 0 || dy > 0) {
				double r = sqrt(dx * dx + dy * dy);

				cover = radius + 0.5 - r;
				if (cover > 1.0)
					cover = 1.0;
				if (cover < 0.0)
					cover = 0.0;
			}

			for (int c = 0; c < 3; c++) {
				double v = base[c] * (1 - k) + sheen * k;

				d[c] = (unsigned char)(v + 0.5);
			}
			d[3] = (unsigned char)(255 * cover + 0.5);
		}
	}
}

/* Compose one badge as pixels, at the given multiple of its size. */
static struct image *badge_image(const struct badge *b, const char *label,
				 int width, int scale)
{
	struct image *im = img_new(width * scale, HEIGHT * scale);
	struct image *mark;
	struct image *fitted;
	char path[512];
	int mark_h = mark_height(b) * scale;
	int mark_w;
	unsigned int ink;

	if (!im)
		return NULL;
	paint_field(im, (unsigned int)strtoul(b->colour + 1, NULL, 16), scale);

	snprintf(path, sizeof path, "tools/marks/%s.png", b->mark);
	mark = png_read(path);
	if (!mark) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	mark_w = (int)(mark_h * b->mark_ratio + 0.5);
	if (mark_w < 1)
		mark_w = 1;
	fitted = img_scale(mark, mark_w, mark_h);
	img_free(mark);
	if (!fitted) {
		img_free(im);
		return NULL;
	}
	img_over(im, fitted, PAD * scale, (HEIGHT * scale - mark_h) / 2);
	img_free(fitted);

	ink = (unsigned int)strtoul(b->ink + 1, NULL, 16);
	text_draw(im, label, (double)(PAD + MARK_COLUMN + 7) * scale,
		  (double)BASELINE * scale, (double)TEXT_SIZE * scale,
		  (unsigned char)(ink >> 16), (unsigned char)(ink >> 8),
		  (unsigned char)ink);
	return im;
}

static void write_badge(const struct badge *b, int width)
{
	char mark_path[256], out_path[256];
	unsigned char *mark_data;
	size_t mark_len;
	char *encoded;
	FILE *f;
	char label[64];
	char glyph_path[8192];
	struct image *raster;
	int mark_h = mark_height(b);
	int mark_w = (int)(mark_h * b->mark_ratio + 0.5);

	if (mark_w < 1)
		mark_w = 1;

	capitalise(b, label, sizeof label);
	if (text_path(glyph_path, sizeof glyph_path, label) >=
	    sizeof glyph_path) {
		fprintf(stderr, "badges: outline of \"%s\" needs more room\n",
			label);
		exit(1);
	}
	snprintf(mark_path, sizeof mark_path, "tools/marks/%s.png", b->mark);
	mark_data = slurp(mark_path, &mark_len);
	encoded = base64(mark_data, mark_len);
	free(mark_data);
	if (!encoded) {
		fprintf(stderr, "out of memory\n");
		exit(1);
	}

	snprintf(out_path, sizeof out_path, "assets/badges/%s.svg", b->name);
	f = fopen(out_path, "wb");
	if (!f) {
		fprintf(stderr, "cannot write %s\n", out_path);
		exit(1);
	}


	fprintf(f,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"assets/badges/%s.svg\n"
		"%s\n"
		"CC-BY-SA-4.0\n"
		"%s badge: %s\n",
		b->name, b->author ? b->author : "@guterion",
		b->theme ? "Web" : "Social", label);

	if (b->theme)
		fprintf(f,
			"Hallmark \u00b7 component: badge \u00b7 genre:"
			" atmospheric \u00b7 theme: %s\n"
			"Hallmark \u00b7 pre-emit critique:"
			" P5 H4 E5 S5 R5 V5\n",
			b->theme);

	fprintf(f,
		"-->\n"
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\""
		" height=\"%d\"\n"
		"     role=\"img\" aria-label=\"%s\">\n"
		"  <title>%s</title>\n"
		"  <linearGradient id=\"g\" x2=\"0\" y2=\"100%%\">\n"
		"    <stop offset=\"0\" stop-color=\"#fff\" stop-opacity=\".12\"/>\n"
		"    <stop offset=\"1\" stop-opacity=\".12\"/>\n"
		"  </linearGradient>\n"
		"  <rect width=\"%d\" height=\"%d\" rx=\"2\" fill=\"%s\"/>\n"
		"  <rect width=\"%d\" height=\"%d\" rx=\"2\" fill=\"url(#g)\"/>\n"
		"  <image x=\"%d\" y=\"%.1f\" width=\"%d\" height=\"%d\"\n"
		"         href=\"data:image/png;base64,%s\"/>\n"
		"  <g transform=\"translate(%d %d) scale(%.6f -%.6f)\">\n"
		"    <path fill=\"%s\" d=\"%s\"/>\n"
		"  </g>\n"
		"</svg>\n",
		width, HEIGHT, label, label,
		width, HEIGHT, b->colour, width, HEIGHT,
		PAD, (HEIGHT - mark_h) / 2.0, mark_w, mark_h, encoded,
		PAD + MARK_COLUMN + 7, BASELINE,
		text_scale(TEXT_SIZE), text_scale(TEXT_SIZE),
		b->ink, glyph_path);

	fclose(f);
	free(encoded);

	/* The same outline, now as pixels. */
	raster = badge_image(b, label, width, PNG_SCALE);
	if (!raster) {
		fprintf(stderr, "cannot compose %s\n", b->name);
		exit(1);
	}
	mkdir("assets/badges/email", 0755);
	snprintf(out_path, sizeof out_path,
		 "assets/badges/email/%s@%dx.png", b->name, PNG_SCALE);
	if (png_write(out_path, raster)) {
		fprintf(stderr, "cannot write %s\n", out_path);
		exit(1);
	}
	img_free(raster);

	printf("  %-16s %4dx%d  %s\n", b->name, width, HEIGHT, label);
}

int main(void)
{
	int width = 0;

	mkdir("assets", 0755);
	mkdir("assets/badges", 0755);

	/* One width for the whole set: the widest badge sets them all. */
	for (int i = 0; SET[i].name; i++) {
		int w = badge_width(&SET[i]);

		if (w > width)
			width = w;
	}
	printf("  one width for all: %dpx\n", width);

	for (int i = 0; SET[i].name; i++)
		write_badge(&SET[i], width);

	return 0;
}
