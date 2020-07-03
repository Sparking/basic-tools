#ifndef _STACK_H_
#define _STACK_H_

#include <linkedlist.h>

typedef linkedlist_t stack_t;

/**
 * @brief stack_init 栈初始化
 * @param s 栈
 * @param item_size 栈中每个元素的大小
 * @param release 元素释放函数
 */
__attribute__((always_inline)) static inline void stack_init(stack_t *restrict s,
        const unsigned int item_size, void (*release)(void *data))
{
    linkedlist_init(s, item_size, release);
}

/**
 * @brief stack_empty 判断栈是否为空
 */
__attribute__((always_inline)) static inline bool stack_empty(const stack_t *s)
{
    return linkedlist_size(s) == 0;
}

/**
 * @brief stack_top 返回栈顶元素的地址
 * @return 空栈返回NULL, 否则返回栈顶元素的地址
 */
__attribute__((always_inline)) static inline void *stack_top(const stack_t *restrict s)
{
    return linkedlist_tail(s);
}

/**
 * @brief stack_push 将一个元素入栈
 * @param data 入栈的元素地址
 */
__attribute__((always_inline)) static inline bool stack_push(stack_t *restrict s,
        const void *restrict data)
{
    return linkedlist_add(s, data);
}

/**
 * @brief stack_pop 将栈顶的元素出栈
 * @param data 元素出栈的地址。设置为NULL, 表示元素出栈但不保存
 */
__attribute__((always_inline)) static inline bool stack_pop(stack_t *restrict s,
        void *restrict data)
{
    return linkedlist_delete(s, data);
}

/**
 * @brief stack_clear 清空栈
 */
__attribute__((always_inline)) static inline void stack_release(stack_t *restrict s)
{
    linkedlist_release(s);
}

#endif /* _STACK_H_ */

