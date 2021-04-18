#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

#include <inttypes.h>

#include <stdio.h>

void murmurhash3_x86_32 (const void *key, int len, uint32_t seed, void *out);

void murmurhash3_x86_128(const void *key, int len, uint32_t seed, void *out);

void murmurhash3_x64_128(const void *key, int len, uint32_t seed, void *out);

#endif
