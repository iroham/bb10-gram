/* PBKDF2-HMAC-SHA256 for the app lock PIN (Qt 4.8 has no SHA-256). SHA-256 per FIPS 180-4. */
#include "pinhash.h"
#include <string.h>

typedef struct { unsigned int h[8]; unsigned char buf[64]; unsigned long long len; unsigned int n; } Sha;

static const unsigned int K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void block(Sha *s, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h, i;
    for (i = 0; i < 16; i++) w[i] = (unsigned int)p[i * 4] << 24 | (unsigned int)p[i * 4 + 1] << 16 | (unsigned int)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        unsigned int s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        unsigned int t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        unsigned int t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void init(Sha *s) {
    static const unsigned int h0[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    memcpy(s->h, h0, sizeof h0); s->len = 0; s->n = 0;
}
static void update(Sha *s, const unsigned char *p, size_t n) {
    s->len += n;
    while (n) {
        size_t k = 64 - s->n; if (k > n) k = n;
        memcpy(s->buf + s->n, p, k); s->n += (unsigned int)k; p += k; n -= k;
        if (s->n == 64) { block(s, s->buf); s->n = 0; }
    }
}
static void final(Sha *s, unsigned char out[32]) {
    unsigned long long bits = s->len * 8;
    unsigned char pad = 0x80, z = 0, l[8];
    int i;
    update(s, &pad, 1);
    while (s->n != 56) update(s, &z, 1);
    for (i = 0; i < 8; i++) l[i] = (unsigned char)(bits >> (56 - 8 * i));
    update(s, l, 8);
    for (i = 0; i < 8; i++) { out[i * 4] = s->h[i] >> 24; out[i * 4 + 1] = s->h[i] >> 16; out[i * 4 + 2] = s->h[i] >> 8; out[i * 4 + 3] = s->h[i]; }
}

void gram_sha256(const unsigned char *p, size_t n, unsigned char out[32]) { Sha s; init(&s); update(&s, p, n); final(&s, out); }

static void hmac(const unsigned char *key, size_t kn, const unsigned char *a, size_t an, const unsigned char *b, size_t bn, unsigned char out[32]) {
    unsigned char k[64], ip[64], op[64], inner[32];
    Sha s;
    int i;
    memset(k, 0, 64);
    if (kn > 64) gram_sha256(key, kn, k); else memcpy(k, key, kn);
    for (i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    init(&s); update(&s, ip, 64); update(&s, a, an); if (bn) update(&s, b, bn); final(&s, inner);
    init(&s); update(&s, op, 64); update(&s, inner, 32); final(&s, out);
}

void gram_pbkdf2_sha256(const unsigned char *pass, size_t pn, const unsigned char *salt, size_t sn, int iter, unsigned char *out, size_t outLen) {
    unsigned int blockNo = 1;
    while (outLen) {
        unsigned char ctr[4] = { (unsigned char)(blockNo >> 24), (unsigned char)(blockNo >> 16), (unsigned char)(blockNo >> 8), (unsigned char)blockNo };
        unsigned char u[32], t[32];
        size_t k;
        int i, j;
        hmac(pass, pn, salt, sn, ctr, 4, u);
        memcpy(t, u, 32);
        for (i = 1; i < iter; i++) { hmac(pass, pn, u, 32, 0, 0, u); for (j = 0; j < 32; j++) t[j] ^= u[j]; }
        k = outLen < 32 ? outLen : 32;
        memcpy(out, t, k); out += k; outLen -= k; blockNo++;
    }
}
