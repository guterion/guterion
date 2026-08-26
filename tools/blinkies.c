/*
 * tools/blinkies.c
 * @guterion
 * CC-BY-SA-4.0
 * Animate the wall stamps with a band of light crossing each face.
 */

/*
 *     $FILC/build/bin/clang -g -O -o tools/blinkies \
 *         tools/blinkies.c tools/img.c -I tools -lm
 *
 * A soft highlight sweeps from one side to the other and wraps, so the
 * loop closes without a seam. Each stamp enters the cycle at its own
 * point, taken from its name, which stops the wall from pulsing as one
 * block.
 *
 * Browsers give every GIF its own clock, and each clock starts when
 * the browser finishes decoding. A sweep that crosses the whole wall
 * in step is therefore beyond what this file can promise. The phase
 * offset is what keeps the wall from looking synchronised.
 */

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"

#define FRAMES   18
#define FLOOR    0.86  /* The face at rest. */
#define LIFT     0.62  /* How much brighter the band makes it. */
#define SPREAD   300.0 /* Width of the band, as a squared falloff. */
#define DELAY_CS 7     /* Seventy milliseconds a frame. */

#define MAX_STAMPS 64
#define MAX_NAME   128

/* Multiply one channel and clamp it. */
static unsigned char lift(
    unsigned char v,
    double k
)
{
    double out = v * k;

    return out > 255.0 ? 255 : (unsigned char)out;
}

/* One frame, a band of light centred somewhere along the face. */
static struct image* sweep(
    const struct image* base,
    double phase
)
{
    struct image* out = img_new(base->w, base->h);
    double centre = phase * (base->w + 60) - 30;
    double* weights;

    if (!out) {
        return NULL;
    }
    weights = malloc(sizeof *weights * (size_t)base->w);
    if (!weights) {
        img_free(out);
        return NULL;
    }

    /* The column weights are shared by every row. */
    for (int x = 0; x < base->w; x++) {
        double d = x - centre;
        double near = exp(-(d * d) / SPREAD);

        /* The band wraps, so its leading edge appears before the
           tail has gone. */
        double d2 = x - (centre - (base->w + 60));
        double near2 = exp(-(d2 * d2) / SPREAD);

        if (near2 > near) {
            near = near2;
        }
        weights[x] = FLOOR + LIFT * near;
    }

    for (int y = 0; y < base->h; y++) {
        for (int x = 0; x < base->w; x++) {
            const unsigned char* s = base->px + img_offset(base->w, x, y);
            unsigned char* d = out->px + img_offset(out->w, x, y);
            double k = weights[x];

            d[0] = lift(s[0], k);
            d[1] = lift(s[1], k);
            d[2] = lift(s[2], k);
            d[3] = s[3];
        }
    }

    free(weights);
    return out;
}

static int by_name(
    const void* a,
    const void* b
)
{
    return strcmp((const char*)a, (const char*)b);
}

/*
 * The point in the cycle where one stamp enters, as a fraction. It
 * comes from the name rather than from the position in the listing, so
 * a stamp that joins the wall writes its own file and leaves every
 * other one at the phase it already had. The hash is FNV-1a, which is
 * short and spreads the names well enough for the eye.
 */
static double phase_of(
    const char* name
)
{
    unsigned int h = 2166136261U;

    for (size_t i = 0; name[i]; i++) {
        h ^= (unsigned char)name[i];
        h *= 16777619U;
    }
    return (double)(h % 1000U) / 1000.0;
}

int main(
    void
)
{
    char names[MAX_STAMPS][MAX_NAME];
    int count = 0;
    long total = 0;
    DIR* d = opendir("assets/stamps");
    const struct dirent* entry;

    if (!d) {
        fprintf(stderr, "cannot open assets/stamps\n");
        return 1;
    }
    while ((entry = readdir(d)) != NULL && count < MAX_STAMPS) {
        size_t len = strlen(entry->d_name);

        if (len < 5 || strcmp(entry->d_name + len - 4, ".png")) {
            continue;
        }
        snprintf(names[count], MAX_NAME, "%.*s", (int)(len - 4), entry->d_name);
        count++;
    }
    closedir(d);
    qsort(names, (size_t)count, MAX_NAME, by_name);

    for (int i = 0; i < count; i++) {
        char in_path[256], out_path[256];
        struct image* base;
        struct image* frames[FRAMES];
        double offset;

        snprintf(in_path, sizeof in_path, "assets/stamps/%s.png", names[i]);
        base = png_read(in_path);
        if (!base) {
            fprintf(stderr, "cannot read %s\n", in_path);
            return 1;
        }

        offset = phase_of(names[i]);
        for (int f = 0; f < FRAMES; f++) {
            frames[f] = sweep(base, fmod((double)f / FRAMES + offset, 1.0));
        }

        snprintf(out_path, sizeof out_path, "assets/stamps/%s.gif", names[i]);
        if (gif_write(out_path, frames, FRAMES, DELAY_CS)) {
            fprintf(stderr, "cannot write %s\n", out_path);
            return 1;
        }

        {
            FILE* f = fopen(out_path, "rb");

            if (f) {
                fseek(f, 0, SEEK_END);
                total += ftell(f);
                fclose(f);
            }
        }
        printf("  %-22s\n", names[i]);

        for (int f = 0; f < FRAMES; f++) {
            img_free(frames[f]);
        }
        img_free(base);
    }

    printf("\n%d stamps animated, %ld bytes\n", count, total);
    return 0;
}
