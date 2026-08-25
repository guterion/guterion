/*
 * tools/stamps.c
 * @guterion
 * CC-BY-SA-4.0
 * Generate the 99x56 stamps and colour the wall like a graph
 *
 *     $FILC/build/bin/clang -g -O -o tools/stamps \
 *         tools/stamps.c tools/img.c -I tools -lm
 *
 * Each stamp carries a canton on the left holding the original
 * logotype, a rule, and the caption on the right.
 *
 * The canton ground comes from the logotype itself: theprogram averages the
 * mark's own colour and darkens it to a ground, so a stamp is
 * recognisable from across the page. A mark that is pale or monochrome
 * averages to grey, so those stamps name their ground by hand in FIXED.
 *
 * The wall is a three by seven grid, and two stamps that touch —
 * including on the diagonal — take different border colours and
 * different canton grounds. That is the eight-neighbour colouring of a
 * king graph. The program verifies the result and stops rather than
 * writing a clash.
 *
 * Colours come from Phosphor Base24 (github.com/guterion/phosphor).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "img.h"

#define W            99
#define H            56
#define CANTON       28   /* One width for every stamp. */
#define CELL         6    /* Terminus 12 advances six pixels. */
#define BOX          20   /* The mark never reaches the border. */
#define GROUND_LEVEL 0.30 /* How far the logotype colour drops. */

/*
 * The Terminus 12 glyphs, taken once from the console font. Each glyph
 * is a column of bytes, one bit a pixel, most significant bit first,
 * in a cell that measures 6x12.
 */

#define GLYPH12_W 6
#define GLYPH12_H 12

/* clang-format off */
static const struct { unsigned int cp; unsigned char rows[12]; }
GLYPHS12[] = {
	{ 32, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
	{ 33, { 0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x20,0x00,0x00 } },
	{ 40, { 0x00,0x00,0x10,0x20,0x40,0x40,0x40,0x40,0x20,0x10,0x00,0x00 } },
	{ 41, { 0x00,0x00,0x40,0x20,0x10,0x10,0x10,0x10,0x20,0x40,0x00,0x00 } },
	{ 43, { 0x00,0x00,0x00,0x00,0x20,0x20,0xF8,0x20,0x20,0x00,0x00,0x00 } },
	{ 45, { 0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00,0x00 } },
	{ 46, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x00,0x00 } },
	{ 47, { 0x00,0x00,0x08,0x08,0x10,0x10,0x20,0x20,0x40,0x40,0x00,0x00 } },
	{ 48, { 0x00,0x00,0x70,0x88,0x98,0xA8,0xC8,0x88,0x88,0x70,0x00,0x00 } },
	{ 49, { 0x00,0x00,0x20,0x60,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00 } },
	{ 50, { 0x00,0x00,0x70,0x88,0x88,0x08,0x10,0x20,0x40,0xF8,0x00,0x00 } },
	{ 51, { 0x00,0x00,0x70,0x88,0x08,0x30,0x08,0x08,0x88,0x70,0x00,0x00 } },
	{ 52, { 0x00,0x00,0x08,0x18,0x28,0x48,0x88,0xF8,0x08,0x08,0x00,0x00 } },
	{ 53, { 0x00,0x00,0xF8,0x80,0x80,0xF0,0x08,0x08,0x88,0x70,0x00,0x00 } },
	{ 54, { 0x00,0x00,0x70,0x80,0x80,0xF0,0x88,0x88,0x88,0x70,0x00,0x00 } },
	{ 55, { 0x00,0x00,0xF8,0x08,0x08,0x10,0x10,0x20,0x20,0x20,0x00,0x00 } },
	{ 56, { 0x00,0x00,0x70,0x88,0x88,0x70,0x88,0x88,0x88,0x70,0x00,0x00 } },
	{ 57, { 0x00,0x00,0x70,0x88,0x88,0x88,0x78,0x08,0x08,0x70,0x00,0x00 } },
	{ 58, { 0x00,0x00,0x00,0x00,0x20,0x20,0x00,0x00,0x20,0x20,0x00,0x00 } },
	{ 65, { 0x00,0x00,0x70,0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00,0x00 } },
	{ 66, { 0x00,0x00,0xF0,0x88,0x88,0xF0,0x88,0x88,0x88,0xF0,0x00,0x00 } },
	{ 67, { 0x00,0x00,0x70,0x88,0x80,0x80,0x80,0x80,0x88,0x70,0x00,0x00 } },
	{ 68, { 0x00,0x00,0xE0,0x90,0x88,0x88,0x88,0x88,0x90,0xE0,0x00,0x00 } },
	{ 69, { 0x00,0x00,0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0xF8,0x00,0x00 } },
	{ 70, { 0x00,0x00,0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x80,0x00,0x00 } },
	{ 71, { 0x00,0x00,0x70,0x88,0x80,0x80,0xB8,0x88,0x88,0x70,0x00,0x00 } },
	{ 72, { 0x00,0x00,0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x88,0x00,0x00 } },
	{ 73, { 0x00,0x00,0x70,0x20,0x20,0x20,0x20,0x20,0x20,0x70,0x00,0x00 } },
	{ 74, { 0x00,0x00,0x38,0x10,0x10,0x10,0x10,0x90,0x90,0x60,0x00,0x00 } },
	{ 75, { 0x00,0x00,0x88,0x90,0xA0,0xC0,0xC0,0xA0,0x90,0x88,0x00,0x00 } },
	{ 76, { 0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00,0x00 } },
	{ 77, { 0x00,0x00,0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x88,0x00,0x00 } },
	{ 78, { 0x00,0x00,0x88,0x88,0xC8,0xA8,0x98,0x88,0x88,0x88,0x00,0x00 } },
	{ 79, { 0x00,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00 } },
	{ 80, { 0x00,0x00,0xF0,0x88,0x88,0x88,0xF0,0x80,0x80,0x80,0x00,0x00 } },
	{ 81, { 0x00,0x00,0x70,0x88,0x88,0x88,0x88,0x88,0xA8,0x70,0x08,0x00 } },
	{ 82, { 0x00,0x00,0xF0,0x88,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00,0x00 } },
	{ 83, { 0x00,0x00,0x70,0x88,0x80,0x70,0x08,0x08,0x88,0x70,0x00,0x00 } },
	{ 84, { 0x00,0x00,0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00 } },
	{ 85, { 0x00,0x00,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00,0x00 } },
	{ 86, { 0x00,0x00,0x88,0x88,0x88,0x50,0x50,0x50,0x20,0x20,0x00,0x00 } },
	{ 87, { 0x00,0x00,0x88,0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88,0x00,0x00 } },
	{ 88, { 0x00,0x00,0x88,0x88,0x50,0x20,0x20,0x50,0x88,0x88,0x00,0x00 } },
	{ 89, { 0x00,0x00,0x88,0x88,0x50,0x50,0x20,0x20,0x20,0x20,0x00,0x00 } },
	{ 90, { 0x00,0x00,0xF8,0x08,0x10,0x20,0x40,0x80,0x80,0xF8,0x00,0x00 } },
	{ 183, { 0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x00,0x00,0x00,0x00,0x00 } },
	{ 0, { 0 } }
};

