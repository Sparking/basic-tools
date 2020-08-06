#ifndef __MEMCACHE_H_
#define __MEMCACHE_H_

#include <stdint.h>
#include <stdbool.h>

typedef void *memcache_t;

extern memcache_t memcache_create(void);

extern void *memcache_alloc(memcache_t cache, size_t size);

extern void memcache_free(memcache_t cache, void *ptr);

extern void memcache_clear(memcache_t cache);

extern void memcache_destroy(memcache_t cache);

#endif

