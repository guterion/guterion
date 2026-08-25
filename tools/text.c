/*
 * tools/text.c
 * @guterion
 * CC-BY-SA-4.0
 * Set a label in League Mono Narrow Bold, as an SVG path or as pixels
 *
 * The outlines travel in `leaguemono.h`, so the badges depend on no
 * font library and on no font that the reader happens to hold. The
 * same outline feeds both consumers: the SVG receives it as path data,
 * which every browser draws the same way, and the PNG receives it as
 * coverage that this file rasterises.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"
#include "leaguemono.h"
#include "text.h"

/*
 * Vertical samples for each row of pixels. The horizontal coverage is
 * exact, so this number alone decides how smooth a near-horizontal
 * edge looks. Sixteen samples put the steps below what the eye finds
 * at this size.
 */
#define SAMPLES 16

/* Line segments that one quadratic curve becomes when it is flattened. */
#define CURVE_STEPS 8

/* --- GLYPH LOOKUP ----------------------------------------------------- */

static const struct glyph* find_glyph(
    unsigned char c
)
{
    for (int i = 0; GLYPHS[i].cp; i++) {
        if (GLYPHS[i].cp == c) {
            return &GLYPHS[i];
        }
    }
    return NULL;
}

double text_scale(
    double size
)
{
    return size / GLYPH_EM;
}

double text_advance(
    const char* s,
    double size
)
{
    size_t n = strlen(s);

    return (double)n * GLYPH_ADVANCE * size / GLYPH_EM;
}

/* --- SVG PATH --------------------------------------------------------- */

/*
 * Append to the buffer and report the length the whole path needs. The
 * caller compares that length against the buffer it gave.
 */
__attribute__((format(
        printf,
        4,
        5
    ))) static void put(
    char* buf,
    size_t n,
    size_t* len,
    const char* fmt,
    ...
    )
{
    va_list ap;
    char part[128];
    int got;

    va_start(ap, fmt);
    got = vsnprintf(part, sizeof part, fmt, ap);
    va_end(ap);
    if (got < 0) {
        return;
    }
    if (*len + (size_t)got < n) {
        memcpy(buf + *len, part, (size_t)got + 1);
    }
    *len += (size_t)got;
}

size_t text_path(
    char* buf,
    size_t n,
    const char* s
)
{
    size_t len = 0;
    int pen = 0;

    if (n) {
        buf[0] = '\0';
    }

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        const struct glyph* g = find_glyph(*p);

        if (g) {
            for (int i = 0; i < g->count; i++) {
                const struct stroke* k = &STROKES[g->first + i];

                switch (k->op) {
                    case 'M':
                        put(buf, n, &len, "M%d %d", pen + k->a, k->b);
                        break;
                    case 'L':
                        put(buf, n, &len, "L%d %d", pen + k->a, k->b);
                        break;
                    case 'Q':
                        put(buf,
                            n,
                            &len,
                            "Q%d %d %d %d",
                            pen + k->a,
                            k->b,
                            pen + k->c,
                            k->d
                        );
                        break;
                    case 'Z':
                        put(buf, n, &len, "Z");
                        break;
                    default:
                        fprintf(
                            stderr,
                            "text: stroke '%c' is unknown\n",
                            k->op
                        );
                        exit(1);
                }
            }
        }
        pen += GLYPH_ADVANCE;
    }
    return len;
}

/* --- RASTER ----------------------------------------------------------- */

/* One flattened line of a contour, with the direction it runs. */
struct edge
{
    double x0, y0, x1, y1;
    int dir;
};

struct edge_list
{
    struct edge* e;
    int n, cap;
};

static int push_edge(
    struct edge_list* l,
    double x0,
    double y0,
    double x1,
    double y1
)
{
    struct edge* slot;

    if (y0 == y1) { /* A flat line crosses no sample. */
        return 0;
    }
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 256;
        struct edge* grown = realloc(l->e, sizeof *grown * (size_t)cap);

        if (!grown) {
            return -1;
        }
        l->e = grown;
        l->cap = cap;
    }
    slot = &l->e[l->n++];
    if (y0 < y1) {
        slot->x0 = x0;
        slot->y0 = y0;
        slot->x1 = x1;
        slot->y1 = y1;
        slot->dir = 1;
    } else {
        slot->x0 = x1;
        slot->y0 = y1;
        slot->x1 = x0;
        slot->y1 = y0;
        slot->dir = -1;
    }
    return 0;
}

