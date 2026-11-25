// anim.cpp: animated stickers for BB10. One small C interface (anim.h) over
//   - rlottie: .tgs files (gzip-compressed Lottie JSON)
//   - libvpx:  .webm files (VP9, alpha in BlockAdditional id 1 as a second VP9 stream), own minimal WebM reader
// Frames come out as premultiplied BGRA (bytes B,G,R,A), size x size, the animation fitted and centred.
#include "anim.h"
#include <rlottie_capi.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

struct Frame { size_t off, len, aoff, alen; };
struct gm_anim {
    int size, frames; double fps;
    Lottie_Animation *lottie;
    // webm
    std::vector<unsigned char> data;
    std::vector<Frame> list;
    vpx_codec_ctx_t dec, adec; bool hasDec, hasAdec;
    int next;                                  // next frame the decoder can produce without restarting
    int vw, vh;
};

static bool readFile(const char *path, std::vector<unsigned char> &out) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return !out.empty();
}
static bool gunzip(const std::vector<unsigned char> &in, std::string &out) {
    z_stream z; memset(&z, 0, sizeof z);
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) return false;
    z.next_in = (Bytef *)&in[0]; z.avail_in = in.size();
    char buf[65536]; int r;
    do {
        z.next_out = (Bytef *)buf; z.avail_out = sizeof buf;
        r = inflate(&z, Z_NO_FLUSH);
        if (r != Z_OK && r != Z_STREAM_END) { inflateEnd(&z); return false; }
        out.append(buf, sizeof buf - z.avail_out);
    } while (r != Z_STREAM_END && out.size() < (16u << 20));
    inflateEnd(&z);
    return r == Z_STREAM_END;
}

// ---------------------------------------------------------------- WebM (EBML) reading
static bool vint(const unsigned char *p, size_t end, size_t &pos, unsigned long long &v, bool keepMarker, bool *unknown = 0) {
    if (pos >= end) return false;
    unsigned char b = p[pos]; int len = 1;
    while (len <= 8 && !(b & (0x80 >> (len - 1)))) len++;
    if (len > 8 || pos + len > end) return false;
    v = keepMarker ? b : (b & (0xFF >> len));
    bool ones = (v == (unsigned long long)(0xFF >> len));
    for (int i = 1; i < len; i++) { v = (v << 8) | p[pos + i]; if (p[pos + i] != 0xFF) ones = false; }
    if (unknown) *unknown = ones;
    pos += len;
    return true;
}
struct Reader {
    gm_anim *a; const unsigned char *p; double scale; double duration;
    void block(size_t s, size_t e, size_t aoff, size_t alen) {
        size_t pos = s; unsigned long long track;
        if (!vint(p, e, pos, track, false) || pos + 3 > e) return;
        pos += 3;                                                 // timecode (2) + flags (1); stickers use no lacing
        if (track != 1) return;
        Frame f = { pos, e - pos, aoff, alen };
        a->list.push_back(f);
    }
    void walk(size_t s, size_t e, int depth) {
        size_t pos = s;
        while (pos < e) {
            unsigned long long id, size; bool unknown = false;
            if (!vint(p, e, pos, id, true) || !vint(p, e, pos, size, false, &unknown)) return;
            size_t ce = unknown || pos + size > e ? e : pos + size;
            switch (id) {
            case 0x18538067: case 0x1F43B675: case 0x1654AE6B: case 0xAE: case 0xE0: case 0x1549A966:   // Segment Cluster Tracks TrackEntry Video Info
                walk(pos, ce, depth + 1); break;
            case 0xB0: a->vw = (int)num(pos, ce); break;
            case 0xBA: a->vh = (int)num(pos, ce); break;
            case 0x2AD7B1: scale = (double)num(pos, ce); break;
            case 0x4489: duration = flt(pos, ce); break;
            case 0xA3: block(pos, ce, 0, 0); break;                  // SimpleBlock
            case 0xA0: group(pos, ce); break;                        // BlockGroup
            default: break;
            }
            if (unknown && (id == 0x1F43B675 || id == 0x18538067)) return;   // walked to the end already
            pos = ce;
        }
    }
    void group(size_t s, size_t e) {
        size_t pos = s, bs = 0, be = 0, ao = 0, al = 0;
        while (pos < e) {
            unsigned long long id, size;
            if (!vint(p, e, pos, id, true) || !vint(p, e, pos, size, false)) return;
            size_t ce = pos + size > e ? e : pos + size;
            if (id == 0xA1) { bs = pos; be = ce; }
            else if (id == 0x75A1) additions(pos, ce, ao, al);
            pos = ce;
        }
        if (be) block(bs, be, ao, al);
    }
    void additions(size_t s, size_t e, size_t &ao, size_t &al) {   // BlockAdditions > BlockMore > BlockAddID / BlockAdditional
        size_t pos = s;
        while (pos < e) {
            unsigned long long id, size;
            if (!vint(p, e, pos, id, true) || !vint(p, e, pos, size, false)) return;
            size_t ce = pos + size > e ? e : pos + size;
            if (id == 0xA6) {
                size_t q = pos; unsigned long long addId = 1; size_t xo = 0, xl = 0;
                while (q < ce) {
                    unsigned long long i2, s2;
                    if (!vint(p, ce, q, i2, true) || !vint(p, ce, q, s2, false)) break;
                    size_t e2 = q + s2 > ce ? ce : q + s2;
                    if (i2 == 0xEE) addId = num(q, e2); else if (i2 == 0xA5) { xo = q; xl = e2 - q; }
                    q = e2;
                }
                if (addId == 1 && xl) { ao = xo; al = xl; }
            }
            pos = ce;
        }
    }
    unsigned long long num(size_t s, size_t e) { unsigned long long v = 0; for (size_t i = s; i < e; i++) v = (v << 8) | p[i]; return v; }
    double flt(size_t s, size_t e) {
        if (e - s == 4) { unsigned int u = (unsigned int)num(s, e); float f; memcpy(&f, &u, 4); return f; }
        if (e - s == 8) { unsigned long long u = num(s, e); double d; memcpy(&d, &u, 8); return d; }
        return 0;
    }
};

