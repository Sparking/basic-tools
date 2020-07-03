#include <stdlib.h>
#include <string.h>

#include <queue.h>
#include <port_memory.h>

struct circular_queue *circular_queue_create(const unsigned int size, const unsigned int block_size)
{
    const unsigned int queue_size = sizeof(struct circular_queue) + size * block_size;
    struct circular_queue *q;

    if (queue_size == sizeof(struct circular_queue))
        return NULL;

    q = (struct circular_queue *)mem_alloc(queue_size);
    if (q == NULL)
        return NULL;

    q->size = size;
    q->block_size = block_size;
    q->used_size = 0;
    q->front = 0;
    q->rear = 0;

    return q;
}

int circular_queue_full(const struct circular_queue *q)
{
    if (q == NULL)
        return 0;

    return q->size == q->used_size;
}

int circular_queue_empty(const struct circular_queue *q)
{
    if (q == NULL)
        return 0;

    return q->used_size == 0;
}

unsigned int circular_queue_used_size(const struct circular_queue *q)
{
    if (q == NULL)
        return 0;

    return q->used_size;
}

unsigned int circular_queue_avaiable_size(const struct circular_queue *q)
{
    if (q == NULL)
        return 0;

    return q->size - q->used_size;
}

unsigned int circular_queue_enque(struct circular_queue *q, const void *data, const unsigned int size)
{
    unsigned int i;

    if (q == NULL || data == NULL)
        return 0;

    for (i = 0; i < size; ++i) {
        if (circular_queue_full(q))
            break;

        memcpy(q->data + q->rear * q->block_size, data, q->block_size);
        data = (const char *)data + q->block_size;
        q->rear = (q->rear + 1) % q->size;
        ++q->used_size;
    }

    return i;
}

unsigned int circular_queue_deque(struct circular_queue *q, void *data, const unsigned int size)
{
    unsigned int i;

    if (q == NULL)
        return 0;

    for (i = 0; i < size; ++i) {
        if (circular_queue_empty(q))
            break;

        if (data != NULL) {
            memcpy(data, q->data + q->front * q->block_size, q->block_size);
            data = (char *)data + q->block_size;
        }

        q->front = (q->front + 1) % q->size;
        --q->used_size;
    }

    return i;
}

int circular_queue_front(const struct circular_queue *q, void *data, const unsigned int size)
{
    unsigned int i, j;

    if (q == NULL)
        return 0;

    if (circular_queue_empty(q))
        return 0;

    for (i = 0, j = q->front; i < size; ++i) {
        if (circular_queue_empty(q))
            break;

        memcpy(data, q->data +j * q->block_size, q->block_size);
        data = (char *)data + q->block_size;
        j = (j + 1) % q->size;
    }

    return 1;
}

int circular_queue_rear(const struct circular_queue *q, void *data)
{
    unsigned int pos;

    if (q == NULL)
        return 0;

    if (circular_queue_empty(q))
        return 0;

    pos = (q->rear == 0 ? q->size : q->rear) - 1;
    memcpy(data, q->data + pos * q->block_size, q->block_size);

    return 1;
}

void circular_queue_clear(struct circular_queue *q)
{
    if (q == NULL)
        return;

    q->used_size = 0;
    q->front = 0;
    q->rear = 0;
}

void circular_queue_free(struct circular_queue *q)
{
    mem_free(q);
}
