#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "tree234.h"
#include "memcache.h"

struct memcache_list {
    size_t alloc_size;
    struct list_head allocated;
    struct list_head cached;
};

struct memcache_node {
    struct memcache_list *list;
    struct list_head node;
    char data[0];
};

struct memcache_s {
    pthread_mutex_t mutex;
    tree234 *list;
};

static int memcache_list_compare(void *a, void *b)
{
    return (int)(((struct memcache_list *)a)->alloc_size - ((struct memcache_list *)b)->alloc_size);
}

static int memcache_list_compare_rel(void *a, void *b)
{
    int ret, rel;

    ret = memcache_list_compare(a, b);
    if (ret == 0) {
        rel = REL234_EQ;
    } else if (ret > 0) {
        rel = REL234_GT;
    } else {
        rel = REL234_LT;
    }

    return rel;
}

memcache_t memcache_create(void)
{
    struct memcache_s *cache;

    cache = (struct memcache_s *)malloc(sizeof(struct memcache_s));
    if (cache == NULL) {
        return NULL;
    }

    if (pthread_mutex_init(&cache->mutex, NULL) != 0) {
        free(cache);
        return NULL;
    }

    cache->list = newtree234(memcache_list_compare);
    if (cache->list == NULL) {
        pthread_mutex_destroy(&cache->mutex);
        free(cache);
        return NULL;
    }

    return (memcache_t)cache;
}

static struct memcache_list *get_memcache_list(tree234 *tree, const size_t size)
{
    struct memcache_list *list;
    struct memcache_list e;

    e.alloc_size = size;
    list = find234(tree, &e, memcache_list_compare_rel);    
    if (list != NULL) {
        return list;
    }

    list = (struct memcache_list *)malloc(sizeof(struct memcache_list));
    if (list == NULL) {
        return NULL;
    }

    list->alloc_size = size;
    INIT_LIST_HEAD(&list->allocated);
    INIT_LIST_HEAD(&list->cached);
    if (add234(tree, list) == NULL) {
        free(list);
        list = NULL;
    }

    return list;
}

void *memcache_alloc(memcache_t cache, const size_t size)
{
    void *value;
    struct list_head *mnode;
    struct memcache_node *node;
    struct memcache_list *list;
    struct memcache_s *const ptr = cache;

    if (ptr == NULL || size == 0) {
        return NULL;
    }

    value = NULL;
    pthread_mutex_lock(&ptr->mutex);
    list = get_memcache_list(ptr->list, size);
    if (list == NULL) {
        goto unlock;
    }

    if (!list_empty(&list->cached)) {
        mnode = list->cached.next;
        list_del(mnode);
        list_add(mnode, &list->allocated);
        value = list_entry(mnode, struct memcache_node, node)->data;
        goto unlock;
    }

    node = (struct memcache_node *)malloc(sizeof(struct memcache_node));
    if (node == NULL) {
        goto unlock;
    }

    list_add(&node->node, &list->allocated);
    node->list = list;
    value = node->data;
unlock:
    pthread_mutex_unlock(&ptr->mutex);

    return value;
}

void memcache_free(memcache_t cache, void *ptr)
{
    struct memcache_node *node;
    struct memcache_s *const mem = cache;

    if (mem == NULL || ptr == NULL) {
        return;
    }

    node = (struct memcache_node *)(ptr - offsetof(struct memcache_node, data));
    pthread_mutex_lock(&mem->mutex);
    list_del(&node->node);
    list_add(&node->node, &node->list->cached);
    pthread_mutex_unlock(&mem->mutex);
}

void memcache_clear(memcache_t cache)
{
    struct memcache_node *node, *n;
    struct memcache_list *list;
    struct memcache_s *const ptr = cache;

    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&ptr->mutex);
    for (list = NULL; (list = findrel234(ptr->list, list, NULL, REL234_GT)) != NULL;) {
        list_for_each_entry_safe(node, n, &list->cached, node) {
            list_del(&node->node);
            free(node);
        }
    }
    pthread_mutex_unlock(&ptr->mutex);
}

void memcache_destroy(memcache_t cache)
{
    struct memcache_node *node, *n;
    struct memcache_list *list;
    struct memcache_s *const ptr = cache;

    if (cache == NULL) {
        return;
    }

    pthread_mutex_destroy(&ptr->mutex);
    for (list = NULL; (list = findrel234(ptr->list, list, NULL, REL234_GT)) != NULL;) {
        list_for_each_entry_safe(node, n, &list->allocated, node) {
            list_del(&node->node);
            free(node);
        }

        list_for_each_entry_safe(node, n, &list->cached, node) {
            list_del(&node->node);
            free(node);
        }
        del234(ptr->list, list);
        free(list);
    }
    freetree234(ptr->list);
    free(ptr);
}
