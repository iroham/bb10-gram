// Face recognition: Local Binary Pattern Histograms (LBPH), hand-rolled.
//
// A detected face box is normalised to a fixed square, histogram-equalised, and
// turned into a spatial LBP histogram (an 8x8 grid of 256-bin cell histograms,
// L1-normalised). Enrolment stores several such templates for the owner; a query
// matches when its chi-square distance to the nearest enrolled template is below
// a threshold. No OpenCV.
#ifndef GLANCE_RECOG_H
#define GLANCE_RECOG_H

#include <stdint.h>
#include "detect.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLANCE_FACE      96          // normalised face size (px)
#define GLANCE_GRID      8           // cells per side
#define GLANCE_BINS      256         // LBP codes
#define GLANCE_DIM       (GLANCE_GRID * GLANCE_GRID * GLANCE_BINS)  // 16384

typedef struct { float h[GLANCE_DIM]; } glance_template;

// Normalise a face box to a GLANCE_FACE x GLANCE_FACE, histogram-equalised
// grayscale buffer (out must hold GLANCE_FACE*GLANCE_FACE bytes). This is the
// shared front-end for both recognition and liveness.
void  glance_normalize(const uint8_t *gray, int width, int height,
                       glance_rect box, uint8_t *out);

// Build a template from a face box in a width*height grayscale image.
// Returns 0 on success. The box is clamped to the image.
int   glance_template_build(const uint8_t *gray, int width, int height,
                            glance_rect box, glance_template *out);

// Chi-square distance between two L1-normalised templates (smaller = more alike).
float glance_template_distance(const glance_template *a, const glance_template *b);

// ---- enrolment store (a set of templates saved to one file) -----------------
typedef struct glance_enroll glance_enroll;

glance_enroll *glance_enroll_new(void);
void           glance_enroll_free(glance_enroll *e);
int            glance_enroll_add(glance_enroll *e, const glance_template *t);  // 0 ok
int            glance_enroll_count(const glance_enroll *e);

// Nearest enrolled template's distance to q (and its index in *which if non-NULL).
// Returns a large value if the store is empty.
float          glance_enroll_match(const glance_enroll *e, const glance_template *q, int *which);

// Persist / load the store (binary, versioned). Return 0 on success.
int            glance_enroll_save(const glance_enroll *e, const char *path);
glance_enroll *glance_enroll_load(const char *path);

#ifdef __cplusplus
}
#endif

#endif
