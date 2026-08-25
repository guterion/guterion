/*
 * tools/qr.h
 * @guterion
 * CC-BY-SA-4.0
 * Build the QR matrix that carries a payment address
 */

#ifndef QR_H
#define QR_H

/* A finished symbol. `m` holds size*size modules, one byte each: 1 is
   dark. The caller owns it and frees it with qr_free. */
struct qr
{
    int size;
    unsigned char* m;
};

/*
 * Encode the text as a QR symbol, in byte mode with the L level of
 * error correction. Returns NULL when the text needs more room than
 * version 5 holds, which is 108 bytes.
 */
struct qr* qr_make(
    const char* text
);
void qr_free(
    struct qr* q
);

#endif
