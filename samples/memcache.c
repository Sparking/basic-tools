#include <stdio.h>
#include <stdlib.h>
#include <memcache.h>

struct demo_s {
    int t;
    int x;
    int y;
    unsigned z;
    float v;
};

int main(void)
{
    int i;
    memcache_t cache;
    struct demo_s *pdm;

    cache = memcache_create();
    if (cache == NULL) {
        return -1;
    }

    for (i = 0; i < 99999; ++i) {
        pdm = memcache_alloc(cache, sizeof(struct demo_s));
        if (pdm == NULL) {
            fprintf(stderr, "no memory allcation\n");
            break;
        }
    }

    memcache_clear(cache);
    for (i = 0; i < 99999; ++i) {
        pdm = memcache_alloc(cache, sizeof(struct demo_s));
        if (pdm == NULL) {
            fprintf(stderr, "no memory allcation\n");
            break;
        }

        memcache_free(cache, pdm);
    }

    for (i = 0; i < 99999; ++i) {
        pdm = memcache_alloc(cache, sizeof(struct demo_s));
        if (pdm == NULL) {
            fprintf(stderr, "no memory allcation\n");
            break;
        }

        memcache_free(cache, pdm);
    }
    memcache_clear(cache);
    memcache_destroy(cache);

    return 0;
}
