#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include "list.h"
#include "rbtree.h"

#define LINUX_PHYSICAL_MEMORY_MAP_DIR	"/sys/firmware/memmap"
#define LINUX_PHYSICAL_MEMORY_MAP_START "start"
#define LINUX_PHYSICAL_MEMORY_MAP_END   "end"
#define LINUX_PHYSICAL_MEMORY_MAP_TYPE  "type"

#define LINUX_IOMEM_FILE                "/proc/iomem"

#define LINUX_KERNEL_CODE_STRING        "Kernel code"
#define LINUX_KERNEL_DATA_STRING        "Kernel data"
#define LINUX_KERNEL_BSS_STRING         "Kernel bss"

typedef struct {
    struct list_head n;
    size_t start;
    size_t end;
} phy_mem_map_node_range_t;

typedef struct {
    struct rb_node rb;
    struct list_head lst;
    size_t size;
    char name[0];
} phy_mem_map_node_t;

typedef struct {
    struct rb_root total_ram;
    size_t total_ram_size;
    struct rb_root kernel_ram;
    size_t kernel_ram_size;
} phy_mem_map_t;

static int read_string_at_dir(int dir_fd, const char *sub_path, char *buf, const size_t nbuf)
{
    int fd;
    ssize_t size;

    fd = openat(dir_fd, sub_path, O_RDONLY);
    if (fd < 0)
        return -errno;

    size = read(fd, buf, nbuf);
    if (size < 0) {
        size = -errno;
    } else {
        if (buf[size - 1] == '\n') {
            buf[--size] = '\0';
        } else {
            buf[size] = '\0';
        }
    }

    (void) close(fd);

    return size;
}

static int add_mem_map(struct rb_root *root, const char *name, size_t start, size_t end)
{
    int ret;
    struct rb_node **new;
    struct rb_node *parent;
    struct list_head *prev;
    phy_mem_map_node_t *node;
    phy_mem_map_node_range_t *range;
    phy_mem_map_node_range_t *tmp;

    parent = NULL;
    new = &root->rb_node;
    while (*new) {
        parent = *new;
        node = rb_entry(parent, phy_mem_map_node_t, rb);
        ret = strcmp(name, node->name);
        if (ret == 0) {
            goto found;
        } else if (ret < 0) {
            new = &parent->rb_left;
        } else {
            new = &parent->rb_right;
        }
    }

    ret = strlen(name) + 1;
    node = (phy_mem_map_node_t *) malloc(sizeof(*node) + ret);
    if (!node)
        return -errno;

    node->size = 0;
    INIT_LIST_HEAD(&node->lst);
    (void) memcpy(node->name, name, ret);
    rb_link_node(&node->rb, parent, new);
    rb_insert_color(&node->rb, root);
found:
    range = (phy_mem_map_node_range_t *) malloc(sizeof(*range));
    if (range == NULL)
        return -errno;

    prev = &node->lst;
    list_for_each_entry(tmp, &node->lst, n) {
        if (tmp->start > start) {
            prev = &tmp->n;
            break;
        }
    }

    list_add_tail(&range->n, prev);
    node->size += (end - start) + 1;
    range->start = start;
    range->end = end;

    return 0;
}

