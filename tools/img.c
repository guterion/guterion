/*
 * tools/img.c
 * @guterion
 * CC-BY-SA-4.0
 * Image primitives: inflate, PNG in and out, Lanczos scaling, GIF out
 *
 * The generators need to read the logotypes, scale them, compose the
 * stamps and write the animation. Fil-C cannot link against libpng or
 * giflib, so each piece is here, in the smallest form that does the job
 * correctly.
 */

/* M_PI comes from the GNU extensions, not from C itself. */
#define _GNU_SOURCE

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"

/* ===================== IMAGE ===================== */

struct image* img_new(
    int w,
    int h
)
{
    struct image* im = malloc(sizeof *im);

    if (!im) {
        return NULL;
    }
    im->w = w;
    im->h = h;
    im->px = calloc((size_t)w * (size_t)h * 4, 1);
    if (!im->px) {
        free(im);
        return NULL;
    }
    return im;
}

void img_free(
    struct image* im
)
{
    if (!im) {
        return;
    }
    free(im->px);
    free(im);
}

void img_fill(
    struct image* im,
    int x0,
    int y0,
    int x1,
    int y1,
    unsigned int rgb
)
{
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= im->h) {
            continue;
        }
        for (int x = x0; x <= x1; x++) {
            unsigned char* p;

            if (x < 0 || x >= im->w) {
                continue;
            }
            p = im->px + ((size_t)y * im->w + x) * 4;
            p[0] = (rgb >> 16) & 0xFF;
            p[1] = (rgb >> 8) & 0xFF;
            p[2] = rgb & 0xFF;
            p[3] = 255;
        }
    }
}

void img_rect(
    struct image* im,
    int x0,
    int y0,
    int x1,
    int y1,
    unsigned int rgb
)
{
    img_fill(im, x0, y0, x1, y0, rgb);
    img_fill(im, x0, y1, x1, y1, rgb);
    img_fill(im, x0, y0, x0, y1, rgb);
    img_fill(im, x1, y0, x1, y1, rgb);
}

void img_over(
    struct image* dst,
    const struct image* top,
    int x,
    int y
)
{
    for (int j = 0; j < top->h; j++) {
        int dy = y + j;

        if (dy < 0 || dy >= dst->h) {
            continue;
        }
        for (int i = 0; i < top->w; i++) {
            int dx = x + i;
            const unsigned char* s;
            unsigned char* d;
            unsigned int a;

            if (dx < 0 || dx >= dst->w) {
                continue;
            }
            s = top->px + ((size_t)j * top->w + i) * 4;
            d = dst->px + ((size_t)dy * dst->w + dx) * 4;
            a = s[3];
            if (!a) {
                continue;
            }
            for (int c = 0; c < 3; c++) {
                d[c] = (unsigned char)((s[c] * a + d[c] * (255 - a)) / 255);
            }
            d[3] = (unsigned char)(a + d[3] * (255 - a) / 255);
        }
    }
}

/* ===================== INFLATE ===================== */

struct bitstream
{
    const unsigned char* data;
    size_t len;
    size_t pos;
    unsigned int bit_buf;
    int bit_count;
};

static int bits_get(
    struct bitstream* bs,
    int count
)
{
    int value = 0;

    for (int i = 0; i < count; i++) {
        if (bs->bit_count == 0) {
            if (bs->pos >= bs->len) {
                return -1;
            }
            bs->bit_buf = bs->data[bs->pos++];
            bs->bit_count = 8;
        }
        value |= (int)((bs->bit_buf & 1) << i);
        bs->bit_buf >>= 1;
        bs->bit_count--;
    }
    return value;
}

/* A canonical Huffman table, in the form the DEFLATE spec describes. */
struct huffman
{
    unsigned short counts[16];
    unsigned short symbols[288];
};

static void huff_build(
    struct huffman* h,
    const unsigned char* lengths,
    int count
)
{
    unsigned short offsets[16];

    memset(h->counts, 0, sizeof h->counts);
    for (int i = 0; i < count; i++) {
        h->counts[lengths[i]]++;
    }
    h->counts[0] = 0;

    offsets[0] = 0;
    for (int i = 1; i < 16; i++) {
        offsets[i] = (unsigned short)(offsets[i - 1] + h->counts[i - 1]);
    }

    for (int i = 0; i < count; i++) {
        if (lengths[i]) {
            h->symbols[offsets[lengths[i]]++] = (unsigned short)i;
        }
    }
}

static int huff_decode(
    struct bitstream* bs,
    const struct huffman* h
)
{
    int code = 0, first = 0, index = 0;

    for (int len = 1; len < 16; len++) {
        int bit = bits_get(bs, 1);

        if (bit < 0) {
            return -1;
        }
        code |= bit;
        if (code - first < h->counts[len]) {
            return h->symbols[index + (code - first)];
        }
        index += h->counts[len];
        first = (first + h->counts[len]) << 1;
        code <<= 1;
    }
    return -1;
}

/* The length and distance codes of RFC 1951, read by the inflate
   above and written by the deflate below. */
static const unsigned short LEN_BASE[] = {3, 4, 5, 6, 7, 8, 9, 10,
                                          11, 13, 15, 17, 19, 23, 27, 31,
                                          35, 43, 51, 59, 67, 83, 99, 115,
                                          131, 163, 195, 227, 258};
