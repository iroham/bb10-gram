#ifndef PINHASH_H
#define PINHASH_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void gram_sha256(const unsigned char *p, size_t n, unsigned char out[32]);
void gram_pbkdf2_sha256(const unsigned char *pass, size_t pn, const unsigned char *salt, size_t sn, int iter, unsigned char *out, size_t outLen);
#ifdef __cplusplus
}
#endif
#endif
