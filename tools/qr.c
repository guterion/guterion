/*
 * tools/qr.c
 * @guterion
 * CC-BY-SA-4.0
 * Build the QR matrix that carries a payment address.
 */

/*
 * The symbols here hold payment addresses, which run from eighteen to
 * ninety-five bytes. That fits versions 1 to 5 at the L level of error
 * correction, where the codewords form a single block, so this file
 * carries no interleaving. A longer address stops the run rather than
 * silently losing its tail.
 *
 * The steps follow ISO/IEC 18004. The encoder writes the text into
 * codewords, adds the Reed-Solomon remainder, lays the function
 * patterns, walks the data through the matrix in the zigzag the
 * standard sets, and tries the eight masks. The mask with the lowest
 * score wins.
 */

#include <stdlib.h>
#include <string.h>

#include "qr.h"

/* --- VERSIONS --------------------------------------------------------- */

/*
 * One row for each version this file writes. It holds the data
 * codewords and the error correction codewords that follow them, at
 * level L.
 */
static const struct
{
    int data;
    int ecc;
    int align; /* Centre of the one alignment pattern, or 0. */
} versions[] = {
    {0, 0, 0}, /* Index 0 goes unused. */
    {19, 7, 0},
    {34, 10, 18},
    {55, 15, 22},
    {80, 20, 26},
    {108, 26, 30},
};

#define MAX_VERSION 5

static int version_size(
    int v
)
{
    return 17 + 4 * v;
}

/* --- GALOIS FIELD ----------------------------------------------------- */

/*
 * Arithmetic in GF(256) with the primitive polynomial 0x11D, which is
 * the field the standard names for the Reed-Solomon step.
 */
static unsigned char gf_exp[512];
static unsigned char gf_log[256];

static void gf_init(
    void
)
{
    int x = 1;

    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (unsigned char)x;
        gf_log[x] = (unsigned char)i;
        x <<= 1;
        if (x & 0x100) {
            x ^= 0x11D;
        }
    }
    for (int i = 255; i < 512; i++) {
        gf_exp[i] = gf_exp[i - 255];
    }
}

static unsigned char gf_mul(
    unsigned char a,
    unsigned char b
)
{
    if (!a || !b) {
        return 0;
    }
    return gf_exp[gf_log[a] + gf_log[b]];
}

/* The generator polynomial of the given degree. */
static void rs_generator(
    int degree,
    unsigned char* g
)
{
    memset(g, 0, (size_t)degree + 1);
    g[0] = 1;
    for (int i = 0; i < degree; i++) {
        /* Multiply the polynomial so far by (x - a^i). */
        for (int j = i + 1; j > 0; j--) {
            g[j] = (unsigned char)(g[j - 1] ^ gf_mul(g[j], gf_exp[i]));
        }
        g[0] = gf_mul(g[0], gf_exp[i]);
    }
}

/* The remainder that follows the data codewords. */
static void rs_encode(
    const unsigned char* data,
    int n,
    int ecc_len,
    unsigned char* ecc
)
{
    unsigned char g[64];

    rs_generator(ecc_len, g);
    memset(ecc, 0, (size_t)ecc_len);
    for (int i = 0; i < n; i++) {
        unsigned char factor = (unsigned char)(data[i] ^ ecc[0]);

        memmove(ecc, ecc + 1, (size_t)ecc_len - 1);
        ecc[ecc_len - 1] = 0;
        for (int j = 0; j < ecc_len; j++) {
            ecc[j] ^= gf_mul(g[ecc_len - 1 - j], factor);
        }
    }
}

/* --- MATRIX ----------------------------------------------------------- */

#define MAX_SIZE 37

/* A module that a function pattern owns cannot carry data or a mask. */
struct grid
{
    unsigned char m[MAX_SIZE][MAX_SIZE];
    unsigned char fixed[MAX_SIZE][MAX_SIZE];
    int size;
};

static void set_module(
    struct grid* g,
    int x,
    int y,
    int dark
)
{
    if (x < 0 || y < 0 || x >= g->size || y >= g->size) {
        return;
    }
    g->m[y][x] = (unsigned char)dark;
    g->fixed[y][x] = 1;
}

/* The three corners that tell a reader where the symbol is. */
static void place_finder(
    struct grid* g,
    int ox,
    int oy
)
{
    for (int y = -1; y <= 7; y++) {
        for (int x = -1; x <= 7; x++) {
            int inside = x >= 0 && x <= 6 && y >= 0 && y <= 6;
            int ring = x == 0 || x == 6 || y == 0 || y == 6;
            int core = x >= 2 && x <= 4 && y >= 2 && y <= 4;

            set_module(g, ox + x, oy + y, inside && (ring || core));
        }
    }
}

