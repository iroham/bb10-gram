/* media.h: converters for formats BB10 cannot handle by itself (see media.c). */
#ifndef MEDIA_H
#define MEDIA_H
#ifdef __cplusplus
extern "C" {
#endif
int opus_to_wav(const char *in, const char *out);
int webp_to_png(const char *in, const char *out, int max_side);
#ifdef __cplusplus
}
#endif
#endif