/* clang-format on */

/* --- PHOSPHOR BASE24 --- */
#define BONE  0xFFFAEB /* base07 */
#define MUTED 0x96948B /* base05; never repeats a border colour */

/* The bright chromatic slots, which carry the borders. */
static const struct
{
    const char* name;
    unsigned int rgb;
} BORDERS[] = {
    {"red", 0xFF4C49},
    {"orange", 0xE7A739},
    {"yellow", 0xC98A04},
    {"lime", 0x9DDB3C},
    {"green", 0x44E084},
    {"blue", 0x3EA4F8},
    {"violet", 0xA573FF},
    {"magenta", 0xD5268A},
    {NULL, 0}
};

/* Borders that must hold, whatever the solver would prefer. */
static const struct
{
    const char* stamp;
    const char* colour;
} PINNED[] = {{"laroja", "red"}, {NULL, NULL}};

/* Grounds named by hand, for marks whose own colour says the wrong thing. */
static const struct
{
    const char* stamp;
    unsigned int rgb;
} FIXED[] = {
    {"chile", 0x4a0a12},    /* The star is white; take the flag's red. */
    {"laroja", 0x6b0f18},   /* The team is called the red one. */
    {"latine", 0x3b1030},   /* Tyrian purple, for the eagle. */
    {"santiago", 0x2e2a26}, /* The cross is red on white. */
    {"shell", 0x0b2418},    /* A drawn chevron on phosphor green. */
    {"foss", 0x5f2408},     /* The Free Software Foundation runs red. */
    {"instituto-nacional", 0x101f42},
    {"lazio", 0x0d2b46},  /* The club plays in sky blue. */
    {"futbol", 0x0f2a14}, /* Grass. */
    {"linkinpark", 0x2a0d14},
    {"clang", 0x1a2c5c},  /* The C logo is blue. */
    {"gentoo", 0x2a1f3d}, /* Gentoo purple. */
    {NULL, 0}
};

