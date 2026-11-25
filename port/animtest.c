/* animtest <file> [size]: render every frame, print frames, fps and the time per frame; writes frame 0 as
   raw BGRA to /tmp/animtest.bgra (size x size). */
#include "anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
int main(int argc, char **argv) {
    int size = argc > 2 ? atoi(argv[2]) : 256;
    double t0 = now();
    gm_anim *a = gm_open(argv[1], size);
    if (!a) { printf("cannot open\n"); return 1; }
    double t1 = now();
    unsigned char *buf = malloc(size * size * 4);
    int n = gm_frames(a), bad = 0;
    for (int i = 0; i < n; i++) {
        if (gm_render(a, i, buf, size * 4)) bad++;
        if (i == 0) { FILE *f = fopen("/tmp/animtest.bgra", "wb"); if (f) { fwrite(buf, 1, size * size * 4, f); fclose(f); } }
    }
    double t2 = now();
    printf("frames %d fps %.1f open %.0f ms render %.1f ms/frame bad %d\n", n, gm_fps(a), (t1 - t0) * 1000, n ? (t2 - t1) * 1000 / n : 0, bad);
    gm_close(a); free(buf);
    return 0;
}
