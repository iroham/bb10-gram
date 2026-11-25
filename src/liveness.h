// Liveness: a passive check that a still photo fails, with two signals combined.
//
// A frontal face detector keeps the face through blinks and small movements, so
// both cues below are observable on the normalised face:
//   - BLINK: "eye-band energy" (gradient activity where the eyes sit) dips and
//     recovers when the eyes close and open. A photo's eyes never close.
//   - MICRO-MOTION: a live face is never perfectly still (eyelids, gaze, skin,
//     breathing), so the frame-to-frame change of the normalised, equalised
//     face stays above sensor noise. A static photo's change is ~zero.
// Passing on EITHER keeps it passive ("just look"): a natural blink or ordinary
// liveness motion both clear it, while a flat static photo clears neither.
//
// Convenience-grade: resists a still photo, NOT a moving photo or a video
// replay. Honest about that.
#ifndef GLANCE_LIVENESS_H
#define GLANCE_LIVENESS_H

#include <stdint.h>
#include "recog.h"   // GLANCE_FACE

#ifdef __cplusplus
extern "C" {
#endif

// Mean gradient magnitude in the eye band of a normalised face (arbitrary units).
float glance_eye_energy(const uint8_t *face /* GLANCE_FACE*GLANCE_FACE */);

typedef struct {
    int    n;
    float  base;        // slow-tracking "open eye" baseline (running max, decaying)
    int    state;       // 0 = eyes open, 1 = mid-blink (dipped)
    int    blinks;      // completed blinks
    float  minr, maxr;  // observed eye-energy/baseline ratio range (diagnostics)
    float  lo, hi;      // dip-enter and recovery thresholds as fraction of base

    // micro-motion
    uint8_t prev[GLANCE_FACE * GLANCE_FACE];
    int     has_prev;
    float   motion_ema;   // smoothed per-frame mean-abs-difference
    float   motion_peak;  // largest smoothed value seen this session
    int     motion_frames;
} glance_liveness;

void glance_liveness_reset(glance_liveness *L);

// Preferred: feed the full normalised (equalised) face; updates both the blink
// detector and the micro-motion estimate.
void glance_liveness_feed_face(glance_liveness *L, const uint8_t *face /* GLANCE_FACE^2 */);

// Legacy: feed only eye energy (blink signal only; no micro-motion).
void glance_liveness_feed(glance_liveness *L, float eye_energy);

// 1 once liveness is satisfied: a completed blink OR enough micro-motion.
int  glance_liveness_passed(const glance_liveness *L);

#ifdef __cplusplus
}
#endif

#endif