struct cell
{
    const char* name;
    const char* caption;
    const char* sub; /* NULL when the stamp carries one line */
    const char* logo;
    const char* want[2]; /* preferred border colours */
};

#define ROWS 3
#define COLS 7

static const struct cell WALL[ROWS][COLS] = {
    {
        {"instituto-nacional",
         "INSTITUTO",
         "NACIONAL",
         "instituto-nacional.png",
         {"violet", "magenta"}},
        {"uchile", "UNIVERSIDAD", "DE CHILE", "uchile.png", {"blue", "violet"}},
        {"fcfm", "FCFM", "UCHILE", "fcfm.png", {"red", "magenta"}},
        {"dcc", "DCC", "UCHILE", "dcc.png", {"magenta", "red"}},
        {"gnu-linux", "GNU/LINUX", NULL, "tux.png", {"yellow", "orange"}},
        {"gentoo", "GENTOO", NULL, "gentoo.png", {"violet", "magenta"}},
        {"openbsd", "OPENBSD", NULL, "openbsd.png", {"orange", "yellow"}},
    },
    {
        {"shell", "SHELL", NULL, "shell.png", {"green", "lime"}},
        {"clang", "C", NULL, "clang.png", {"blue", "violet"}},
        {"neovim", "NEOVIM", NULL, "neovim.png", {"lime", "green"}},
        {"foss", "FREE", "SOFTWARE", "fsf.png", {"magenta", "red"}},
        {"monero", "MONERO", NULL, "monero.png", {"orange", "yellow"}},
        {"minecraft", "MINECRAFT", NULL, "minecraft.png", {"lime", "green"}},
        {"dragonball",
         "DRAGON",
         "BALL",
         "dragonball.png",
         {"yellow", "orange"}},
    },
    {
        {"latine", "LINGVA", "LATINA", "aquila.png", {"violet", "magenta"}},
        {"santiago", "SANTIAGO", NULL, "santiago.png", {"red", "magenta"}},
        {"chile", "CHILE", NULL, "gunelve.png", {"blue", "green"}},
        {"laroja", "LA ROJA", NULL, "laroja.png", {"red", "magenta"}},
        {"lazio", "SS LAZIO", NULL, "lazio.png", {"blue", "green"}},
        {"futbol", "FOOTBALL", NULL, "futbol.png", {"green", "yellow"}},
        {"linkinpark",
         "LINKIN",
         "PARK",
         "linkinpark.png",
         {"magenta", "violet"}},
    }
};

static int border_of[ROWS][COLS]; /* index into BORDERS */
static unsigned int ground_of[ROWS][COLS];

/* Draw one caption line, centred in the space right of the canton. */
static void draw_line(
    struct image* im,
    const char* text,
    int y,
    unsigned int rgb
)
{
    size_t len = strlen(text);

    /* Terminus leaves a pixel of air beside each glyph; dropping it
       lets a long caption keep clear of the border. */
    int cell = len > 9 ? 5 : CELL;
    int x = CANTON + (W - CANTON - (int)len * cell) / 2;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        const unsigned char* rows = NULL;

        for (int g = 0; GLYPHS12[g].cp; g++) {
            if (GLYPHS12[g].cp == c) {
                rows = GLYPHS12[g].rows;
                break;
            }
        }
        if (rows) {
            for (int gy = 0; gy < GLYPH12_H; gy++) {
                for (int gx = 0; gx < GLYPH12_W; gx++) {
                    if (rows[gy] & (0x80 >> gx)) {
                        img_fill(im, x + gx, y + gy, x + gx, y + gy, rgb);
                    }
                }
            }
        }
        x += cell;
    }
}

/* The canton ground: the logotype's own colour, dropped to a field. */
static unsigned int ground_for(
    const char* name,
    const struct image* mark
)
{
    double sum[3] = {0, 0, 0};
    long n = 0;

    for (int i = 0; FIXED[i].stamp; i++) {
        if (!strcmp(FIXED[i].stamp, name)) {
            return FIXED[i].rgb;
        }
    }

    for (int y = 0; y < mark->h; y++) {
        for (int x = 0; x < mark->w; x++) {
            const unsigned char* p = mark->px + ((size_t)y * mark->w + x) * 4;
            int max = p[0] > p[1] ? p[0] : p[1];

            if (p[2] > max) {
                max = p[2];
            }
            /* Skip the transparent parts and the dark outlines. */
            if (p[3] > 120 && max > 60) {
                sum[0] += p[0];
                sum[1] += p[1];
                sum[2] += p[2];
                n++;
            }
        }
    }
    if (!n) {
        return 0x141a1e;
    }
    return ((unsigned int)(sum[0] / n * GROUND_LEVEL) << 16)
           | ((unsigned int)(sum[1] / n * GROUND_LEVEL) << 8)
           | (unsigned int)(sum[2] / n * GROUND_LEVEL);
}

