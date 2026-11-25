/* anim.h: animated stickers (.tgs Lottie, .webm VP9 with alpha) -> premultiplied BGRA frames. libgramanim.so */
#ifndef GRAM_ANIM_H
#define GRAM_ANIM_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct gm_anim gm_anim;
gm_anim *gm_open(const char *path, int size);               /* NULL if the file is not a supported animation */
int gm_frames(gm_anim *a);
double gm_fps(gm_anim *a);
int gm_render(gm_anim *a, int frame, unsigned char *bgra, int stride);   /* 0 = ok; webm: fastest in order */
void gm_close(gm_anim *a);
#ifdef __cplusplus
}
#endif
#endif