/* Turn one label into the edges that bound its filled area. */
static int build_edges(
    struct edge_list* l,
    const char* s,
    double x,
    double y,
    double size
)
{
    double scale = size / GLYPH_EM;
    double pen = x;

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        const struct glyph* g = find_glyph(*p);
        double cur_x = 0, cur_y = 0, start_x = 0, start_y = 0;

        if (!g) {
            pen += GLYPH_ADVANCE * scale;
            continue;
        }
        for (int i = 0; i < g->count; i++) {
            const struct stroke* k = &STROKES[g->first + i];
            double ax = pen + k->a * scale;
            double ay = y - k->b * scale;
            double cx = pen + k->c * scale;
            double cy = y - k->d * scale;

            switch (k->op) {
                case 'M':
                    cur_x = start_x = ax;
                    cur_y = start_y = ay;
                    break;
                case 'L':
                    if (push_edge(l, cur_x, cur_y, ax, ay)) {
                        return -1;
                    }
                    cur_x = ax;
                    cur_y = ay;
                    break;
                case 'Q':
                    /* Flatten the curve into short lines. */
                    for (int t = 1; t <= CURVE_STEPS; t++) {
                        double u = (double)t / CURVE_STEPS;
                        double v = 1.0 - u;
                        double px = v * v * cur_x + 2 * v * u * ax + u * u * cx;
                        double py = v * v * cur_y + 2 * v * u * ay + u * u * cy;

                        if (push_edge(l, cur_x, cur_y, px, py)) {
                            return -1;
                        }
                        cur_x = px;
                        cur_y = py;
                    }
                    break;
                case 'Z':
                    if (push_edge(l, cur_x, cur_y, start_x, start_y)) {
                        return -1;
                    }
                    cur_x = start_x;
                    cur_y = start_y;
                    break;
                default:
                    fprintf(stderr, "text: stroke '%c' is unknown\n", k->op);
                    exit(1);
            }
        }
        pen += GLYPH_ADVANCE * scale;
    }
    return 0;
}

/* One crossing of a sample line, and which way the contour ran there. */
struct hit
{
    double x;
    int dir;
};

static int by_x(
    const void* a,
    const void* b
)
{
    const struct hit* p = a, * q = b;

    return p->x < q->x ? -1 : (p->x > q->x ? 1 : 0);
}

/*
 * Add the coverage of one span to the row. A pixel that the span
 * crosses in part receives that part, which is what smooths a vertical
 * edge.
 */
static void add_span(
    double* row,
    int w,
    double x0,
    double x1,
    double weight
)
{
    int first, last;

    if (x1 <= x0) {
        return;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > w) {
        x1 = w;
    }
    if (x1 <= x0) {
        return;
    }

    first = (int)floor(x0);
    last = (int)ceil(x1) - 1;
    for (int px = first; px <= last && px < w; px++) {
        double lo = x0 > px ? x0 : px;
        double hi = x1 < px + 1 ? x1 : px + 1;

        if (hi > lo) {
            row[px] += (hi - lo) * weight;
        }
    }
}

void text_draw(
    struct image* im,
    const char* s,
    double x,
    double y,
    double size,
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{
    struct edge_list l = {NULL, 0, 0};
    struct hit* hits;
    double* row;

    if (build_edges(&l, s, x, y, size)) {
        free(l.e);
        return;
    }
    if (!l.n) {
        free(l.e);
        return;
    }
    hits = malloc(sizeof *hits * (size_t)l.n);
    row = malloc(sizeof *row * (size_t)im->w);
    if (!hits || !row) {
        free(hits);
        free(row);
        free(l.e);
        return;
    }

    for (int py = 0; py < im->h; py++) {
        int painted = 0;

        for (int i = 0; i < im->w; i++) {
            row[i] = 0.0;
        }

        for (int s_i = 0; s_i < SAMPLES; s_i++) {
            double sy = py + (s_i + 0.5) / SAMPLES;
            int n = 0;
            int winding = 0;

            for (int i = 0; i < l.n; i++) {
                const struct edge* e = &l.e[i];

                if (sy < e->y0 || sy >= e->y1) {
                    continue;
                }
                hits[n].x = e->x0
                    + (e->x1 - e->x0) * (sy - e->y0) / (e->y1 - e->y0);
                hits[n].dir = e->dir;
                n++;
            }
            if (n < 2) {
                continue;
            }
            qsort(hits, (size_t)n, sizeof *hits, by_x);

            /* Non-zero winding: the fill lies where the
               crossings do not cancel out. */
            for (int i = 0; i < n - 1; i++) {
                winding += hits[i].dir;
                if (winding) {
                    add_span(
                        row,
                        im->w,
                        hits[i].x,
                        hits[i + 1].x,
                        1.0 / SAMPLES
                    );
                }
            }
            painted = 1;
        }
        if (!painted) {
            continue;
        }

        for (int px = 0; px < im->w; px++) {
            double a = row[px];
            unsigned char* d;

            if (a <= 0.0) {
                continue;
            }
            if (a > 1.0) {
                a = 1.0;
            }
            d = im->px + img_offset(im->w, px, py);
            d[0] = (unsigned char)(d[0] * (1 - a) + r * a + 0.5);
            d[1] = (unsigned char)(d[1] * (1 - a) + g * a + 0.5);
            d[2] = (unsigned char)(d[2] * (1 - a) + b * a + 0.5);
            if (d[3] < (unsigned char)(a * 255 + 0.5)) {
                d[3] = (unsigned char)(a * 255 + 0.5);
            }
        }
    }

    free(hits);
    free(row);
    free(l.e);
}
