#include "detect.h"
#include "cascade_data.h"

#include <stdlib.h>
#include <string.h>

struct glance_detector {
    int      maxw, maxh;
    uint32_t *integral;   // (maxw+1) * (maxh+1)
    glance_rect *cand;    // candidate windows before grouping
    int      cand_cap;
};

glance_detector *glance_detector_new(int maxw, int maxh)
{
    glance_detector *d = (glance_detector *)calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->maxw = maxw; d->maxh = maxh;
    d->integral = (uint32_t *)malloc((size_t)(maxw + 1) * (maxh + 1) * sizeof(uint32_t));
    d->cand_cap = 4096;
    d->cand = (glance_rect *)malloc((size_t)d->cand_cap * sizeof(glance_rect));
    if (!d->integral || !d->cand) { glance_detector_free(d); return NULL; }
    return d;
}

void glance_detector_free(glance_detector *d)
{
    if (!d) return;
    free(d->integral);
    free(d->cand);
    free(d);
}

// Build the integral image: II[y][x] = sum of all pixels above-left of (x,y).
// Dimensions (w+1)x(h+1); row 0 and col 0 are zero.
static void build_integral(glance_detector *d, const uint8_t *gray, int w, int h)
{
    int iw = w + 1;
    uint32_t *I = d->integral;
    memset(I, 0, (size_t)iw * sizeof(uint32_t));           // top row
    for (int y = 0; y < h; ++y) {
        uint32_t rowsum = 0;
        uint32_t *Icur = I + (size_t)(y + 1) * iw;
        const uint32_t *Iprev = I + (size_t)y * iw;
        const uint8_t *g = gray + (size_t)y * w;
        Icur[0] = 0;
        for (int x = 0; x < w; ++x) {
            rowsum += g[x];
            Icur[x + 1] = Iprev[x + 1] + rowsum;
        }
    }
}

// Sum of the rectangle [x,x+rw) x [y,y+rh) via the integral image.
static inline uint32_t rsum(const uint32_t *I, int iw, int x, int y, int rw, int rh)
{
    const uint32_t *a = I + (size_t)y * iw + x;
    const uint32_t *b = I + (size_t)(y + rh) * iw + x;
    return a[0] - a[rw] - b[0] + b[rw];
}

// Evaluate the cascade at window origin (ox,oy) with integer cell size (cw,ch)
// per LBP cell. Returns 1 if the window passes all stages (a face).
static int eval_window(const uint32_t *I, int iw, int ox, int oy, float s)
{
    for (int st = 0; st < GC_NSTAGES; ++st) {
        const gc_stage_t *stage = &gc_stage[st];
        float sum = 0.0f;
        const gc_weak_t *wk = &gc_weak[stage->offset];
        for (int i = 0; i < stage->count; ++i, ++wk) {
            const int16_t *f = gc_feat[wk->feat];
            int rx = (int)(f[0] * s + 0.5f);
            int ry = (int)(f[1] * s + 0.5f);
            int cw = (int)(f[2] * s + 0.5f);
            int ch = (int)(f[3] * s + 0.5f);
            if (cw < 1) cw = 1;
            if (ch < 1) ch = 1;
            int bx = ox + rx, by = oy + ry;
            // 3x3 grid of cw x ch cells; p[r*3+c] is the cell sum
            uint32_t p[9];
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    p[r * 3 + c] = rsum(I, iw, bx + c * cw, by + r * ch, cw, ch);
            uint32_t cval = p[4];
            // OpenCV LBP bit order: TL,TM,TR,MR,BR,BM,BL,ML -> bits 7..0
            int code =
                ((p[0] >= cval) << 7) | ((p[1] >= cval) << 6) |
                ((p[2] >= cval) << 5) | ((p[5] >= cval) << 4) |
                ((p[8] >= cval) << 3) | ((p[7] >= cval) << 2) |
                ((p[6] >= cval) << 1) | ((p[3] >= cval) << 0);
            int bit = wk->subset[code >> 5] & (1 << (code & 31));
            sum += bit ? wk->leaf0 : wk->leaf1;
        }
        if (sum < stage->threshold) return 0;   // stage rejects -> not a face
    }
    return 1;
}