static int parse_phy_mem_map_total_ram(phy_mem_map_t *map)
{
    int ret;
    int prefix_len;
    char type[64];
    char path[256];
    size_t range[2];
    DIR *root;
    struct dirent *dp;

    root = opendir(LINUX_PHYSICAL_MEMORY_MAP_DIR);
    if (!root)
        return -errno;

    ret = 0;
    map->total_ram_size = 0;
    while ((dp = readdir(root)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;

        prefix_len = snprintf(path, sizeof(path), "%s/", dp->d_name);
        if (prefix_len <= 0 || prefix_len >= sizeof(path)) {
            ret = -errno;
            break;
        }

        (void) strncpy(path + prefix_len, LINUX_PHYSICAL_MEMORY_MAP_START,
            (sizeof(path) - 1) - prefix_len);
        ret = read_string_at_dir(dirfd(root), path, type, sizeof(type));
        if (ret < 0)
            break;

        range[0] = strtoull(type, NULL, 16);
        (void) strncpy(path + prefix_len, LINUX_PHYSICAL_MEMORY_MAP_END,
            (sizeof(path) - 1) - prefix_len);
        ret = read_string_at_dir(dirfd(root), path, type, sizeof(type));
        if (ret < 0)
            break;

        range[1] = strtoull(type, NULL, 16);
        (void) strncpy(path + prefix_len, LINUX_PHYSICAL_MEMORY_MAP_TYPE,
            (sizeof(path) - 1) - prefix_len);
        ret = read_string_at_dir(dirfd(root), path, type, sizeof(type));
        if (ret < 0)
            break;

        map->total_ram_size += (range[1] - range[0]) + 1;
        ret = add_mem_map(&map->total_ram, type, range[0], range[1]);
        if (ret < 0)
            break;
    }
    (void) closedir(root);

    return ret;
}

static int parse_phy_mem_map_kernel_ram(phy_mem_map_t *map)
{
    int ret;
    FILE *fp;
    char *mark;
    size_t r[2];
    char buf[128];

    fp = fopen("/proc/iomem", "r");
    if (fp == NULL)
        return -errno;

    map->kernel_ram_size = 0;
    while ((fgets(buf, sizeof(buf), fp)) != NULL) {
        if (*buf != ' ')
            continue;

        r[0] = strtoull(buf, &mark, 16);
        r[1] = strtoull(mark + 1, &mark, 16);
        mark = strchr(mark, ':') + 2;
        mark[strlen(mark) - 1] = '\0';
        if (strncmp(mark, "Kernel", 6) != 0)
            continue;

        map->kernel_ram_size += (r[1] - r[0]) + 1;
        ret = add_mem_map(&map->kernel_ram, mark, r[0], r[1]);
        if (ret < 0)
            break;
    }
    (void) fclose(fp);

    return ret;
}

static void format_size(size_t size, char *buf, size_t nbuf)
{
    size_t bits;
    size_t mask;
    unsigned int i;
    const char *units[] = {"Bytes", "KiB", "MiB", "GiB"};

    for (i = 0, bits = 10; i < 4; i++, bits += 10) {
        mask = (1ull << bits);
        if (size < mask)
            break;
    }

    if (i == 0) {
        snprintf(buf, nbuf, "%zu %s", size, units[0]);
        return;
    }

    bits -= 10;
    mask = 1ull << bits;
    snprintf(buf, nbuf, "%zu.%02zu %s", size >> bits, (size & (mask - 1)) * 100 / mask, units[i]);
}

static void release_map(struct rb_root *head, FILE *fp)
{
    struct rb_node *rb, *tmp;
    phy_mem_map_node_t *node;
    phy_mem_map_node_range_t *r, *tmpr;
    char buf[32];

    rb = rb_first(head);
    while (rb) {
        tmp = rb;
        rb = rb_next(rb);
        rb_erase(tmp, head);

        node = rb_entry(tmp, phy_mem_map_node_t, rb);
        if (fp) {
            fprintf(fp, "Type  : %s\n", node->name);
            format_size(node->size, buf, sizeof(buf));
            fprintf(fp, "Size  : %s\n", buf);
            fprintf(fp, "Range :\n");
        }

        list_for_each_entry_safe(r, tmpr, &node->lst, n) {
            if (fp)
                fprintf(fp, "  %0*lx-%0*lx\n", (int) sizeof(size_t) * 2, r->start,
                    (int) sizeof(size_t) * 2, r->end);
            /* 不需要摘链表，直接释放 */
            free(r);
        }

        if (fp)
            fputc('\n', fp);

        free(node);
    }
}

static void phy_mem_map_release(phy_mem_map_t *map, const char *file)
{
    FILE *fp;

    fp = NULL;
    if (file)
        fp = fopen(file, "w");

    release_map(&map->total_ram, fp);
    release_map(&map->kernel_ram, fp);

    if (fp) {
        (void) fflush(fp);
        (void) fclose(fp);
    }
}

int main(void)
{
    int ret;
    phy_mem_map_t map;

    map.kernel_ram = RB_ROOT;
    map.kernel_ram_size = 0;
    map.total_ram = RB_ROOT;
    map.total_ram_size = 0;
    ret = parse_phy_mem_map_kernel_ram(&map);
    ret |= parse_phy_mem_map_total_ram(&map);
    if (ret < 0) {
        phy_mem_map_release(&map, NULL);
        return ret;
    }

    phy_mem_map_release(&map, "/workspace/memory_map");

    return ret;
}