static void place_alignment(
    struct grid* g,
    int cx,
    int cy
)
{
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            int edge = x == -2 || x == 2 || y == -2 || y == 2;

            set_module(g, cx + x, cy + y, edge || (!x && !y));
        }
    }
}

static void place_function_patterns(
    struct grid* g,
    int version
)
{
    int n = g->size;

    place_finder(g, 0, 0);
    place_finder(g, n - 7, 0);
    place_finder(g, 0, n - 7);

    /* The timing lines run between the finders. */
    for (int i = 8; i < n - 8; i++) {
        set_module(g, i, 6, !(i % 2));
        set_module(g, 6, i, !(i % 2));
    }

    if (versions[version].align) {
        place_alignment(g, versions[version].align, versions[version].align);
    }

    /* The module that is always dark. */
    set_module(g, 8, n - 8, 1);

    /* Reserve the two areas that carry the format bits. */
    for (int i = 0; i <= 8; i++) {
        if (i != 6) {
            set_module(g, i, 8, 0);
            set_module(g, 8, i, 0);
        }
        if (i < 8) {
            set_module(g, n - 1 - i, 8, 0);
        }
        if (i < 7) {
            set_module(g, 8, n - 1 - i, 0);
        }
    }
}

/*
 * Walk the codewords into the matrix. The path runs upward through two
 * columns at a time, then downward through the next two, and it steps
 * over the vertical timing line.
 */
static void place_data(
    struct grid* g,
    const unsigned char* bits,
    int total
)
{
    int n = g->size;
    int bit = 0;
    int upward = 1;

    for (int right = n - 1; right > 0; right -= 2) {
        if (right == 6) { /* The timing column takes no data. */
            right = 5;
        }
        for (int step = 0; step < n; step++) {
            int y = upward ? n - 1 - step : step;

            for (int k = 0; k < 2; k++) {
                int x = right - k;

                if (g->fixed[y][x]) {
                    continue;
                }
                g->m[y][x] = bit < total ? bits[bit] : 0;
                bit++;
            }
        }
        upward = !upward;
    }
}

/* The eight masks of the standard, by their number. */
static int mask_bit(
    int mask,
    int x,
    int y
)
{
    switch (mask) {
        case 0:
            return (x + y) % 2 == 0;
        case 1:
            return y % 2 == 0;
        case 2:
            return x % 3 == 0;
        case 3:
            return (x + y) % 3 == 0;
        case 4:
            return (y / 2 + x / 3) % 2 == 0;
        case 5:
            return (x * y) % 2 + (x * y) % 3 == 0;
        case 6:
            return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
        default:
            return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
    }
}

static void apply_mask(
    struct grid* g,
    int mask
)
{
    for (int y = 0; y < g->size; y++) {
        for (int x = 0; x < g->size; x++) {
            if (!g->fixed[y][x] && mask_bit(mask, x, y)) {
                g->m[y][x] ^= 1;
            }
        }
    }
}

/*
 * The format bits. They carry the error correction level, the mask,
 * and a BCH remainder, all exclusive-ored with a fixed pattern so the
 * field is never all zero.
 */
static void place_format(
    struct grid* g,
    int mask
)
{
    int n = g->size;
    int data = (0x01 << 3) | mask; /* 01 is level L. */
    int rem = data;

    for (int i = 0; i < 10; i++) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    int bits = ((data << 10) | rem) ^ 0x5412;

    for (int i = 0; i <= 5; i++) {
        set_module(g, 8, i, (bits >> i) & 1);
    }
    set_module(g, 8, 7, (bits >> 6) & 1);
    set_module(g, 8, 8, (bits >> 7) & 1);
    set_module(g, 7, 8, (bits >> 8) & 1);
    for (int i = 9; i < 15; i++) {
        set_module(g, 14 - i, 8, (bits >> i) & 1);
    }

    for (int i = 0; i < 8; i++) {
        set_module(g, n - 1 - i, 8, (bits >> i) & 1);
    }
    for (int i = 8; i < 15; i++) {
        set_module(g, 8, n - 15 + i, (bits >> i) & 1);
    }
    set_module(g, 8, n - 8, 1);
}

/* --- MASK SCORE ------------------------------------------------------- */

/*
 * The four penalties of the standard. The mask that scores lowest is
 * the one that leaves the fewest patterns a reader could mistake for a
 * finder, and the most even balance of dark and light.
 */
