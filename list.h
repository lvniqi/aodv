#ifndef _LIST_H_
#define _LIST_H_

#include "defs.h"

typedef struct list_t
{
	struct list_t *prev, *next;
}list_t;

/* 链表遍历 */
#define list_for_each(iter, head) \
    for (iter = (head)->next; iter != (head); iter = iter->next)

/* 链表逆序遍历 */
#define list_for_each_reverse(iter, head) \
    for (iter = (head)->prev; iter != (head); iter = iter->prev)

/* 链表遍历，支持删除操作 */
#define list_for_each_safe(iter, n, head) \
    for (iter = (head)->next, n = iter->next; iter != (head); iter = n, n = iter->next)

/* 求包含此链表的结构体指针 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

void list_init_head(list_t *head);
u8_t list_is_empty(const list_t *head);
u8_t list_unattached(const list_t *head);
void list_add(list_t *head, list_t *node);
void list_push_front(list_t *head, list_t *node);
void list_push_back(list_t *head, list_t *node);
void list_remove(list_t *node);

#endif
