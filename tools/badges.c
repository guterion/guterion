/*
 * tools/badges.c
 * @guterion
 * CC-BY-SA-4.0
 * Generate the social badges, each carrying its own logotype
 *
 *     $FILC/build/bin/clang -g -O -o tools/badges tools/badges.c
 *
 * A badge is twenty pixels tall, the height at which a Markdown list
 * bullet sits level with it. The mark travels inside the SVG as a
 * base64 data URI, so a badge needs no other host.
 *
 * Every badge takes the width of the widest one, which lines the column
 * up. The advance widths below were measured once with FreeType; the C
 * needs no font library at run time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define HEIGHT 20
#define MARK 12          /* Height of the logotype inside the badge. */
#define PAD 7
#define MARK_COLUMN 18   /* Fixed column keeps every label on one axis. */

/*
 * The width of each label in DejaVu Sans Bold at 10px, measured once
 * with FreeType. A whole label is measured, never the sum of its
 * characters, because DejaVu kerns pairs such as "AY" and "PA": the
 * sum overstates LIBERAPAY by nearly two pixels.
 *
 * A new label needs a new measurement. Take it with:
 *
 *     python3 -c 'from PIL import ImageFont; \
 *         f = ImageFont.truetype("/usr/share/fonts/dejavu/\
 * DejaVuSans-Bold.ttf", 10); print(f.getlength("LABEL"))'
 */
static const struct { const char *label; float w; } LABELS[] = {
	{ "EMAIL",       34.6094f },
	{ "GPG",         23.7344f },
	{ "GIT",         18.7500f },
	{ "ORCID",       35.5625f },
	{ "LINKEDIN",    53.4375f },
	{ "X",           7.7031f },
	{ "LIBERAPAY",   60.4062f },
	{ "CORREO",      46.5781f },
	{ "EPISTULA",    54.1406f },
	{ "CLAVIS",      39.4375f },
	{ "VENTURAS",    60.5312f },
	{ "MI PORTAL",   60.3906f },
	{ NULL, 0.0f }
};

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

/* The ORCID display rules set a minimum size for their mark. */
static int mark_height(const struct badge *b)
{
	return strcmp(b->mark, "orcid") ? MARK : 16;
}

/* Look the label up. An unmeasured label stops the run. */
static double text_width(const char *label)
{
	for (int i = 0; LABELS[i].label; i++)
		if (!strcmp(LABELS[i].label, label))
			return LABELS[i].w;

	fprintf(stderr, "badges: no measurement for \"%s\"; "
			"see the note above LABELS\n", label);
	exit(1);
}

/* Every label starts at the same column, so only the text varies. */
static int badge_width(const struct badge *b)
{
	return PAD + MARK_COLUMN + 7 + (int)text_width(b->label) + PAD;
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
	buf = malloc((size_t)size);
	if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	fclose(f);
	*len = (size_t)size;
	return buf;
}

static void write_badge(const struct badge *b, int width)
{
	char mark_path[256], out_path[256];
	unsigned char *mark_data;
	size_t mark_len;
	char *encoded;
	FILE *f;
	int mark_h = mark_height(b);
	int mark_w = (int)(mark_h * b->mark_ratio + 0.5);

	if (mark_w < 1)
		mark_w = 1;

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
		b->theme ? "Web" : "Social", b->label);

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
		"  <text x=\"%d\" y=\"14\" fill=\"%s\" font-size=\"10\"\n"
		"        font-family=\"DejaVu Sans,Verdana,Geneva,sans-serif\"\n"
		"        font-weight=\"bold\">%s</text>\n"
		"</svg>\n",
		width, HEIGHT, b->label, b->label,
		width, HEIGHT, b->colour, width, HEIGHT,
		PAD, (HEIGHT - mark_h) / 2.0, mark_w, mark_h, encoded,
		PAD + MARK_COLUMN + 7, b->ink, b->label);

	fclose(f);
	free(encoded);
	printf("  %-16s %4dx%d  %s\n", b->name, width, HEIGHT, b->label);
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