static bool openWebm(gm_anim *a) {
    Reader r; r.a = a; r.p = &a->data[0]; r.scale = 1000000; r.duration = 0;
    r.walk(0, a->data.size(), 0);
    if (a->list.empty()) return false;
    a->frames = (int)a->list.size();
    double secs = r.duration * r.scale / 1e9;
    a->fps = secs > 0.05 ? a->frames / secs : 30;
    if (a->fps > 60) a->fps = 60;
    if (vpx_codec_dec_init(&a->dec, vpx_codec_vp9_dx(), NULL, 0)) return false;
    a->hasDec = true;
    return true;
}
static inline unsigned char clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : (unsigned char)v; }
static int renderWebm(gm_anim *a, int frame, unsigned char *out, int stride) {
    if (frame < a->next) {                                        // loop: start again from the first (key) frame
        vpx_codec_destroy(&a->dec); vpx_codec_dec_init(&a->dec, vpx_codec_vp9_dx(), NULL, 0);
        if (a->hasAdec) { vpx_codec_destroy(&a->adec); a->hasAdec = false; }
        a->next = 0;
    }
    vpx_image_t *img = 0, *aimg = 0;
    for (; a->next <= frame; a->next++) {
        const Frame &f = a->list[a->next];
        if (vpx_codec_decode(&a->dec, &a->data[f.off], (unsigned int)f.len, NULL, 0)) return -1;
        vpx_codec_iter_t it = NULL; img = vpx_codec_get_frame(&a->dec, &it);
        if (f.alen) {
            if (!a->hasAdec) { if (vpx_codec_dec_init(&a->adec, vpx_codec_vp9_dx(), NULL, 0)) return -1; a->hasAdec = true; }
            if (!vpx_codec_decode(&a->adec, &a->data[f.aoff], (unsigned int)f.alen, NULL, 0)) { vpx_codec_iter_t i2 = NULL; aimg = vpx_codec_get_frame(&a->adec, &i2); }
        }
    }
    if (!img) return -1;
    int S = a->size, w = img->d_w, h = img->d_h;
    double k = (double)S / (w > h ? w : h);
    int ow = (int)(w * k), oh = (int)(h * k), ox = (S - ow) / 2, oy = (S - oh) / 2;
    for (int y = 0; y < S; y++) memset(out + y * stride, 0, S * 4);
    bool hi = (img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) != 0;
    if (hi) return -1;
    for (int y = 0; y < oh; y++) {
        int sy = (int)(y / k); if (sy >= h) sy = h - 1;
        const unsigned char *Y = img->planes[0] + sy * img->stride[0];
        const unsigned char *U = img->planes[1] + (sy >> img->y_chroma_shift) * img->stride[1];
        const unsigned char *V = img->planes[2] + (sy >> img->y_chroma_shift) * img->stride[2];
        const unsigned char *A = aimg ? aimg->planes[0] + sy * aimg->stride[0] : 0;
        unsigned char *o = out + (y + oy) * stride + ox * 4;
        for (int x = 0; x < ow; x++, o += 4) {
            int sx = (int)(x / k); if (sx >= w) sx = w - 1;
            int c = Y[sx] - 16, d = U[sx >> img->x_chroma_shift] - 128, e = V[sx >> img->x_chroma_shift] - 128;
            int r = clamp8((298 * c + 409 * e + 128) >> 8), g = clamp8((298 * c - 100 * d - 208 * e + 128) >> 8), b = clamp8((298 * c + 516 * d + 128) >> 8);
            int al = A ? clamp8(((A[sx] - 16) * 298 + 128) >> 8) : 255;
            o[0] = (unsigned char)(b * al / 255); o[1] = (unsigned char)(g * al / 255); o[2] = (unsigned char)(r * al / 255); o[3] = (unsigned char)al;
        }
    }
    return 0;
}

