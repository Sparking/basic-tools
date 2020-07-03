#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stack.h>

#define STREQ(s1, s2)   strcasecmp(s1, s2) == 0

enum cmd_type {
    CMD_SET_UNIT,
    CMD_COREDUMP,
    CMD_PUSH,
    CMD_POP,
    CMD_HELP,
    CMD_QUIT,
    CMD_UNKNOWN,
    CMD_INVALID
};

typedef struct {
    size_t size;
    char content[0];
} mem_node_t;

static ssize_t full_read(int fd, void *ptr, size_t sz)
{
    ssize_t total, current_sz;

    total = 0;
    while (total < sz) {
        current_sz = read(fd, ptr + total, sz - total);
        if (current_sz < 0 || (current_sz == 0 && errno != EINTR)) {
            break;
        }

        total += current_sz;
    }

    return total;
}

static void stack_fill_rand(void *ptr, size_t size)
{
    size_t cnt, off, i;
    int *int_ptr;

    srand(time(NULL));
    int_ptr = (int *)ptr;
    for (cnt = size / sizeof(int), off = 0, i = 0; i < cnt; ++i, off += sizeof(int)) {
        int_ptr[i] = rand(); 
    }

    memset(int_ptr + i, rand(), size - off);
}

static void stack_memset(void *ptr, size_t size)
{
    int fd;
    ssize_t ret;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        stack_fill_rand(ptr, size);
        return;
    }

    ret = full_read(fd, ptr, size);
    if (ret != size) {
        if (ret < 0) {
            ret = 0;
        }

        stack_fill_rand(ptr + ret, size - ret);
    }
    close(fd);
}

static void stack_item_release(void *ptr)
{
    mem_node_t *content;

    if (ptr) {
        content = *(mem_node_t **)ptr;
        printf("release 0x%016zx, size = %zu\n", (size_t)content, content->size);
        free(content);
    } else {
        fprintf(stderr, "null pointer\n");
    }
}

static void stack_make_coredump(stack_t *s)
{
    mem_node_t *ptr;

    stack_pop(s, &ptr);
    free(ptr);
    free(ptr->content);
    free(ptr);
}

static bool push(stack_t *s, const size_t size)
{
    mem_node_t *ptr;

    ptr = (mem_node_t *)malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "malloc(%zu) failed!\n", size);
        return false;
    }

    if (!stack_push(s, &ptr)) {
        fprintf(stderr, "push tem failed\n");
        free(ptr);
        return false;
    }

    stack_memset(ptr, size);
    ptr->size = size;

    return true;
}

static size_t pop(stack_t *s, const size_t count)
{
    size_t i;
    mem_node_t *ptr;

    for (i = 0; i < count; ++i) {
        if (!stack_pop(s, &ptr)) {
            break;
        }

        stack_item_release(&ptr);
    }

    return i;
}

static inline void help(void)
{
    printf("commands list:\n"
           "  help              : show commands help\n"
           "  coredump          : generate coredump\n"
           "  push [item size]  : push an item to stack\n"
           "  pop  [item count] : pop items from stack\n"
           "  unit [unit]       : set unit for memory allocation, options: G, M, K and B\n"
           "  quit              : quit program\n");
    fflush(stdout);
}

int main(void)
{
    stack_t s;
    size_t unit;
    size_t size, i;
    enum cmd_type t;
    char *cmd, xxx[BUFSIZ];
    char *ptr, *arg, *next;

    unit = 1;
    stack_init(&s, sizeof(mem_node_t *), stack_item_release);
    while ((cmd = readline("# ")) != NULL) {
        strncpy(xxx, cmd, sizeof(xxx) - 1);
        size = 0;
        next = cmd;
        t = CMD_INVALID;
        for (i = 0, ptr = NULL; (arg = strtok_r(next, " \n", &ptr)) != NULL; ++i, next = ptr) {
            if (i == 0) {
                if (STREQ(arg, "push")) {
                    t = CMD_PUSH;
                } else if (STREQ(arg, "pop")) {
                    t = CMD_POP;
                } else if (STREQ(arg, "quit")) {
                    t = CMD_QUIT;
                    break;
                } else if (STREQ(arg, "help")) {
                    t = CMD_HELP;
                    break;
                } else if (STREQ(arg, "coredump")) {
                    t = CMD_COREDUMP;
                    break;
                } else if (STREQ(arg, "unit")) {
                    t = CMD_SET_UNIT;
                } else {
                    t = CMD_UNKNOWN;
                    break;
                }
            } else if (i > 2) {
                printf("omit arg '%s'\n", arg);
            } else {
                if (t == CMD_SET_UNIT) {
                    if (STREQ(arg, "G")) {
                        unit = 1 << 30;
                    } else if (STREQ(arg, "M")) {
                        unit = 1 << 20;
                    } else if (STREQ(arg, "K")) {
                        unit = 1 << 10;
                    } else if (STREQ(arg, "B")) {
                        unit = 1;
                    } else {
                        fprintf(stderr, "Invalid unit, options are: G, M, K and B.\n");
                    }
                    continue;
                }

                if (sscanf(arg, "%zu", &size) != 1) {
                    fprintf(stderr, "invalid arg %s is not a positive integer\n", arg);
                    t = CMD_INVALID;
                    break;
                }

                if (size == 0) {
                    fprintf(stderr, "invalid value 0\n");
                    t = CMD_INVALID;
                    break;
                }
            }
        }
        free(cmd);

        if (t == CMD_PUSH) {
            size *= unit;
            if (size == 0) {
                fprintf(stderr, "usage: push <item size>\n");
            } else if (size < sizeof(mem_node_t)) {
                fprintf(stderr, "item size must be bigger than %zu\n", sizeof(mem_node_t));
            } else if (push(&s, size)) {
                printf("Success to push an item, size %zu.\n", size);
            } else {
                fprintf(stderr, "Failed to push an item, size %zu.\n", size);
            }
        } else if (t == CMD_POP) {
            if (size == 0) {
                fprintf(stderr, "usage: pop <item count>\n");
            } else {
                size = pop(&s, size);
                printf("Pop data count: %zu\n", size);
            }
        } else if (t == CMD_QUIT) {
            break;
        } else if (t == CMD_UNKNOWN) {
            fprintf(stderr, "unknown command\n");
            continue;
        } else if (t == CMD_HELP) {
            help();
            continue;
        } else if (t == CMD_COREDUMP) {
            if (stack_empty(&s)) {
                printf("stack is empty, please push 1 item at least.\n");
                continue;
            }

            stack_make_coredump(&s);
        } else if (t == CMD_SET_UNIT) {
            switch (unit) {
            case 1 << 30:
                cmd = "G";
                break;
            case 1 << 20:
                cmd = "M";
                break;
            case 1 << 10:
                cmd = "K";
                break;
            default:
                unit = 1;
                cmd = "B";
                break;
            }
            printf("Unit: %s.\n", cmd);
        }
        add_history(xxx);
    }
    stack_release(&s);

    return 0;
}