/* Two grounds differ when their channels are far enough apart. */
static int far_enough(
    unsigned int a,
    unsigned int b
)
{
    int d = abs((int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF))
        + abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF))
        + abs((int)(a & 0xFF) - (int)(b & 0xFF));

    return d >= 34;
}

/*
 * Lift or deepen a ground, holding the hue the logotype gave it.
 * Separating by weight rather than by hue is what keeps Monero orange:
 * a neighbour moves in lightness, not in colour.
 */
static unsigned int shade(
    unsigned int rgb,
    int step
)
{
    double r = ((rgb >> 16) & 0xFF) / 255.0;
    double g = ((rgb >> 8) & 0xFF) / 255.0;
    double b = (rgb & 0xFF) / 255.0;
    double max = r > g ? r : g, min = r < g ? r : g;
    double h = 0.0, s, v;

    if (b > max) {
        max = b;
    }
    if (b < min) {
        min = b;
    }
    v = max;
    s = max > 0.0 ? (max - min) / max : 0.0;
    if (max > min) {
        double d = max - min;

        if (max == r) {
            h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (max == g) {
            h = (b - r) / d + 2.0;
        } else {
            h = (r - g) / d + 4.0;
        }
        h /= 6.0;
    }

    v += 0.055 * step;
    if (v < 0.05) {
        v = 0.05;
    }
    if (v > 0.42) {
        v = 0.42;
    }
    s += 0.05 * abs(step);
    if (s > 1.0) {
        s = 1.0;
    }

    {
        double c = v * s;
        double x = c * (1.0 - fabs(fmod(h * 6.0, 2.0) - 1.0));
        double m = v - c;
        double rr, gg, bb;
        int sector = (int)(h * 6.0) % 6;

        switch (sector) {
            case 0:
                rr = c;
                gg = x;
                bb = 0;
                break;
            case 1:
                rr = x;
                gg = c;
                bb = 0;
                break;
            case 2:
                rr = 0;
                gg = c;
                bb = x;
                break;
            case 3:
                rr = 0;
                gg = x;
                bb = c;
                break;
            case 4:
                rr = x;
                gg = 0;
                bb = c;
                break;
            default:
                rr = c;
                gg = 0;
                bb = x;
                break;
        }
        return ((unsigned int)((rr + m) * 255) << 16)
               | ((unsigned int)((gg + m) * 255) << 8)
               | (unsigned int)((bb + m) * 255);
    }
}

static int is_fixed(
    const char* name
)
{
    for (int i = 0; FIXED[i].stamp; i++) {
        if (!strcmp(FIXED[i].stamp, name)) {
            return 1;
        }
    }
    return 0;
}

static int border_index(
    const char* name
)
{
    for (int i = 0; BORDERS[i].name; i++) {
        if (!strcmp(BORDERS[i].name, name)) {
            return i;
        }
    }
    return -1;
}

/* Give each cell a border no neighbour holds, by preference. */
static void solve_borders(
    void
)
{
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            border_of[r][c] = -1;
        }
    }

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            for (int i = 0; PINNED[i].stamp; i++) {
                if (!strcmp(PINNED[i].stamp, WALL[r][c].name)) {
                    border_of[r][c] = border_index(PINNED[i].colour);
                }
            }
        }
    }

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int taken[16] = {0};

            if (border_of[r][c] >= 0) {
                continue;
            }
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;

                    if ((!dr && !dc) || nr < 0 || nc < 0 || nr >= ROWS
                        || nc >= COLS) {
                        continue;
                    }
                    if (border_of[nr][nc] >= 0) {
                        taken[border_of[nr][nc]] = 1;
                    }
                }
            }
            for (int w = 0; w < 2 && border_of[r][c] < 0; w++) {
                int idx = border_index(WALL[r][c].want[w]);

                if (idx >= 0 && !taken[idx]) {
                    border_of[r][c] = idx;
                }
            }
            for (int i = 0; BORDERS[i].name && border_of[r][c] < 0; i++) {
                if (!taken[i]) {
                    border_of[r][c] = i;
                }
            }
            if (border_of[r][c] < 0) {
                fprintf(stderr, "no colour left for %s\n", WALL[r][c].name);
                exit(1);
            }
        }
    }
}

