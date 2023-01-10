#ifndef _ADT_BASIC_LIST_H_
#define _ADT_BASIC_LIST_H_

#include <stddef.h>
#include <stdbool.h>

struct basic_list_head {
    struct basic_list_head *prev;
    struct basic_list_head *next;
};

#define BASIC_LIST_HEAD_INIT(name)          { &(name), &(name) }

#define _basic_list_prefetch(x)             __builtin_prefetch(x)

#define basic_list_entry(ptr, type, member) \
    ((type *)((char *) (ptr)) - offsetof(type, member))

#define basic_list_first_entry(ptr, type, member) \
    basic_list_entry((ptr)->next, type, member)

#define basic_list_for_each(pos, head) \
    for (pos = (head)->next; _basic_list_prefetch(pos->next), pos != (head); \
         pos = pos->next)

#define _basic_list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define basic_list_for_each_prev(pos, head) \
    for (pos = (head)->prev; _basic_list_prefetch(pos->prev), pos != (head); \
            pos = pos->prev)

#define basic_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

#define basic_list_for_each_entry(pos, head, member) \
    for (pos = basic_list_entry((head)->next, typeof(*pos), member);  \
         _basic_list_prefetch(pos->member.next), &pos->member != (head); \
         pos = basic_list_entry(pos->member.next, typeof(*pos), member))

#define basic_list_for_each_entry_reverse(pos, head, member) \
    for (pos = basic_list_entry((head)->prev, typeof(*pos), member); \
         _basic_list_prefetch(pos->member.prev), &pos->member != (head); \
         pos = basic_list_entry(pos->member.prev, typeof(*pos), member))

#define basic_list_for_each_entry_safe(pos, n, head, member) \
    for (pos = basic_list_entry((head)->next, typeof(*pos), member), \
         n = basic_list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = basic_list_entry(n->member.next, typeof(*n), member))

#define basic_list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = basic_list_entry((head)->prev, typeof(*pos), member), \
         n = basic_list_entry(pos->member.prev, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = basic_list_entry(n->member.prev, typeof(*n), member))

static inline void basic_list_head_init(struct basic_list_head *entry)
{
    entry->prev = NULL;
    entry->next = NULL;
}

static inline void _basic_list_add(struct basic_list_head *newn, struct basic_list_head *prev,
                                   struct basic_list_head *next)
{
    next->prev = newn;
    newn->next = next;
    newn->prev = prev;
    prev->next = newn;
}

static inline void basic_list_add(struct basic_list_head *newn, struct basic_list_head *head)
{
    _basic_list_add(newn, head, head->next);
}

static inline void basic_list_add_tail(struct basic_list_head *newn, struct basic_list_head *head)
{
    _basic_list_add(newn, head->prev, head);
}

static inline void _basic_list_del(struct basic_list_head *prev, struct basic_list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void basic_list_del(struct basic_list_head *entry)
{
    _basic_list_del(entry->prev, entry->next);
}

static inline void basic_list_del_init(struct basic_list_head *entry)
{
    _basic_list_del(entry->prev, entry->next);
    basic_list_head_init(entry);
}

static inline bool basic_list_empty(const struct basic_list_head *head)
{
    return head->next == head;
}

static inline bool basic_list_is_last(const struct basic_list_head *entry,
                                      const struct basic_list_head *head)
{
    return entry->next == head;
}

#endif /* _ADT_BASIC_LIST_H_ */
