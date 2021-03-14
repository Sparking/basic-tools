#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

typedef struct {
    struct list_head n;
    int cpus_mask;
    char type[0];
} cpu_bind_info_t;

static int read_cpu_bind_info(struct list_head *h, const char *file)
{
    int i;
    unsigned int b;
    FILE *fp;
    char *ptr;
    char *mark;
    char *word;
    char buf[256];
    cpu_bind_info_t *info;

    fp = fopen(file, "r");
    if (!fp)
        return -errno;

    i = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (*buf != '[')
            continue;

        ptr = strrchr(buf + 1, ']');
        if (ptr == NULL) {
            i = -EINVAL;
            break;
        }

        info = (cpu_bind_info_t *) malloc(sizeof(cpu_bind_info_t) + (ptr - (buf + 1)));
        if (!info) {
            i = -errno;
            break;
        }

        *ptr = '\0';
        (void) memcpy(info->type, buf + 1, ptr - (buf + 1));
        ptr++;
        info->cpus_mask = 0;
        mark = NULL;
        for (; (word = strtok_r(ptr, ",", &mark)) != NULL; ptr = NULL) {
            b = atoi(word);
            info->cpus_mask |= 1u << b;
        }
        list_add_tail(&info->n, h);

        i++;
    }
    (void) fclose(fp);

    return i;
}

static void show_list(unsigned int mask)
{
    unsigned int i;
    unsigned int next;

    for (i = 0; mask; i++) {
        next = mask >> 1;
        if (mask & 1) {
            printf("%d", i);
            if (next)
                printf(",");
        }
        mask = next;
    }
}


static int mask2list(char *buf, size_t nbuf, char delc, unsigned int mask)
{
    char *ptr;
    int ret;
    int left;
    unsigned int i;
    unsigned int next;

    ptr = buf;
    left = nbuf;
    for (i = 0; mask; i++) {
        next = mask >> 1;
        if (mask & 1) {
            ret = snprintf(ptr, left, "%d", i);
            if (ret >= left || ret < 0)
                return -1;

            ptr += ret;
            left -= ret;
            if (next) {
                if (left < 1)
                    return -1;

                *ptr++ = ',';
                *ptr = '\0';
                left -= 1;
            }
        }
        mask = next;
    }

    return 0;
}

static void show(struct list_head *h)
{
    cpu_bind_info_t *info, *tmp;
    char buf[256];

    list_for_each_entry_safe(info, tmp, h, n) {
        printf("%-18s : ", info->type);
        show_list(info->cpus_mask);
        mask2list(buf, sizeof(buf), ',', info->cpus_mask);
        printf("\n\t%s\n", buf);
        free(info);
    }

    INIT_LIST_HEAD(h);
}

int main(int argc, char *argv[])
{
    int n;
    struct list_head h;

    if (argc <= 1)
        return -1;

    INIT_LIST_HEAD(&h);
    n = read_cpu_bind_info(&h, argv[1]);
    if (n < 0) {
        return n;
    }

    show(&h);

    return 0;
}
