#include <stdlib.h>
#include <string.h>
#include <linkedlist.h>

struct linkedlist_node {
    struct list_head node;
    char data[0];
};

void linkedlist_init(linkedlist_t *restrict list, const unsigned int item_size,
    void (*release)(void *data))
{
    INIT_LIST_HEAD(&list->head);
    list->item_size = item_size;
    list->size = 0;
    list->release = release;
}

bool linkedlist_add(linkedlist_t *restrict list, const void *restrict data)
{
    size_t size;
    struct linkedlist_node *node;

    if (list == NULL || data == NULL) {
        return false;
    }

    size = list->item_size + sizeof(struct linkedlist_node);
    node = (struct linkedlist_node *)malloc(size);
    if (node == NULL) {
        return false;
    }

    (void)memcpy(node->data, data, list->item_size);
    list_add(&node->node, &list->head);
    list->size++;

    return true;
}

bool linkedlist_add_tail(linkedlist_t *restrict list, const void *restrict data)
{
    size_t size;
    struct linkedlist_node *node;

    if (list == NULL || data == NULL) {
        return false;
    }

    size = list->item_size + sizeof(struct linkedlist_node);
    node = (struct linkedlist_node *)malloc(size);
    if (node == NULL) {
        return false;
    }

    (void)memcpy(node->data, data, list->item_size);
    list_add_tail(&node->node, &list->head);
    list->size++;

    return true;
}

bool linkedlist_delete(linkedlist_t *restrict list, void *restrict data)
{
    struct linkedlist_node *node;

    if (list == NULL || data == NULL || linkedlist_size(list) == 0) {
        return false;
    }

    node = list_entry(list->head.next, struct linkedlist_node, node);
    (void)memcpy(data, node->data, list->item_size);
    list_del(&node->node);
    free(node);
    list->size--;

    return true;
}

bool linkedlist_delete_tail(linkedlist_t *restrict list, void *restrict data)
{
    struct linkedlist_node *node;

    if (list == NULL || data == NULL || linkedlist_size(list) == 0) {
        return false;
    }

    node = list_entry(list->head.prev, struct linkedlist_node, node);
    (void)memcpy(data, node->data, list->item_size);
    list_del(&node->node);
    free(node);
    list->size--;

    return true;
}

void linkedlist_release(linkedlist_t *restrict list)
{
    struct linkedlist_node *node, *tmp;

    if (list == NULL || linkedlist_size(list) == 0) {
        return;
    }

    list_for_each_entry_safe(node, tmp, &list->head, node) {
        list_del(&node->node);
        if (list->release) {
            list->release(node->data);
        }

        free(node);
    }

    list->size = 0;
}

void *linkedlist_head(const linkedlist_t *restrict list)
{
    if (list == NULL || linkedlist_size(list) == 0) {
        return NULL;
    }

    return list_entry(list->head.prev, struct linkedlist_node, node)->data;
}

void *linkedlist_tail(const linkedlist_t *restrict list)
{
    if (list == NULL || linkedlist_size(list) == 0) {
        return NULL;
    }

    return list_entry(list->head.next, struct linkedlist_node, node)->data;
}

