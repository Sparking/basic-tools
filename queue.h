#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <linkedlist.h>

struct circular_queue {
    unsigned int size;          /* 数据块的个数 */
    unsigned int block_size;    /* 数据块的大小 */
    unsigned int used_size;     /* 已被使用的数据块个数 */
    unsigned int front;         /* 头指针 */
    unsigned int rear;          /* 尾指针 */
    char data[0];               /* 数据块 */
};

/**
 * @brief circular_queue_create 创建一个指定大小的循环队列
 * @param size 数据块的个数
 * @param block_size 数据块的大小
 * @return 返回循环队列指针
 */
extern struct circular_queue *circular_queue_create(const unsigned int size, const unsigned int block_size);

/**
 * @brief circular_queue_used_size 检查队列已使用的大小
 */
extern unsigned int circular_queue_used_size(const struct circular_queue *q);

/**
 * @brief circular_queue_avaiable_size 检查队列可用的剩余空间大小
 */
extern unsigned int circular_queue_avaiable_size(const struct circular_queue *q);

/**
 * @brief circular_queue_full 判断队列是否已满
 */
extern int circular_queue_full(const struct circular_queue *q);

/**
 * @brief circular_queue_empty 判断队列是否为空
 */
extern int circular_queue_empty(const struct circular_queue *q);

/**
 * @brief circular_queue_enque 将数据入队
 * @param q 循环队列
 * @param data 连续的数据首地址
 * @param size 入队数据个数
 * @return 返回入队的数据个数
 */
extern unsigned int circular_queue_enque(struct circular_queue *q, const void *data, const unsigned int size);

/**
 * @brief circular_queue_enque 将数据出队
 * @param q 循环队列
 * @param data 接受数据的地址
 * @param size 出队的数据个数
 * @return 返回出队的数据个数
 */
extern unsigned int circular_queue_deque(struct circular_queue *q, void *data, const unsigned int size);

/**
 * @brief circular_queue_front 只读取队列头的数据, 不出队该数据
 * @param q 循环队列
 * @param data 接受数据的地址
 * @param size 数据个数
 * @return 失败返回0
 */
extern int circular_queue_front(const struct circular_queue *q, void *data, const unsigned int size);

/**
 * @brief circular_queue_rear 只读取队列尾的数据, 不出对该数据
 * @return 失败返回0
 */
extern int circular_queue_rear(const struct  circular_queue *q, void *data);

/**
 * @brief circular_queue_clear 清空整个队列
 */
extern void circular_queue_clear(struct circular_queue *q);

/**
 * @brief circular_queue_free 释放队列占用的内存
 */
extern void circular_queue_free(struct circular_queue *q);

typedef linkedlist_t queue_t;

/**
 * @brief queue_init 初始化
 * @param q 链表
 * @param item_size 链表中每个数据块的大小
 * @param release
 */
__attribute__((always_inline)) static inline void queue_init(queue_t *restrict q,
        const unsigned int item_size, void (*release)(void *data))
{
    linkedlist_init(q, item_size, release);
}

/**
 * @brief queue_size 返回链表中元素的格式, 即双链表的大小
 */
__attribute__((always_inline)) static inline unsigned int queue_size(const queue_t *q)
{
    return linkedlist_size(q);
}

/**
 * @brief queue_enque 入队操作
 * @param q 链表
 * @param data 数据地址
 * @return 返回已入队的数据个数
 */
__attribute__((always_inline)) static inline bool queue_enque(queue_t *restrict q,
        const void *restrict data)
{
    return linkedlist_add(q, data);
}

/**
 * @brief queue_deque 出队操作
 * @param q 链表
 * @param data 接收连续数据的地址
 * @return 返回已出队的数据个数
 */
__attribute__((always_inline)) static inline bool queue_deque(queue_t *restrict q,
        void *restrict data)
{
    return linkedlist_delete_tail(q, data);
}

/**
 * @brief queue_front 读取队列头的元素, 但不出队
 * @param q 链表
 * @return 返回数据的地址
 */
__attribute__((always_inline)) static inline void *queue_front(const queue_t *q)
{
    return linkedlist_head(q);
}

/**
 * @brief queue_clear 清空队列
 * @param q 链表
 */
__attribute__((always_inline)) static inline void queue_release(queue_t *q)
{
    linkedlist_release(q);
}

#endif /* _QUEUE_H_ */