static int penalty(
    const struct grid* g
)
{
    int n = g->size, score = 0;

    /* Runs of five or more in a line. */
    for (int i = 0; i < n; i++) {
        for (int dir = 0; dir < 2; dir++) {
            int run = 1;

            for (int j = 1; j < n; j++) {
                int a = dir ? g->m[j - 1][i] : g->m[i][j - 1];
                int b = dir ? g->m[j][i] : g->m[i][j];

                if (a == b) {
                    run++;
                    continue;
                }
                if (run >= 5) {
                    score += run - 2;
                }
                run = 1;
            }
            if (run >= 5) {
                score += run - 2;
            }
        }
    }

    /* Blocks of the same colour, two by two. */
    for (int y = 0; y + 1 < n; y++) {
        for (int x = 0; x + 1 < n; x++) {
            if (g->m[y][x] == g->m[y][x + 1] && g->m[y][x] == g->m[y + 1][x]
                && g->m[y][x] == g->m[y + 1][x + 1]) {
                score += 3;
            }
        }
    }

    /* The finder-like sequence, in either direction. */
    static const int want[7] = {1, 0, 1, 1, 1, 0, 1};

    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            for (int dir = 0; dir < 2; dir++) {
                int ok = 1;

                if (dir ? y + 7 > n : x + 7 > n) {
                    continue;
                }
                for (int k = 0; k < 7 && ok; k++) {
                    int v = dir ? g->m[y + k][x] : g->m[y][x + k];

                    ok = v == want[k];
                }
                if (!ok) {
                    continue;
                }
                /* Four light modules on one side of it. */
                int before = 1, after = 1;

                for (int k = 1; k <= 4; k++) {
                    int p = dir ? y - k : x - k;
                    int q = dir ? y + 6 + k : x + 6 + k;

                    if (p >= 0 && (dir ? g->m[p][x] : g->m[y][p])) {
                        before = 0;
                    }
                    if (q < n && (dir ? g->m[q][x] : g->m[y][q])) {
                        after = 0;
                    }
                }
                if (before || after) {
                    score += 40;
                }
            }
        }
    }

    /* How far the dark modules sit from half of the symbol. */
    int dark = 0;

    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            dark += g->m[y][x];
        }
    }
    int percent = dark * 100 / (n * n);
    int off = percent > 50 ? percent - 50 : 50 - percent;

    score += off / 5 * 10;
    return score;
}

/* --- THE SYMBOL ------------------------------------------------------- */

struct qr* qr_make(
    const char* text
)
{
    size_t len = strlen(text);
    int version = 0;
    unsigned char codewords[256];
    unsigned char bits[256 * 8];
    struct grid best;
    int best_score = -1;

    gf_init();

    for (int v = 1; v <= MAX_VERSION; v++) {
        /* Four bits of mode, eight of length, then the text. */
        if ((int)len + 2 <= versions[v].data) {
            version = v;
            break;
        }
    }
    if (!version) {
        return NULL;
    }

    {
        int total = versions[version].data;
        int n = 0;
        unsigned int acc = 0;
        int acc_bits = 0;

        /* Byte mode, then the length in eight bits. */
        acc = (acc << 4) | 4;
        acc_bits += 4;
        acc = (acc << 8) | (unsigned int)(len & 0xFF);
        acc_bits += 8;
        for (size_t i = 0; i <= len; i++) {
            if (i < len) {
                acc = (acc << 8) | (unsigned char)text[i];
                acc_bits += 8;
            } else {
                acc <<= 4; /* The terminator. */
                acc_bits += 4;
            }
            while (acc_bits >= 8) {
                acc_bits -= 8;
                codewords[n++] = (unsigned char)(acc >> acc_bits);
            }
        }
        if (acc_bits) {
            codewords[n++] = (unsigned char)(acc << (8 - acc_bits));
        }
        /* The pad bytes the standard names, in turn. */
        for (int pad = 0; n < total; pad++) {
            codewords[n++] = pad % 2 ? 0x11 : 0xEC;
        }

        rs_encode(codewords, total, versions[version].ecc, codewords + total);
    }

    {
        int total = versions[version].data + versions[version].ecc;

        for (int i = 0; i < total; i++) {
            for (int b = 7; b >= 0; b--) {
                bits[i * 8 + (7 - b)] = (codewords[i] >> b) & 1;
            }
        }

        for (int mask = 0; mask < 8; mask++) {
            struct grid g;
            int score;

            memset(&g, 0, sizeof g);
            g.size = version_size(version);
            place_function_patterns(&g, version);
            place_data(&g, bits, total * 8);
            apply_mask(&g, mask);
            place_format(&g, mask);
            score = penalty(&g);
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best = g;
            }
        }
    }

    {
        struct qr* q = malloc(sizeof *q);
        int n = best.size;

        if (!q) {
            return NULL;
        }
        q->size = n;
        q->m = malloc((size_t)n * (size_t)n);
        if (!q->m) {
            free(q);
            return NULL;
        }
        for (int y = 0; y < n; y++) {
            for (int x = 0; x < n; x++) {
                q->m[y * n + x] = best.m[y][x];
            }
        }
        return q;
    }
}

void qr_free(
    struct qr* q
)
{
    if (!q) {
        return;
    }
    free(q->m);
    free(q);
}
