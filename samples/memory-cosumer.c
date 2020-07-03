#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stack.h>

typedef struct {
    size_t size;
    void *ptr;
} mem_node_t;

static void stack_item_release(void *ptr)
{
    mem_node_t *content;

    if (ptr) {
        content = (mem_node_t *)ptr;
        printf("release 0x%016zx, size = %zu\n", (size_t)content->ptr, content->size);
        free(content->ptr);
    } else {
        fprintf(stderr, "null pointer\n");
    }
}

static bool push(stack_t *s, const size_t size)
{
    mem_node_t node;

    node.ptr = malloc(size);
    if (node.ptr == NULL) {
        fprintf(stderr, "malloc(%zu) failed!\n", size);
        return false;
    }

    node.size = size;
    if (!stack_push(s, &node)) {
        fprintf(stderr, "push tem failed\n");
        free(node.ptr);
        return false;
    }

    memset(node.ptr, size & 0x0FF, size);
    return true;
}

__attribute__((unused)) static size_t pop(stack_t *s, const size_t count)
{
    size_t i;
    mem_node_t node;

    for (i = 0; i < count; ++i) {
        if (!stack_pop(s, (void *)&node)) {
            break;
        }

        stack_item_release(&node);
    }

    return i;
}

int main(int argc, char *argv[])
{
    stack_t s;
    int second;
    size_t size;

    if (argc < 3) {
        fprintf(stderr, "usage: <%s> <interval seconds> <block size>\n", argv[0]);
        return -1;
    }

    second = atoi(argv[1]);
    size = atoi(argv[2]);
    stack_init(&s, sizeof(mem_node_t), stack_item_release);
    while (1) {
        if (!push(&s, size) && size != 0) {
            sleep(second);
        }
    }
    stack_release(&s);

    return 0;
}