extern "C" {
__attribute__((visibility("default"))) gm_anim *gm_open(const char *path, int size) {
    gm_anim *a = new gm_anim();
    a->size = size; a->lottie = 0; a->hasDec = a->hasAdec = false; a->next = 0; a->vw = a->vh = 0; a->frames = 0; a->fps = 30;
    if (!readFile(path, a->data)) { delete a; return 0; }
    if (a->data.size() > 4 && a->data[0] == 0x1F && a->data[1] == 0x8B) {          // gzip: .tgs
        std::string json;
        if (gunzip(a->data, json)) a->lottie = lottie_animation_from_data(json.c_str(), path, "");
        std::vector<unsigned char>().swap(a->data);
        if (!a->lottie) { delete a; return 0; }
        a->frames = (int)lottie_animation_get_totalframe(a->lottie);
        a->fps = lottie_animation_get_framerate(a->lottie);
        return a;
    }
    if (a->data.size() > 4 && a->data[0] == 0x1A && a->data[1] == 0x45 && a->data[2] == 0xDF && a->data[3] == 0xA3 && openWebm(a)) return a;
    gm_close(a);
    return 0;
}
__attribute__((visibility("default"))) int gm_frames(gm_anim *a) { return a ? a->frames : 0; }
__attribute__((visibility("default"))) double gm_fps(gm_anim *a) { return a ? a->fps : 0; }
__attribute__((visibility("default"))) int gm_render(gm_anim *a, int frame, unsigned char *bgra, int stride) {
    if (!a || frame < 0 || frame >= a->frames) return -1;
    if (a->lottie) { lottie_animation_render(a->lottie, frame, (uint32_t *)bgra, a->size, a->size, stride); return 0; }
    return renderWebm(a, frame, bgra, stride);
}
__attribute__((visibility("default"))) void gm_close(gm_anim *a) {
    if (!a) return;
    if (a->lottie) lottie_animation_destroy(a->lottie);
    if (a->hasDec) vpx_codec_destroy(&a->dec);
    if (a->hasAdec) vpx_codec_destroy(&a->adec);
    delete a;
}
}
