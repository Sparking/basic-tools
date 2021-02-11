#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "rbtree.h"
#include "memcache.h"

struct memcache_list {
    size_t alloc_size;
    struct rb_node rb;
    struct hlist_head using;
    struct hlist_head cached;
};

struct memcache_node {
    struct memcache_list *list;
    struct hlist_node node;
    char data[0];
};

struct memcache_root {
    pthread_mutex_t mutex;
    struct rb_root root;
};

memcache_t memcache_create(void)
{
    struct memcache_root *root;

    root = (struct memcache_root *) malloc(sizeof(*root));
    if (root) {
        root->root = RB_ROOT;
        pthread_mutex_init(&root->mutex, NULL);
    }

    return root;
}

void *memcache_alloc(memcache_t cache, const size_t size)
{
    struct rb_node **rb;
    struct rb_node *parent;
    struct memcache_node *mem;
    struct memcache_list *list;
    struct memcache_root *root;

    if (!cache || !size)
        return NULL;

    root = (struct memcache_root *) cache;
    pthread_mutex_lock(&root->mutex);
    parent = NULL;
    rb = &root->root.rb_node;
    while (*rb != NULL) {
        parent = *rb;
        list = rb_entry(parent, struct memcache_list, rb);
        if (list->alloc_size == size) {
            goto drop_cache;
        } else if (list->alloc_size < size) {
            rb = &parent->rb_left;
        } else {
            rb = &parent->rb_right;
        }
    }

    list = (struct memcache_list *) malloc(sizeof(struct memcache_list));
    if (!list) {
        pthread_mutex_unlock(&root->mutex);
        return NULL;
    }

    list->alloc_size = size;
    INIT_HLIST_HEAD(&list->using);
    INIT_HLIST_HEAD(&list->cached);
    rb_insert_color(&list->rb, &root->root);
    rb_link_node(&list->rb, parent, rb);
    goto new_node;

drop_cache:
    if (!hlist_empty(&list->cached)) {
        mem = hlist_entry(list->cached.first, struct memcache_node, node);
        hlist_del(&mem->node);
        goto finish;        
    }

new_node:
    mem = (struct memcache_node *) malloc(sizeof(struct memcache_node) + size);
    if (!mem) {
        pthread_mutex_unlock(&root->mutex);
        return NULL;
    }

    mem->list = list;
finish:
    hlist_add_head(&mem->node, &list->using);
    pthread_mutex_unlock(&root->mutex);

    return mem->data;
}

void memcache_free(void *ptr)
{
    struct memcache_node *mem;

    if (ptr) {
        mem = container_of(ptr, struct memcache_node, data);
        hlist_del(&mem->node);
        hlist_add_head(&mem->node, &mem->list->cached);
    }
}

static void memcache_list_release(struct hlist_head *head)
{
    struct hlist_node *n;
    struct memcache_node *node;

    hlist_for_each_entry(node, n, head, node)
        free(node);

    INIT_HLIST_HEAD(head);
}

void memcache_clear(memcache_t cache, bool del_empty_list)
{
    struct rb_node *rb;
    struct memcache_list *list;
    struct memcache_root *root;

    if (cache) {
        root = (struct memcache_root *) cache;
        pthread_mutex_lock(&root->mutex);
        for (rb = rb_first(&root->root); rb;) {
            list = rb_entry(rb, struct memcache_list, rb);
            rb = rb_next(rb);
            memcache_list_release(&list->cached);
            if (del_empty_list && hlist_empty(&list->using)) {
                rb_erase(&list->rb, &root->root);
                free(list);
            }
        }
        pthread_mutex_unlock(&root->mutex);
    }
}

void memcache_destroy(memcache_t cache)
{
    struct rb_node *rb;
    struct memcache_list *list;
    struct memcache_root *root;

    if (cache) {
        root = (struct memcache_root *) cache;
        pthread_mutex_destroy(&root->mutex);
        for (rb = rb_first(&root->root); rb;) {
            list = rb_entry(rb, struct memcache_list, rb);
            rb = rb_next(rb);
            rb_erase(&list->rb, &root->root);
            memcache_list_release(&list->cached);
            memcache_list_release(&list->using);
            free(list);
        }
        free(root);
    }
}