static const unsigned char LEN_EXTRA[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                          1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                          4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short DIST_BASE[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
    33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned char DIST_EXTRA[] = {0, 0, 0, 0, 1, 1, 2, 2,
                                           3, 3, 4, 4, 5, 5, 6, 6,
                                           7, 7, 8, 8, 9, 9, 10, 10,
                                           11, 11, 12, 12, 13, 13};

struct growbuf
{
    unsigned char* data;
    size_t len;
    size_t cap;
};

static int grow_push(
    struct growbuf* g,
    unsigned char byte
)
{
    if (g->len == g->cap) {
        size_t cap = g->cap ? g->cap * 2 : 65536;
        unsigned char* bigger = realloc(g->data, cap);

        if (!bigger) {
            return -1;
        }
        g->data = bigger;
        g->cap = cap;
    }
    g->data[g->len++] = byte;
    return 0;
}

/*
 * Inflate a zlib stream: two header bytes, then the DEFLATE blocks.
 * Returns a fresh buffer that the caller owns.
 */
static unsigned char* inflate_zlib(
    const unsigned char* src,
    size_t src_len,
    size_t* out_len
)
{
    struct bitstream bs = {src, src_len, 2, 0, 0}; /* skip zlib header */
    struct growbuf out = {NULL, 0, 0};
    struct huffman fixed_lit, fixed_dist;
    unsigned char lengths[288 + 32];
    int final;

    if (src_len < 3) {
        return NULL;
    }

    /* The fixed tables the spec defines, built once. */
    for (int i = 0; i < 288; i++) {
        lengths[i] = (unsigned char)(i < 144   ? 8
                                     : i < 256 ? 9
                                     : i < 280 ? 7
                                               : 8);
    }
    huff_build(&fixed_lit, lengths, 288);
    for (int i = 0; i < 30; i++) {
        lengths[i] = 5;
    }
    huff_build(&fixed_dist, lengths, 30);

    do {
        struct huffman lit, dist;
        const struct huffman* use_lit, * use_dist;
        int type;

        final = bits_get(&bs, 1);
        type = bits_get(&bs, 2);
        if (final < 0 || type < 0) {
            goto fail;
        }

        if (type == 0) {
            unsigned int len;

            bs.bit_count = 0; /* stored blocks are aligned */
            if (bs.pos + 4 > bs.len) {
                goto fail;
            }
            len = (unsigned int)bs.data[bs.pos]
                | ((unsigned int)bs.data[bs.pos + 1] << 8);
            bs.pos += 4;
            for (unsigned int i = 0; i < len; i++) {
                if (bs.pos >= bs.len) {
                    goto fail;
                }
                if (grow_push(&out, bs.data[bs.pos++])) {
                    goto fail;
                }
            }
            continue;
        }

        if (type == 1) {
            use_lit = &fixed_lit;
            use_dist = &fixed_dist;
        } else if (type == 2) {
            static const unsigned char ORDER[] = {
                16,
                17,
                18,
                0,
                8,
                7,
                9,
                6,
                10,
                5,
                11,
                4,
                12,
                3,
                13,
                2,
                14,
                1,
                15
            };
            unsigned char code_lengths[19] = {0};
            struct huffman code_huff;
            int hlit = bits_get(&bs, 5) + 257;
            int hdist = bits_get(&bs, 5) + 1;
            int hclen = bits_get(&bs, 4) + 4;
            int n = 0;

            if (hlit < 257 || hdist < 1) {
                goto fail;
            }
            for (int i = 0; i < hclen; i++) {
                int v = bits_get(&bs, 3);

                if (v < 0) {
                    goto fail;
                }
                code_lengths[ORDER[i]] = (unsigned char)v;
            }
            huff_build(&code_huff, code_lengths, 19);

            memset(lengths, 0, sizeof lengths);
            while (n < hlit + hdist) {
                int sym = huff_decode(&bs, &code_huff);
                int repeat, value = 0;

                if (sym < 0) {
                    goto fail;
                }
                if (sym < 16) {
                    lengths[n++] = (unsigned char)sym;
                    continue;
                }
                if (sym == 16) {
                    if (!n) {
                        goto fail;
                    }
                    value = lengths[n - 1];
                    repeat = 3 + bits_get(&bs, 2);
                } else if (sym == 17) {
                    repeat = 3 + bits_get(&bs, 3);
                } else {
                    repeat = 11 + bits_get(&bs, 7);
                }
                while (repeat-- > 0 && n < hlit + hdist) {
                    lengths[n++] = (unsigned char)value;
                }
            }
            huff_build(&lit, lengths, hlit);
            huff_build(&dist, lengths + hlit, hdist);
            use_lit = &lit;
            use_dist = &dist;
        } else {
            goto fail;
        }

        for (;;) {
            int sym = huff_decode(&bs, use_lit);

            if (sym < 0) {
                goto fail;
            }
            if (sym == 256) {
                break;
            }
            if (sym < 256) {
                if (grow_push(&out, (unsigned char)sym)) {
                    goto fail;
                }
                continue;
            }
            sym -= 257;
            if (sym >= 29) {
                goto fail;
            }
            {
                int length = LEN_BASE[sym] + bits_get(&bs, LEN_EXTRA[sym]);
                int dsym = huff_decode(&bs, use_dist);
                size_t distance;

                if (dsym < 0 || dsym >= 30) {
                    goto fail;
                }
                distance = (size_t)DIST_BASE[dsym]
                    + (size_t)bits_get(&bs, DIST_EXTRA[dsym]);
                if (distance > out.len) {
                    goto fail;
                }
                for (int i = 0; i < length; i++) {
                    if (grow_push(&out, out.data[out.len - distance])) {
                        goto fail;
                    }
                }
            }
        }
    } while (!final);

    *out_len = out.len;
    return out.data;

    fail:
    free(out.data);
    return NULL;
}

/* ===================== PNG READ ===================== */

static unsigned int be32(
    const unsigned char* p
)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16)
           | ((unsigned int)p[2] << 8) | p[3];
}