// Group overlapping candidate rects (OpenCV-style simple grouping): union-find by
// overlap, keep groups with >= min_neighbors members, output the averaged rect.
static int group_rects(glance_rect *c, int n, int min_neighbors,
                       glance_rect *out, int max_out)
{
    if (n == 0) return 0;
    int *label = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; ++i) label[i] = i;
    // naive O(n^2) union by similarity
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            // similar if each is roughly inside a grown version of the other
            float d = 0.2f * (c[i].w + c[j].w) * 0.5f + 1.0f;
            if (abs(c[i].x - c[j].x) <= d && abs(c[i].y - c[j].y) <= d &&
                abs(c[i].w - c[j].w) <= d && abs(c[i].h - c[j].h) <= d) {
                int li = label[i], lj = label[j];
                if (li != lj) for (int k = 0; k < n; ++k) if (label[k] == lj) label[k] = li;
            }
        }
    }
    int nout = 0;
    for (int lab = 0; lab < n; ++lab) {
        long sx = 0, sy = 0, sw = 0, sh = 0, cnt = 0;
        for (int k = 0; k < n; ++k) if (label[k] == lab) {
            sx += c[k].x; sy += c[k].y; sw += c[k].w; sh += c[k].h; cnt++;
        }
        if (cnt < min_neighbors) continue;
        glance_rect r;
        r.x = (int)(sx / cnt); r.y = (int)(sy / cnt);
        r.w = (int)(sw / cnt); r.h = (int)(sh / cnt);
        // insert sorted by area desc
        if (nout < max_out) { out[nout++] = r; }
        else {
            int smallest = 0;
            for (int t = 1; t < nout; ++t)
                if (out[t].w * out[t].h < out[smallest].w * out[smallest].h) smallest = t;
            if (r.w * r.h > out[smallest].w * out[smallest].h) out[smallest] = r;
        }
    }
    // simple sort largest-first
    for (int i = 0; i < nout; ++i)
        for (int j = i + 1; j < nout; ++j)
            if (out[j].w * out[j].h > out[i].w * out[i].h) {
                glance_rect t = out[i]; out[i] = out[j]; out[j] = t;
            }
    free(label);
    return nout;
}

int glance_detect(glance_detector *d, const uint8_t *gray, int width, int height,
                  float scale_step, int min_size, int max_size, int min_neighbors,
                  glance_rect *out, int max_out)
{
    if (!d || width > d->maxw || height > d->maxh) return -1;
    if (scale_step < 1.01f) scale_step = 1.15f;
    if (min_neighbors < 1) min_neighbors = 1;
    int smaller = width < height ? width : height;
    if (min_size <= 0) min_size = GC_WIN_W;             // ~45px
    if (max_size <= 0) max_size = smaller;

    build_integral(d, gray, width, height);
    int iw = width + 1;

    int ncand = 0;
    // scale is window/GC_WIN_W; start so the window >= min_size
    for (float s = (float)min_size / GC_WIN_W;
         (int)(GC_WIN_W * s) <= max_size && (int)(GC_WIN_H * s) <= height && (int)(GC_WIN_W * s) <= width;
         s *= scale_step) {
        int win_w = (int)(GC_WIN_W * s + 0.5f);
        int win_h = (int)(GC_WIN_H * s + 0.5f);
        if (win_w < GC_WIN_W || win_h < GC_WIN_H) continue;
        int step = (int)(s * 1.2f + 0.5f);              // scan step scales with window
        if (step < 1) step = 1;
        for (int oy = 0; oy + win_h < height; oy += step) {
            for (int ox = 0; ox + win_w < width; ox += step) {
                if (eval_window(d->integral, iw, ox, oy, s)) {
                    if (ncand < d->cand_cap) {
                        d->cand[ncand].x = ox; d->cand[ncand].y = oy;
                        d->cand[ncand].w = win_w; d->cand[ncand].h = win_h;
                        ncand++;
                    }
                }
            }
        }
    }
    return group_rects(d->cand, ncand, min_neighbors, out, max_out);
}
