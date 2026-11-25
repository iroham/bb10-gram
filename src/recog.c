#include "recog.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---- normalisation ----------------------------------------------------------

// Bilinear-resample the face box from the source gray image into a
// GLANCE_FACE x GLANCE_FACE buffer.
static void resample_face(const uint8_t *gray, int width, int height,
                          glance_rect b, uint8_t *out)
{
    // clamp box to image
    if (b.x < 0) { b.w += b.x; b.x = 0; }
    if (b.y < 0) { b.h += b.y; b.y = 0; }
    if (b.x + b.w > width)  b.w = width  - b.x;
    if (b.y + b.h > height) b.h = height - b.y;
    if (b.w < 1) b.w = 1;
    if (b.h < 1) b.h = 1;

    for (int oy = 0; oy < GLANCE_FACE; ++oy) {
        float fy = b.y + (oy + 0.5f) * b.h / GLANCE_FACE - 0.5f;
        int y0 = (int)fy; float wy = fy - y0;
        if (y0 < 0) { y0 = 0; wy = 0; }
        if (y0 >= height - 1) { y0 = height - 2; wy = 1; }
        const uint8_t *r0 = gray + (size_t)y0 * width;
        const uint8_t *r1 = r0 + width;
        uint8_t *orow = out + (size_t)oy * GLANCE_FACE;
        for (int ox = 0; ox < GLANCE_FACE; ++ox) {
            float fx = b.x + (ox + 0.5f) * b.w / GLANCE_FACE - 0.5f;
            int x0 = (int)fx; float wx = fx - x0;
            if (x0 < 0) { x0 = 0; wx = 0; }
            if (x0 >= width - 1) { x0 = width - 2; wx = 1; }
            float top = r0[x0] * (1 - wx) + r0[x0 + 1] * wx;
            float bot = r1[x0] * (1 - wx) + r1[x0 + 1] * wx;
            orow[ox] = (uint8_t)(top * (1 - wy) + bot * wy + 0.5f);
        }
    }
}

