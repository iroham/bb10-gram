// Face detection: a hand-rolled multi-scale LBP cascade evaluator over a
// grayscale frame. Model is baked in (cascade_data.h from an OpenCV LBP cascade).
// No OpenCV dependency.
#ifndef GLANCE_DETECT_H
#define GLANCE_DETECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int x, y, w, h; } glance_rect;

typedef struct glance_detector glance_detector;

// Create a detector sized for frames up to maxw x maxh (buffers are reused).
glance_detector *glance_detector_new(int maxw, int maxh);
void             glance_detector_free(glance_detector *d);

// Detect faces in a width*height grayscale image (row stride == width).
// scale_step e.g. 1.15; min_size/max_size in pixels (0 = default); min_neighbors
// e.g. 3. Writes up to max_out rects (largest first) and returns the count.
int glance_detect(glance_detector *d, const uint8_t *gray, int width, int height,
                  float scale_step, int min_size, int max_size, int min_neighbors,
                  glance_rect *out, int max_out);

#ifdef __cplusplus
}
#endif

#endif