static int paeth(
    int a,
    int b,
    int c
)
{
    int p = a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);

    if (pa <= pb && pa <= pc) {
        return a;
    }
    return pb <= pc ? b : c;
}

struct image* png_read(
    const char* path
)
{
    FILE* f = fopen(path, "rb");
    unsigned char* file = NULL, * idat = NULL, * raw = NULL;
    unsigned char palette[256 * 3] = {0};
    unsigned char trns[256];
    struct image* im = NULL;
    size_t size, idat_len = 0, raw_len = 0, pos = 8;
    int w = 0, h = 0, depth = 0, colour = 0, channels = 0;
    int trns_count = 0;

    if (!f) {
        fprintf(stderr, "png_read: cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    {
        long got = ftell(f);

        if (got < 0) { /* The file gave no length. */
            fclose(f);
            return NULL;
        }
        size = (size_t)got;
    }
    fseek(f, 0, SEEK_SET);
    file = malloc(size);
    if (!file || fread(file, 1, size, f) != size) {
        fclose(f);
        free(file);
        return NULL;
    }
    fclose(f);
    memset(trns, 255, sizeof trns);

    if (size < 8 || memcmp(file, "\x89PNG\r\n\x1a\n", 8) != 0) {
        fprintf(stderr, "png_read: %s is not a PNG\n", path);
        free(file);
        return NULL;
    }

    while (pos + 8 <= size) {
        unsigned int len = be32(file + pos);
        const char* type = (const char*)file + pos + 4;
        const unsigned char* data = file + pos + 8;

        if (pos + 12 + len > size) {
            break;
        }
        if (!memcmp(type, "IHDR", 4)) {
            w = (int)be32(data);
            h = (int)be32(data + 4);
            depth = data[8];
            colour = data[9];
            if (data[12]) {
                fprintf(stderr, "png_read: %s is interlaced\n", path);
                goto done;
            }
        } else if (!memcmp(type, "PLTE", 4)) {
            memcpy(palette, data, len < sizeof palette ? len : sizeof palette);
        } else if (!memcmp(type, "tRNS", 4)) {
            trns_count = (int)len;
            memcpy(trns, data, len < sizeof trns ? len : sizeof trns);
        } else if (!memcmp(type, "IDAT", 4)) {
            unsigned char* bigger = realloc(idat, idat_len + len);

            if (!bigger) {
                goto done;
            }
            idat = bigger;
            memcpy(idat + idat_len, data, len);
            idat_len += len;
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + len;
    }

    if (!idat || depth != 8) {
        fprintf(stderr, "png_read: %s needs 8-bit samples\n", path);
        goto done;
    }

    /*
     * The five colour types the format defines. The reader takes
     * the number of channels from this value and reads that many
     * bytes for each pixel, so a type it does not know would read
     * past the end of the line.
     */
    if (colour != 0 && colour != 2 && colour != 3 && colour != 4
        && colour != 6) {
        fprintf(stderr, "png_read: %s holds colour type %d\n", path, colour);
        goto done;
    }
    channels = colour == 0   ? 1
               : colour == 2 ? 3
               : colour == 3 ? 1
               : colour == 4 ? 2
                             : 4;

    raw = inflate_zlib(idat, idat_len, &raw_len);
    if (!raw) {
        fprintf(stderr, "png_read: %s failed to inflate\n", path);
        goto done;
    }

    im = img_new(w, h);
    if (!im) {
        goto done;
    }

    {
        size_t stride = (size_t)w * channels;
        unsigned char* prev = calloc(stride, 1);
        unsigned char* line = calloc(stride, 1);

        if (!prev || !line) {
            img_free(im);
            im = NULL;
            free(prev);
            free(line);
            goto done;
        }
        for (int y = 0; y < h; y++) {
            size_t offset = (size_t)y * (stride + 1);
            int filter;

            if (offset + stride >= raw_len + 1) {
                break;
            }
            filter = raw[offset];
            memcpy(line, raw + offset + 1, stride);

            for (size_t i = 0; i < stride; i++) {
                int a = i >= (size_t)channels ? line[i - channels] : 0;
                int b = prev[i];
                int c = i >= (size_t)channels ? prev[i - channels] : 0;

                switch (filter) {
                    case 1:
                        line[i] = (unsigned char)(line[i] + a);
                        break;
                    case 2:
                        line[i] = (unsigned char)(line[i] + b);
                        break;
                    case 3:
                        line[i] = (unsigned char)(line[i] + (a + b) / 2);
                        break;
                    case 4:
                        line[i] = (unsigned char)(line[i] + paeth(a, b, c));
                        break;
                    default:
                        break;
                }
            }

            for (int x = 0; x < w; x++) {
                unsigned char* p = im->px + ((size_t)y * w + x) * 4;
                const unsigned char* s = line + (size_t)x * channels;

                switch (colour) {
                    case 0:
                        p[0] = p[1] = p[2] = s[0];
                        p[3] = 255;
                        break;
                    case 2:
                        p[0] = s[0];
                        p[1] = s[1];
                        p[2] = s[2];
                        p[3] = 255;
                        break;
                    case 3: {
                            int idx = s[0];

                            p[0] = palette[idx * 3];
                            p[1] = palette[idx * 3 + 1];
                            p[2] = palette[idx * 3 + 2];
                            p[3] = idx < trns_count ? trns[idx] : 255;
                            break;
                        }
                    case 4:
                        p[0] = p[1] = p[2] = s[0];
                        p[3] = s[1];
                        break;
                    default:
                        p[0] = s[0];
                        p[1] = s[1];
                        p[2] = s[2];
                        p[3] = s[3];
                        break;
                }
            }
            memcpy(prev, line, stride);
        }
        free(prev);
        free(line);
    }

    done:
    free(file);
    free(idat);
    free(raw);
    return im;
}

/* ===================== PNG WRITE ===================== */

static unsigned int crc_table[256];
static int crc_ready;

static unsigned int crc32_of(
    const unsigned char* data,
    size_t len,
    unsigned int crc
)
{
    if (!crc_ready) {
        for (unsigned int i = 0; i < 256; i++) {
            unsigned int c = i;

            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            crc_table[i] = c;
        }
        crc_ready = 1;
    }
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

static void put32(
    unsigned char* p,
    unsigned int v
)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void chunk(
    FILE* f,
    const char* type,
    const unsigned char* data,
    size_t len
)
{
    unsigned char head[8];
    unsigned char tail[4];
    unsigned int crc;

    put32(head, (unsigned int)len);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (len) {
        fwrite(data, 1, len, f);
    }

    crc = crc32_of((const unsigned char*)type, 4, 0);
    if (len) {
        crc = crc32_of(data, len, crc);
    }
    put32(tail, crc);
    fwrite(tail, 1, 4, f);
}

/* --- DEFLATE ---------------------------------------------------------- */

/*
 * Compress with the fixed Huffman table of RFC 1951. A dynamic table
 * would squeeze a little further, but it costs a second pass and a
 * tree in the output, and the images here are flat colour that the
 * matcher already handles well.
 */

/* The output bit stream. DEFLATE fills each byte from the low bit up. */
struct bits
{
    unsigned char* buf;
    size_t cap, len;
    unsigned int acc;
    int count;
};

static int bits_byte(
    struct bits* b,
    unsigned char v
)
{
    if (b->len == b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        unsigned char* grown = realloc(b->buf, cap);

        if (!grown) {
            return -1;
        }
        b->buf = grown;
        b->cap = cap;
    }
    b->buf[b->len++] = v;
    return 0;
}

static int put_bits(
    struct bits* b,
    unsigned int value,
    int n
)
{
    b->acc |= (value & ((1u << n) - 1)) << b->count;
    b->count += n;
    while (b->count >= 8) {
        if (bits_byte(b, (unsigned char)(b->acc & 0xFF))) {
            return -1;
        }
        b->acc >>= 8;
        b->count -= 8;
    }
    return 0;
}

/* A Huffman code travels most significant bit first. */
static int put_code(
    struct bits* b,
    unsigned int code,
    int n
)
{
    for (int i = n - 1; i >= 0; i--) {
        if (put_bits(b, (code >> i) & 1, 1)) {
            return -1;
        }
    }
    return 0;
}

/* The fixed table gives each literal its length and its code. */
static int put_literal(
    struct bits* b,
    unsigned int sym
)
{
    if (sym < 144) {
        return put_code(b, 0x30 + sym, 8);
    }
    if (sym < 256) {
        return put_code(b, 0x190 + sym - 144, 9);
    }
    if (sym < 280) {
        return put_code(b, sym - 256, 7);
    }
    return put_code(b, 0xC0 + sym - 280, 8);
}

#define HASH_BITS 15
#define HASH_SIZE (1 << HASH_BITS)
#define WINDOW    32768
#define MIN_MATCH 3
#define MAX_MATCH 258
#define MAX_CHAIN 64 /* How far back one hash chain is followed. */

static unsigned int hash3(
    const unsigned char* p
)
{
    return (((unsigned int)p[0] << 16) ^ ((unsigned int)p[1] << 8) ^ p[2])
           * 2654435761u
           >> (32 - HASH_BITS);
}

/*
 * Compress `src` into a zlib stream. The caller owns the result and
 * receives its length in `*out_len`.
 */
static unsigned char* deflate_fixed(
    const unsigned char* src,
    size_t n,
    size_t* out_len
)
{
    struct bits b = {NULL, 0, 0, 0, 0};
    int* head = malloc(sizeof *head * HASH_SIZE);
    int* prev = malloc(sizeof *prev * (n ? n : 1));
    unsigned int s1 = 1, s2 = 0;
    size_t i = 0;

    if (!head || !prev) {
        free(head);
        free(prev);
        return NULL;
    }
    for (int k = 0; k < HASH_SIZE; k++) {
        head[k] = -1;
    }

    /* zlib header: deflate, 32K window, default level, no dictionary. */
    if (bits_byte(&b, 0x78) || bits_byte(&b, 0x9C)) {
        goto fail;
    }
    /* One final block, fixed Huffman. */
    if (put_bits(&b, 1, 1) || put_bits(&b, 1, 2)) {
        goto fail;
    }

    while (i < n) {
        size_t best_len = 0, best_dist = 0;

        if (i + MIN_MATCH <= n) {
            unsigned int h = hash3(src + i);
            int cand = head[h];

            for (int chain = 0; cand >= 0 && chain < MAX_CHAIN; chain++) {
                size_t dist = i - (size_t)cand;
                size_t len = 0;
                size_t limit = n - i;

                if (dist == 0 || dist > WINDOW) {
                    break;
                }
                if (limit > MAX_MATCH) {
                    limit = MAX_MATCH;
                }
                while (len < limit && src[cand + len] == src[i + len]) {
                    len++;
                }
                if (len > best_len) {
                    best_len = len;
                    best_dist = dist;
                    if (len == MAX_MATCH) {
                        break;
                    }
                }
                cand = prev[cand];
            }
            prev[i] = head[h];
            head[h] = (int)i;
        }

        if (best_len >= MIN_MATCH) {
            int lc = 0, dc = 0;

            while (lc < 28 && LEN_BASE[lc + 1] <= best_len) {
                lc++;
            }
            while (dc < 29 && DIST_BASE[dc + 1] <= best_dist) {
                dc++;
            }
            if (put_literal(&b, (unsigned int)(257 + lc))
                || put_bits(
                    &b,
                    (unsigned int)(best_len - LEN_BASE[lc]),
                    LEN_EXTRA[lc]
                )
                || put_code(&b, (unsigned int)dc, 5)
                || put_bits(
                    &b,
                    (unsigned int)(best_dist - DIST_BASE[dc]),
                    DIST_EXTRA[dc]
                )) {
                goto fail;
            }
            /* Register the bytes the match covered. */
            for (size_t k = i + 1; k < i + best_len; k++) {
                if (k + MIN_MATCH <= n) {
                    unsigned int h = hash3(src + k);

                    prev[k] = head[h];
                    head[h] = (int)k;
                }
            }
            i += best_len;
        } else {
            if (put_literal(&b, src[i])) {
                goto fail;
            }
            i++;
        }
    }
    if (put_literal(&b, 256)) { /* End of block. */
        goto fail;
    }
    while (b.count) { /* Pad out the last byte. */
        if (put_bits(&b, 0, 1)) {
            goto fail;
        }
    }

    for (size_t k = 0; k < n; k++) {
        s1 = (s1 + src[k]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    if (bits_byte(&b, (unsigned char)(s2 >> 8))
        || bits_byte(&b, (unsigned char)(s2 & 0xFF))
        || bits_byte(&b, (unsigned char)(s1 >> 8))
        || bits_byte(&b, (unsigned char)(s1 & 0xFF))) {
        goto fail;
    }

    free(head);
    free(prev);
    *out_len = b.len;
    return b.buf;

    fail:
    free(head);
    free(prev);
    free(b.buf);
    return NULL;
}

/*
 * Choose a filter for one row. Each of the five is scored by the sum
 * of its bytes read as signed values, which is the heuristic that the
 * PNG specification recommends: the filter that leaves the smallest
 * sum leaves the least for the compressor to carry.
 */
static int choose_filter(
    const unsigned char* row,
    const unsigned char* up,
    size_t width_bytes,
    int bpp,
    unsigned char* out
)
{
    long best_score = -1;
    int best = 0;

    for (int f = 0; f < 5; f++) {
        long score = 0;

        for (size_t i = 0; i < width_bytes; i++) {
            int a = i >= (size_t)bpp ? row[i - bpp] : 0;
            int b = up ? up[i] : 0;
            int c = (up && i >= (size_t)bpp) ? up[i - bpp] : 0;
            int v;

            switch (f) {
                case 0:
                    v = row[i];
                    break;
                case 1:
                    v = row[i] - a;
                    break;
                case 2:
                    v = row[i] - b;
                    break;
                case 3:
                    v = row[i] - ((a + b) >> 1);
                    break;
                default: {
                        int p = a + b - c;
                        int pa = p > a ? p - a : a - p;
                        int pb = p > b ? p - b : b - p;
                        int pc = p > c ? p - c : c - p;
                        int pred = (pa <= pb &&
                            pa <= pc) ? a : (pb <= pc ? b : c);

                        v = row[i] - pred;
                        break;
                    }
            }

            /* The specification scores the byte as a
               signed value, so 255 counts as one. */
            v = (signed char)v;
            score += v < 0 ? -v : v;
        }
        if (best_score < 0 || score < best_score) {
            best_score = score;
            best = f;
        }
    }

    /* Apply the winner. */
    for (size_t i = 0; i < width_bytes; i++) {
        int a = i >= (size_t)bpp ? row[i - bpp] : 0;
        int b = up ? up[i] : 0;
        int c = (up && i >= (size_t)bpp) ? up[i - bpp] : 0;

        switch (best) {
            case 0:
                out[i] = row[i];
                break;
            case 1:
                out[i] = (unsigned char)(row[i] - a);
                break;
            case 2:
                out[i] = (unsigned char)(row[i] - b);
                break;
            case 3:
                out[i] = (unsigned char)(row[i] - ((a + b) >> 1));
                break;
            default: {
                    int p = a + b - c;
                    int pa = p > a ? p - a : a - p;
                    int pb = p > b ? p - b : b - p;
                    int pc = p > c ? p - c : c - p;

                    out[i] = (unsigned char)(row[i]
                        - ((pa <= pb && pa <= pc)
                                                ? a
                                                : (pb <= pc ? b : c)));
                    break;
                }
        }
    }
    return best;
}

/*
 * Write the image. Each row takes the filter that flattens it best,
 * and the result goes through the deflate above.
 */
int png_write(
    const char* path,
    const struct image* im
)
{
    FILE* f = fopen(path, "wb");
    unsigned char ihdr[13];
    unsigned char* raw, * z;
    size_t width_bytes = (size_t)im->w * 4;
    size_t stride = width_bytes + 1;
    size_t raw_len = stride * (size_t)im->h;
    size_t z_len = 0;

    if (!f) {
        return -1;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);

    put32(ihdr, (unsigned int)im->w);
    put32(ihdr + 4, (unsigned int)im->h);
    ihdr[8] = 8; /* bit depth */
    ihdr[9] = 6; /* RGBA */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, sizeof ihdr);

    raw = malloc(raw_len);
    if (!raw) {
        fclose(f);
        return -1;
    }
    for (int y = 0; y < im->h; y++) {
        const unsigned char* row = im->px + (size_t)y * width_bytes;
        const unsigned char* up = y ? im->px + (size_t)(y - 1) * width_bytes
                                    : NULL;
        unsigned char* dst = raw + (size_t)y * stride;

        dst[0] = (unsigned char)choose_filter(row, up, width_bytes, 4, dst + 1);
    }

    z = deflate_fixed(raw, raw_len, &z_len);
    free(raw);
    if (!z) {
        fclose(f);
        return -1;
    }

    chunk(f, "IDAT", z, z_len);
    chunk(f, "IEND", NULL, 0);

    free(z);
    fclose(f);
    return 0;
}

/* ===================== SCALING ===================== */

static double lanczos(
    double x,
    double a
)
{
    if (x == 0.0) {
        return 1.0;
    }
    if (x < -a || x > a) {
        return 0.0;
    }
    x *= M_PI;
    return a * sin(x) * sin(x / a) / (x * x);
}

struct image* img_scale(
    const struct image* src,
    int w,
    int h
)
{
    struct image* out = img_new(w, h);
    const double a = 3.0;

    if (!out) {
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        double sy = ((double)y + 0.5) * src->h / h - 0.5;
        double scale_y = (double)src->h / h;
        double support_y = scale_y > 1.0 ? a * scale_y : a;

        for (int x = 0; x < w; x++) {
            double sx = ((double)x + 0.5) * src->w / w - 0.5;
            double scale_x = (double)src->w / w;
            double support_x = scale_x > 1.0 ? a * scale_x : a;
            double sum[4] = {0, 0, 0, 0}, weight = 0.0;
            int y0 = (int)ceil(sy - support_y);
            int y1 = (int)floor(sy + support_y);
            int x0 = (int)ceil(sx - support_x);
            int x1 = (int)floor(sx + support_x);

            for (int j = y0; j <= y1; j++) {
                int cj = j < 0 ? 0 : j >= src->h ? src->h - 1 : j;
                double wy = lanczos(
                    scale_y > 1.0 ? (j - sy) / scale_y : j - sy,
                    a
                );

                for (int i = x0; i <= x1; i++) {
                    int ci = i < 0 ? 0 : i >= src->w ? src->w - 1 : i;
                    double wx = lanczos(
                        scale_x > 1.0 ? (i - sx) / scale_x : i - sx,
                        a
                    );
                    double k = wx * wy;
                    const unsigned char* p = src->px
                        + ((size_t)cj * src->w + ci) * 4;
                    double alpha = p[3] / 255.0;

                    /* Weight colour by alpha, so a
                       transparent edge does not bleed. */
                    sum[0] += p[0] * alpha * k;
                    sum[1] += p[1] * alpha * k;
                    sum[2] += p[2] * alpha * k;
                    sum[3] += p[3] * k;
                    weight += k;
                }
            }
            {
                unsigned char* d = out->px + ((size_t)y * w + x) * 4;
                double alpha = weight != 0.0 ? sum[3] / weight : 0.0;

                if (alpha < 0.0) {
                    alpha = 0.0;
                }
                if (alpha > 255.0) {
                    alpha = 255.0;
                }
                d[3] = (unsigned char)(alpha + 0.5);
                for (int c = 0; c < 3; c++) {
                    double v = 0.0;

                    if (weight != 0.0 && alpha > 0.5) {
                        v = sum[c] / weight / (alpha / 255.0);
                    }
                    if (v < 0.0) {
                        v = 0.0;
                    }
                    if (v > 255.0) {
                        v = 255.0;
                    }
                    d[c] = (unsigned char)(v + 0.5);
                }
            }
        }
    }
    return out;
}

/* ===================== GIF WRITE ===================== */

/*
 * A shared palette for every frame, built by median cut. The generator
 * gathers the distinct colours of every frame, then it divides that set
 * at the median of its widest channel until it holds 64 groups. Each
 * group gives one palette entry, which is the average of its colours,
 * weighted by how many pixels carry each one.
 */
#define GIF_COLOURS 64

/* One distinct colour, and the number of pixels that carry it. */
struct swatch
{
    unsigned char r, g, b;
    long n;
};

/* A group of swatches, held as a half-open range of the sorted array. */
struct group
{
    int lo, hi;
    long n; /* The pixels that the range covers. */
};

/* Find the channel that spans the widest range over one group. */
static int widest_channel(
    const struct swatch* s,
    const struct group* g,
    int* span
)
{
    int lo[3] = {255, 255, 255};
    int hi[3] = {0, 0, 0};
    int best = 0;

    for (int i = g->lo; i < g->hi; i++) {
        const unsigned char c[3] = {s[i].r, s[i].g, s[i].b};

        for (int k = 0; k < 3; k++) {
            if (c[k] < lo[k]) {
                lo[k] = c[k];
            }
            if (c[k] > hi[k]) {
                hi[k] = c[k];
            }
        }
    }
    *span = hi[0] - lo[0];
    for (int k = 1; k < 3; k++) {
        if (hi[k] - lo[k] > *span) {
            *span = hi[k] - lo[k];
            best = k;
        }
    }
    return best;
}

/*
 * The channel that the current split sorts on. The standard comparison
 * function takes no context of its own, so the key travels here.
 */
static int sort_channel;

static int by_channel(
    const void* a,
    const void* b
)
{
    const struct swatch* x = a, * y = b;
    const unsigned char xc[3] = {x->r, x->g, x->b};
    const unsigned char yc[3] = {y->r, y->g, y->b};

    return xc[sort_channel] - yc[sort_channel];
}

/*
 * Collect the distinct colours of every frame, with their counts. The
 * hash table holds four slots for each pixel, so it stays sparse and
 * open addressing terminates.
 */
static struct swatch* histogram(
    struct image* const* frames,
    int count,
    int w,
    int h,
    int* out_n
)
{
    size_t total = (size_t)count * w * h;
    size_t cap = 1024;
    int* slot;
    struct swatch* sw;
    int n = 0;

    while (cap < total * 4) {
        cap *= 2;
    }
    slot = malloc(sizeof *slot * cap);
    sw = malloc(sizeof *sw * (total ? total : 1));
    if (!slot || !sw) {
        free(slot);
        free(sw);
        return NULL;
    }
    for (size_t i = 0; i < cap; i++) {
        slot[i] = -1;
    }

    for (int f = 0; f < count; f++) {
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const unsigned char* p = frames[f]->px + i * 4;
            unsigned int key = ((unsigned int)p[0] << 16)
                | ((unsigned int)p[1] << 8) | p[2];

            /* The product wraps at 32 bits, which is
               what scatters the colours. */
            size_t probe = (size_t)(key * 2654435761u) & (cap - 1);

            while (slot[probe] >= 0
                && (sw[slot[probe]].r != p[0] || sw[slot[probe]].g != p[1]
                || sw[slot[probe]].b != p[2])) {
                probe = (probe + 1) & (cap - 1);
            }

            if (slot[probe] >= 0) {
                sw[slot[probe]].n++;
                continue;
            }
            sw[n].r = p[0];
            sw[n].g = p[1];
            sw[n].b = p[2];
            sw[n].n = 1;
            slot[probe] = n++;
        }
    }

    free(slot);
    *out_n = n;
    return sw;
}

/* Build the shared palette. Returns the number of entries it holds. */
static int build_palette(
    struct image* const* frames,
    int count,
    int w,
    int h,
    unsigned char* pal
)
{
    struct group groups[GIF_COLOURS];
    struct swatch* sw;
    int swatches = 0;
    int n_groups = 1;

    sw = histogram(frames, count, w, h, &swatches);
    if (!sw || swatches < 1) {
        free(sw);
        return -1;
    }

    groups[0].lo = 0;
    groups[0].hi = swatches;
    groups[0].n = 0;
    for (int i = 0; i < swatches; i++) {
        groups[0].n += sw[i].n;
    }

    /*
     * Divide the group that spans the widest channel, at the median
     * of that channel, until the palette is full. The median keeps
     * the same number of pixels on each side, so a crowded region of
     * the colour cube receives more entries than an empty one.
     */
    while (n_groups < GIF_COLOURS) {
        int pick = -1, pick_span = 0, pick_ch = 0;
        long half, run;
        int cut;

        for (int i = 0; i < n_groups; i++) {
            int span, ch;

            if (groups[i].hi - groups[i].lo < 2) {
                continue;
            }
            ch = widest_channel(sw, &groups[i], &span);
            if (span > pick_span) {
                pick_span = span;
                pick = i;
                pick_ch = ch;
            }
        }
        if (pick < 0) {
            break;
        }

        sort_channel = pick_ch;
        qsort(
            sw + groups[pick].lo,
            (size_t)(groups[pick].hi - groups[pick].lo),
            sizeof *sw,
            by_channel
        );

        half = groups[pick].n / 2;
        run = 0;
        cut = groups[pick].lo;
        while (cut < groups[pick].hi - 1 && run + sw[cut].n <= half) {
            run += sw[cut].n;
            cut++;
        }
        /* Both sides must keep at least one swatch. */
        if (cut == groups[pick].lo) {
            cut++;
        }

        groups[n_groups].lo = cut;
        groups[n_groups].hi = groups[pick].hi;
        groups[pick].hi = cut;
        for (int i = 0; i < 2; i++) {
            struct group* g = i ? &groups[n_groups] : &groups[pick];

            g->n = 0;
            for (int k = g->lo; k < g->hi; k++) {
                g->n += sw[k].n;
            }
        }
        n_groups++;
    }

    for (int i = 0; i < n_groups; i++) {
        long r = 0, g = 0, b = 0;
        long n = groups[i].n ? groups[i].n : 1;

        for (int k = groups[i].lo; k < groups[i].hi; k++) {
            r += (long)sw[k].r * sw[k].n;
            g += (long)sw[k].g * sw[k].n;
            b += (long)sw[k].b * sw[k].n;
        }
        pal[i * 3] = (unsigned char)(r / n);
        pal[i * 3 + 1] = (unsigned char)(g / n);
        pal[i * 3 + 2] = (unsigned char)(b / n);
    }

    free(sw);
    return n_groups;
}

static int nearest(
    const unsigned char* pal,
    int count,
    const unsigned char* p
)
{
    int best = 0;
    long best_d = -1;

    for (int i = 0; i < count; i++) {
        long dr = (long)pal[i * 3] - p[0];
        long dg = (long)pal[i * 3 + 1] - p[1];
        long db = (long)pal[i * 3 + 2] - p[2];
        long d = dr * dr + dg * dg + db * db;

        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

/* Emit one LZW code into the packed byte stream. */
struct lzw_out
{
    FILE* f;
    unsigned char block[255];
    int block_len;
    unsigned int bit_buf;
    int bit_count;
};

static void lzw_flush_block(
    struct lzw_out* o
)
{
    if (!o->block_len) {
        return;
    }
    fputc(o->block_len, o->f);
    fwrite(o->block, 1, (size_t)o->block_len, o->f);
    o->block_len = 0;
}

static void lzw_put(
    struct lzw_out* o,
    int code,
    int width
)
{
    o->bit_buf |= (unsigned int)code << o->bit_count;
    o->bit_count += width;
    while (o->bit_count >= 8) {
        o->block[o->block_len++] = (unsigned char)(o->bit_buf & 0xFF);
        o->bit_buf >>= 8;
        o->bit_count -= 8;
        if (o->block_len == 255) {
            lzw_flush_block(o);
        }
    }
}

static void lzw_encode(
    FILE* f,
    const unsigned char* indices,
    size_t len,
    int min_code_size
)
{
    struct lzw_out o = {f, {0}, 0, 0, 0};
    int clear = 1 << min_code_size;
    int end = clear + 1;
    int next = end + 1;
    int width = min_code_size + 1;
    /* A dictionary keyed by (prefix, byte). */
    static int table[4096][256];
    int prefix;

    fputc(min_code_size, f);
    memset(table, 0xFF, sizeof table);
    lzw_put(&o, clear, width);

    if (!len) {
        lzw_put(&o, end, width);
        if (o.bit_count) {
            o.block[o.block_len++] = (unsigned char)o.bit_buf;
        }
        lzw_flush_block(&o);
        fputc(0, f);
        return;
    }

    prefix = indices[0];
    for (size_t i = 1; i < len; i++) {
        int c = indices[i];

        if (table[prefix][c] >= 0) {
            prefix = table[prefix][c];
            continue;
        }
        lzw_put(&o, prefix, width);
        if (next < 4096) {
            table[prefix][c] = next++;
            if (next - 1 == (1 << width) && width < 12) {
                width++;
            }
        } else {
            lzw_put(&o, clear, width);
            memset(table, 0xFF, sizeof table);
            next = end + 1;
            width = min_code_size + 1;
        }
        prefix = c;
    }
    lzw_put(&o, prefix, width);
    lzw_put(&o, end, width);
    if (o.bit_count) {
        o.block[o.block_len++] = (unsigned char)o.bit_buf;
    }
    lzw_flush_block(&o);
    fputc(0, f);
}

int gif_write(
    const char* path,
    struct image* const* frames,
    int count,
    int delay_cs
)
{
    FILE* f;
    unsigned char pal[GIF_COLOURS * 3] = {0};
    int n_colours;
    int w, h;
    unsigned char* indices;

    if (count < 1) {
        return -1;
    }
    w = frames[0]->w;
    h = frames[0]->h;

    n_colours = build_palette(frames, count, w, h, pal);
    if (n_colours < 1) {
        return -1;
    }

    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    fwrite("GIF89a", 1, 6, f);
    fputc(w & 0xFF, f);
    fputc((w >> 8) & 0xFF, f);
    fputc(h & 0xFF, f);
    fputc((h >> 8) & 0xFF, f);
    fputc(0xF0 | 5, f); /* global table, 64 entries */
    fputc(0, f);
    fputc(0, f);
    fwrite(pal, 1, sizeof pal, f);

    /* Netscape block: loop for ever. */
    fputc(0x21, f);
    fputc(0xFF, f);
    fputc(11, f);
    fwrite("NETSCAPE2.0", 1, 11, f);
    fputc(3, f);
    fputc(1, f);
    fputc(0, f);
    fputc(0, f);
    fputc(0, f);

    indices = malloc((size_t)w * h);
    if (!indices) {
        fclose(f);
        return -1;
    }

    for (int n = 0; n < count; n++) {
        size_t pixels = (size_t)w * h;

        fputc(0x21, f); /* graphic control extension */
        fputc(0xF9, f);
        fputc(4, f);
        fputc(0x04, f); /* disposal: leave in place */
        fputc(delay_cs & 0xFF, f);
        fputc((delay_cs >> 8) & 0xFF, f);
        fputc(0, f);
        fputc(0, f);

        fputc(0x2C, f); /* image descriptor */
        fputc(0, f);
        fputc(0, f);
        fputc(0, f);
        fputc(0, f);
        fputc(w & 0xFF, f);
        fputc((w >> 8) & 0xFF, f);
        fputc(h & 0xFF, f);
        fputc((h >> 8) & 0xFF, f);
        fputc(0, f);

        for (size_t i = 0; i < pixels; i++) {
            indices[i] = (unsigned char)
                nearest(pal, n_colours, frames[n]->px + i * 4);
        }
        lzw_encode(f, indices, pixels, 6);
    }

    fputc(0x3B, f);
    free(indices);
    fclose(f);
    return 0;
}