/* Push a ground off its neighbours, in lightness. */
static void separate_grounds(
    void
)
{
    int moved = 0;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (is_fixed(WALL[r][c].name)) {
                continue;
            }
            for (int turn = 1; turn < 8; turn++) {
                int clash = 0;

                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        int nr = r + dr, nc = c + dc;

                        if ((!dr && !dc) || nr < 0 || nc < 0 || nr >= ROWS
                            || nc >= COLS) {
                            continue;
                        }
                        if (!far_enough(ground_of[r][c], ground_of[nr][nc])) {
                            clash = 1;
                        }
                    }
                }
                if (!clash) {
                    break;
                }

                /* Alternate down and up, so the wall keeps
                   both weights. */
                ground_of[r][c] = shade(
                    ground_of[r][c],
                    ((r + c) % 2) ? -turn : turn
                );
                if (turn == 1) {
                    moved++;
                }
            }
        }
    }
    if (moved) {
        printf("  grounds: %d moved off a neighbour\n", moved);
    }
}

static void check(
    void
)
{
    int left = 0;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;

                    if ((!dr && !dc) || nr < 0 || nc < 0 || nr >= ROWS
                        || nc >= COLS) {
                        continue;
                    }
                    if (border_of[r][c] == border_of[nr][nc]) {
                        fprintf(
                            stderr,
                            "border clash: %s touches %s\n",
                            WALL[r][c].name,
                            WALL[nr][nc].name
                        );
                        exit(1);
                    }
                    if (!far_enough(ground_of[r][c], ground_of[nr][nc])) {
                        left++;
                    }
                }
            }
        }
    }
    {
        int used[16] = {0}, n = 0;

        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (!used[border_of[r][c]]) {
                    used[border_of[r][c]] = 1;
                    n++;
                }
            }
        }
        printf("  borders: %d colours, no touching pair shares one\n", n);
    }
    if (left) {
        printf(
            "  grounds: %d pairs still sit close; name one in FIXED\n",
            left / 2
        );
    } else {
        printf("  grounds: every touching pair separated\n");
    }
}

static void write_stamp(
    const struct cell* cell,
    unsigned int accent,
    unsigned int ground
)
{
    struct image* im = img_new(W, H);
    struct image* mark, * scaled;
    char path[256];
    double ratio;
    int mw, mh;

    img_fill(im, 0, 0, W - 1, H - 1, 0x000000);
    img_fill(im, 0, 0, CANTON, H - 1, ground);

    snprintf(path, sizeof path, "assets/logos/%s", cell->logo);
    mark = png_read(path);
    if (!mark) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    ratio = (double)BOX / (mark->w > mark->h ? mark->w : mark->h);
    mw = (int)(mark->w * ratio);
    mh = (int)(mark->h * ratio);
    if (mw < 1) {
        mw = 1;
    }
    if (mh < 1) {
        mh = 1;
    }
    scaled = img_scale(mark, mw, mh);
    img_over(im, scaled, (CANTON - mw) / 2 + 1, (H - mh) / 2);
    img_free(mark);
    img_free(scaled);

    img_fill(im, CANTON, 2, CANTON, H - 3, accent);

    if (cell->sub) {
        draw_line(im, cell->caption, 15, BONE);
        draw_line(im, cell->sub, 29, MUTED);
    } else {
        draw_line(im, cell->caption, 22, BONE);
    }
    img_rect(im, 0, 0, W - 1, H - 1, accent);

    snprintf(path, sizeof path, "assets/stamps/%s.png", cell->name);
    png_write(path, im);
    img_free(im);
}

int main(
    void
)
{
    mkdir("assets", 0755);
    mkdir("assets/stamps", 0755);

    solve_borders();

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char path[256];
            struct image* mark;

            snprintf(path, sizeof path, "assets/logos/%s", WALL[r][c].logo);
            mark = png_read(path);
            if (!mark) {
                fprintf(stderr, "cannot read %s\n", path);
                return 1;
            }
            ground_of[r][c] = ground_for(WALL[r][c].name, mark);
            img_free(mark);
        }
    }

    separate_grounds();
    check();

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            write_stamp(
                &WALL[r][c],
                BORDERS[border_of[r][c]].rgb,
                ground_of[r][c]
            );
        }
    }

    printf("\n%d wall stamps -> assets/stamps\n\n", ROWS * COLS);
    for (int r = 0; r < ROWS; r++) {
        printf(" ");
        for (int c = 0; c < COLS; c++) {
            printf(
                " %9.9s %-4.4s#%06x",
                WALL[r][c].name,
                BORDERS[border_of[r][c]].name,
                ground_of[r][c]
            );
        }
        printf("\n");
    }
    return 0;
}