// In-place histogram equalisation over a GLANCE_FACE^2 buffer (lighting robustness).
static void equalize(uint8_t *f)
{
    int n = GLANCE_FACE * GLANCE_FACE;
    int hist[256] = {0};
    for (int i = 0; i < n; ++i) hist[f[i]]++;
    int cdf[256]; int acc = 0, cdfmin = 0;
    for (int i = 0; i < 256; ++i) { acc += hist[i]; cdf[i] = acc; if (!cdfmin && acc) cdfmin = acc; }
    int denom = n - cdfmin;
    if (denom <= 0) return;
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        int v = (cdf[i] - cdfmin) * 255 / denom;
        lut[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    for (int i = 0; i < n; ++i) f[i] = lut[f[i]];
}

// ---- LBPH -------------------------------------------------------------------

void glance_normalize(const uint8_t *gray, int width, int height,
                      glance_rect box, uint8_t *out)
{
    resample_face(gray, width, height, box, out);
    equalize(out);
}

int glance_template_build(const uint8_t *gray, int width, int height,
                          glance_rect box, glance_template *out)
{
    if (!gray || !out || width < 2 || height < 2) return -1;
    uint8_t face[GLANCE_FACE * GLANCE_FACE];
    glance_normalize(gray, width, height, box, face);

    memset(out->h, 0, sizeof out->h);
    int cell = GLANCE_FACE / GLANCE_GRID;   // 12

    // Standard 3x3 LBP; border pixels skipped. Accumulate into the cell histogram.
    for (int y = 1; y < GLANCE_FACE - 1; ++y) {
        for (int x = 1; x < GLANCE_FACE - 1; ++x) {
            const uint8_t *p = face + (size_t)y * GLANCE_FACE + x;
            uint8_t c = *p;
            int code =
                ((p[-GLANCE_FACE - 1] >= c) << 7) | ((p[-GLANCE_FACE] >= c) << 6) |
                ((p[-GLANCE_FACE + 1] >= c) << 5) | ((p[1] >= c) << 4) |
                ((p[GLANCE_FACE + 1] >= c) << 3) | ((p[GLANCE_FACE] >= c) << 2) |
                ((p[GLANCE_FACE - 1] >= c) << 1) | ((p[-1] >= c) << 0);
            int gx = x / cell, gy = y / cell;
            if (gx >= GLANCE_GRID) gx = GLANCE_GRID - 1;
            if (gy >= GLANCE_GRID) gy = GLANCE_GRID - 1;
            out->h[(gy * GLANCE_GRID + gx) * GLANCE_BINS + code] += 1.0f;
        }
    }
    // L1-normalise each cell histogram independently
    for (int cidx = 0; cidx < GLANCE_GRID * GLANCE_GRID; ++cidx) {
        float *hc = out->h + (size_t)cidx * GLANCE_BINS;
        float s = 0;
        for (int i = 0; i < GLANCE_BINS; ++i) s += hc[i];
        if (s > 0) { float inv = 1.0f / s; for (int i = 0; i < GLANCE_BINS; ++i) hc[i] *= inv; }
    }
    return 0;
}

float glance_template_distance(const glance_template *a, const glance_template *b)
{
    float d = 0;
    for (int i = 0; i < GLANCE_DIM; ++i) {
        float s = a->h[i] + b->h[i];
        if (s > 1e-9f) { float df = a->h[i] - b->h[i]; d += df * df / s; }
    }
    return d;
}

// ---- enrolment store --------------------------------------------------------

struct glance_enroll {
    glance_template *t;
    int              n, cap;
};

glance_enroll *glance_enroll_new(void)
{
    glance_enroll *e = (glance_enroll *)calloc(1, sizeof(*e));
    return e;
}

void glance_enroll_free(glance_enroll *e)
{
    if (!e) return;
    free(e->t);
    free(e);
}

int glance_enroll_add(glance_enroll *e, const glance_template *t)
{
    if (!e) return -1;
    if (e->n == e->cap) {
        int nc = e->cap ? e->cap * 2 : 8;
        glance_template *nt = (glance_template *)realloc(e->t, (size_t)nc * sizeof(glance_template));
        if (!nt) return -1;
        e->t = nt; e->cap = nc;
    }
    e->t[e->n++] = *t;
    return 0;
}

int glance_enroll_count(const glance_enroll *e) { return e ? e->n : 0; }

float glance_enroll_match(const glance_enroll *e, const glance_template *q, int *which)
{
    if (which) *which = -1;
    if (!e || e->n == 0) return 1e30f;
    float best = 1e30f;
    for (int i = 0; i < e->n; ++i) {
        float d = glance_template_distance(&e->t[i], q);
        if (d < best) { best = d; if (which) *which = i; }
    }
    return best;
}

#define GLANCE_ENROLL_MAGIC 0x47464c31u   // "GFL1"

int glance_enroll_save(const glance_enroll *e, const char *path)
{
    if (!e) return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint32_t magic = GLANCE_ENROLL_MAGIC, dim = GLANCE_DIM, n = (uint32_t)e->n;
    int ok = fwrite(&magic, 4, 1, fp) == 1 &&
             fwrite(&dim, 4, 1, fp) == 1 &&
             fwrite(&n, 4, 1, fp) == 1 &&
             (n == 0 || fwrite(e->t, sizeof(glance_template), e->n, fp) == (size_t)e->n);
    fclose(fp);
    return ok ? 0 : -1;
}

glance_enroll *glance_enroll_load(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    uint32_t magic = 0, dim = 0, n = 0;
    if (fread(&magic, 4, 1, fp) != 1 || fread(&dim, 4, 1, fp) != 1 || fread(&n, 4, 1, fp) != 1 ||
        magic != GLANCE_ENROLL_MAGIC || dim != GLANCE_DIM || n > 1000) {
        fclose(fp); return NULL;
    }
    glance_enroll *e = glance_enroll_new();
    if (!e) { fclose(fp); return NULL; }
    if (n) {
        e->t = (glance_template *)malloc((size_t)n * sizeof(glance_template));
        if (!e->t || fread(e->t, sizeof(glance_template), n, fp) != n) {
            glance_enroll_free(e); fclose(fp); return NULL;
        }
        e->n = e->cap = (int)n;
    }
    fclose(fp);
    return e;
}
