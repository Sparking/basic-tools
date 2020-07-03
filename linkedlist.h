#ifndef _LINKEDLIST_H_
#define _LINKEDLIST_H_

#include <list.h>
#include <stdbool.h>

struct linkedlist_s {
    struct list_head head;          /* 链表头 */
    unsigned int item_size;         /* 元素大小 */
    unsigned int size;              /* 链表元素个数 */
    void (*release)(void *data);    /* 元素删除处理函数 */
};
typedef struct linkedlist_s linkedlist_t;

/**
 * @brief linkedlist_size 返回链表的大小
 */
__attribute__((always_inline)) static inline unsigned int linkedlist_size(
        const linkedlist_t *restrict list)
{
    return list->size;
}

/**
 * @brief linkedlist_init 初始化链表
 */
extern void linkedlist_init(linkedlist_t *restrict list, const unsigned int item_size,
    void (*release)(void *data));

/**
 * @brief linkedlist_add 向链表插入一个数据
 */
extern bool linkedlist_add(linkedlist_t *restrict list, const void *restrict data);

/**
 * @brief linkedlist_add_tail 向链表尾插入一个数据
 */
extern bool linkedlist_add_tail(linkedlist_t *restrict list, const void *restrict data);

/**
 * @brief linkedlist_delete 删除链表第一个元素, 保存到data中
 */
extern bool linkedlist_delete(linkedlist_t *restrict list, void *restrict data);

/**
 * @brief linkedlist_delete_tail 删除链表尾部的最后一个元素, 保存到data中
 */
extern bool linkedlist_delete_tail(linkedlist_t *restrict list, void *restrict data);

/**
 * @brief linkedlist_release 清空整个链表
 */
extern void linkedlist_release(linkedlist_t *restrict list);

/**
 * @brief 查看链表头的元素，但不取出
 */
extern void *linkedlist_head(const linkedlist_t *restrict list);

/**
 * @brief 查看链表尾的元素，但不取出
 */
extern void *linkedlist_tail(const linkedlist_t *restrict list);

#endif /* _LINKEDLIST_H_ */

