#include "liveness.h"

#include <stddef.h>
#include <string.h>

// Micro-motion: smoothed frame-to-frame mean-abs-difference of the normalised,
// equalised face must peak above this (gray levels) for the motion signal to
// pass. Calibrated on-device: a live face peaks ~23 and clears ~14 within the
// first second; a sharp static photo detects stably (low box jitter) and stays
// well below. 12 passes a live face in <1s with margin.
#define MOTION_THRESH 12.0f

// Eye band: rows ~27%..47% of the face height, cols 12%..88%. On a 96px face
// that is rows 26..45, cols 11..84 — covers both eyes with margin.
float glance_eye_energy(const uint8_t *f)
{
    const int F = GLANCE_FACE;
    int y0 = (int)(0.27f * F), y1 = (int)(0.47f * F);
    int x0 = (int)(0.12f * F), x1 = (int)(0.88f * F);
    double sum = 0.0; int n = 0;
    for (int y = y0; y < y1; ++y) {
        const uint8_t *row = f + (size_t)y * F;
        const uint8_t *up = row - F, *dn = row + F;
        for (int x = x0; x < x1; ++x) {
            int dx = row[x + 1] - row[x - 1]; if (dx < 0) dx = -dx;
            int dy = dn[x] - up[x];           if (dy < 0) dy = -dy;
            sum += dx + dy;
            n++;
        }
    }
    return n ? (float)(sum / n) : 0.0f;
}

void glance_liveness_reset(glance_liveness *L)
{
    L->n = 0; L->base = 0.0f; L->state = 0; L->blinks = 0;
    L->minr = 1e9f; L->maxr = -1e9f;
    L->lo = 0.70f;   // dip below 70% of baseline -> eyes closing
    L->hi = 0.85f;   // back above 85% -> eyes open again (blink complete)
    L->has_prev = 0; L->motion_ema = 0.0f; L->motion_peak = 0.0f; L->motion_frames = 0;
}

// Blink detector on the eye-band energy.
static void feed_blink(glance_liveness *L, float e)
{
    L->n++;
    // Baseline tracks the "open" level: jump up fast, decay slowly so a sustained
    // closed state can't drag it down to where blinks stop registering.
    if (e > L->base) L->base = e;
    else L->base = L->base * 0.98f + e * 0.02f;

    if (L->base <= 1e-6f) return;
    float r = e / L->base;
    if (r < L->minr) L->minr = r;
    if (r > L->maxr) L->maxr = r;

    if (L->state == 0) {                 // eyes open
        if (r < L->lo) L->state = 1;     // dipped -> closing
    } else {                             // mid-blink
        if (r > L->hi) { L->blinks++; L->state = 0; }  // recovered -> a blink
    }
}

void glance_liveness_feed(glance_liveness *L, float e)
{
    feed_blink(L, e);
}

void glance_liveness_feed_face(glance_liveness *L, const uint8_t *face)
{
    feed_blink(L, glance_eye_energy(face));

    const int N = GLANCE_FACE * GLANCE_FACE;
    if (L->has_prev) {
        long acc = 0;
        for (int i = 0; i < N; ++i) {
            int d = (int)face[i] - (int)L->prev[i];
            acc += d < 0 ? -d : d;
        }
        float mad = (float)acc / (float)N;
        // light smoothing so a single jittered frame doesn't spike the peak
        L->motion_ema = L->motion_frames ? (L->motion_ema * 0.5f + mad * 0.5f) : mad;
        if (L->motion_ema > L->motion_peak) L->motion_peak = L->motion_ema;
    }
    memcpy(L->prev, face, (size_t)N);
    L->has_prev = 1;
    L->motion_frames++;
}

int glance_liveness_passed(const glance_liveness *L)
{
    return L->blinks >= 1 || L->motion_peak >= MOTION_THRESH;
}
