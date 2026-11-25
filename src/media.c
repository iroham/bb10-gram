/* media.c: formats Telegram uses that BB10 cannot show or play by itself.
 *   opus_to_wav: voice messages (Ogg/Opus) -> 16-bit PCM WAV, 48 kHz, for bb.multimedia.
 *   webp_to_png: stickers and their thumbnails (WebP) -> PNG, for Cascades images.
 * Both write to a temporary name and rename, so a half-written file is never used. Return 0 on success. */
#include "media.h"
#include <opus.h>
#include <webp/decode.h>
#include <zlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 1 << 16, n = 0, r;
    unsigned char *b = malloc(cap);
    while (b && (r = fread(b + n, 1, cap - n, f)) > 0) {
        n += r;
        if (n == cap) { unsigned char *nb = realloc(b, cap * 2); if (!nb) { free(b); b = NULL; break; } b = nb; cap *= 2; }
    }
    fclose(f);
    *len = n;
    return b;
}
static void put32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(unsigned char *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

int opus_to_wav(const char *in, const char *out) {
    size_t len, pos = 0;
    unsigned char *d = read_all(in, &len);
    if (!d) return -1;
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.part", out);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(d); return -1; }
    unsigned char hdr[44] = {0};
    fwrite(hdr, 1, 44, f);                                   /* filled in at the end */
    OpusDecoder *dec = NULL;
    int channels = 1, preskip = 0, packets = 0, err = 0;
    uint32_t frames_out = 0;
    unsigned char *pkt = malloc(1 << 16);
    size_t pktlen = 0;
    static opus_int16 pcm[5760 * 2];
    while (pos + 27 <= len && !err) {
        unsigned char *pg = d + pos;
        if (memcmp(pg, "OggS", 4)) { err = 1; break; }
        int nseg = pg[26];
        if (pos + 27 + nseg > len) break;
        size_t body = pos + 27 + nseg;
        for (int i = 0; i < nseg; i++) {
            int l = pg[27 + i];
            if (body + l > len || pktlen + l > (1 << 16)) { err = 1; break; }
            memcpy(pkt + pktlen, d + body, l);
            pktlen += l; body += l;
            if (l == 255) continue;                            /* the packet goes on */
            if (packets == 0) {                                /* OpusHead */
                if (pktlen < 19 || memcmp(pkt, "OpusHead", 8)) { err = 1; break; }
                channels = pkt[9] > 1 ? 2 : 1;
                preskip = pkt[10] | pkt[11] << 8;
                int e;
                dec = opus_decoder_create(48000, channels, &e);
                if (!dec) { err = 1; break; }
            } else if (packets > 1) {                          /* packet 1 = OpusTags */
                int n = opus_decode(dec, pkt, pktlen, pcm, 5760, 0);
                if (n > 0) {
                    int skip = preskip < n ? preskip : n;
                    preskip -= skip;
                    fwrite(pcm + skip * channels, 2 * channels, n - skip, f);   /* little-endian host */
                    frames_out += n - skip;
                }
            }
            packets++;
            pktlen = 0;
        }
        pos = body;
    }
    free(pkt); free(d);
    if (dec) opus_decoder_destroy(dec);
    if (err || !frames_out) { fclose(f); remove(tmp); return -1; }
    uint32_t bytes = frames_out * 2 * channels;
    memcpy(hdr, "RIFF", 4); put32(hdr + 4, 36 + bytes); memcpy(hdr + 8, "WAVEfmt ", 8);
    put32(hdr + 16, 16); put16(hdr + 20, 1); put16(hdr + 22, channels); put32(hdr + 24, 48000);
    put32(hdr + 28, 48000 * 2 * channels); put16(hdr + 32, 2 * channels); put16(hdr + 34, 16);
    memcpy(hdr + 36, "data", 4); put32(hdr + 40, bytes);
    fseek(f, 0, SEEK_SET);
    fwrite(hdr, 1, 44, f);
    if (fclose(f)) { remove(tmp); return -1; }
    return rename(tmp, out);
}

static void chunk(FILE *f, const char *type, const unsigned char *data, uint32_t n) {
    unsigned char b[4];
    b[0] = n >> 24; b[1] = n >> 16; b[2] = n >> 8; b[3] = n;
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (n) fwrite(data, 1, n, f);
    uLong c = crc32(0, (const Bytef *)type, 4);
    if (n) c = crc32(c, data, n);
    b[0] = c >> 24; b[1] = c >> 16; b[2] = c >> 8; b[3] = c;
    fwrite(b, 1, 4, f);
}

int webp_to_png(const char *in, const char *out, int max_side) {
    size_t len;
    unsigned char *d = read_all(in, &len);
    if (!d) return -1;
    int w, h;
    if (!WebPGetInfo(d, len, &w, &h)) { free(d); return -1; }
    WebPDecoderConfig cfg;
    WebPInitDecoderConfig(&cfg);
    if (max_side > 0 && (w > max_side || h > max_side)) {
        cfg.options.use_scaling = 1;
        if (w >= h) { cfg.options.scaled_width = max_side; cfg.options.scaled_height = h * max_side / w; }
        else { cfg.options.scaled_height = max_side; cfg.options.scaled_width = w * max_side / h; }
        if (cfg.options.scaled_width < 1) cfg.options.scaled_width = 1;
        if (cfg.options.scaled_height < 1) cfg.options.scaled_height = 1;
    }
    cfg.output.colorspace = MODE_RGBA;
    if (WebPDecode(d, len, &cfg) != VP8_STATUS_OK) { free(d); return -1; }
    free(d);
    w = cfg.output.width; h = cfg.output.height;
    size_t stride = (size_t)w * 4, raw = (stride + 1) * h;
    unsigned char *rows = malloc(raw);
    uLongf zn = compressBound(raw);
    unsigned char *z = malloc(zn);
    int ok = rows && z;
    if (ok) {
        for (int y = 0; y < h; y++) { rows[y * (stride + 1)] = 0; memcpy(rows + y * (stride + 1) + 1, cfg.output.u.RGBA.rgba + y * cfg.output.u.RGBA.stride, stride); }
        ok = compress2(z, &zn, rows, raw, 6) == Z_OK;
    }
    WebPFreeDecBuffer(&cfg.output);
    free(rows);
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s.part", out);
    FILE *f = ok ? fopen(tmp, "wb") : NULL;
    if (!f) { free(z); return -1; }
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    unsigned char ihdr[13];
    ihdr[0] = w >> 24; ihdr[1] = w >> 16; ihdr[2] = w >> 8; ihdr[3] = w;
    ihdr[4] = h >> 24; ihdr[5] = h >> 16; ihdr[6] = h >> 8; ihdr[7] = h;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    fwrite(sig, 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", NULL, 0);
    free(z);
    if (fclose(f)) { remove(tmp); return -1; }
    return rename(tmp, out);
}
