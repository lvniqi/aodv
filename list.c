#include "list.h"

void list_init_head(list_t *head)//Init list_head
{
    head->prev = head->next = head;
}

void list_init_head_null(list_t *head)
{
	head->prev = head->next = NULL;
}

u8_t list_is_empty(const list_t *head)
{
    return (head->prev == head) && (head->next == head);
}

u8_t list_unattached(const list_t *head)
{
	return (head->prev == NULL) && (head->next == NULL);
}

static void list_insert(list_t *node, list_t *prev, list_t *next)
{
    prev->next = next->prev = node;
    node->prev = prev;
    node->next = next;
}

void list_add(list_t *head, list_t *node)
{
	list_insert(node, head, head->next);
}

void list_push_front(list_t *head, list_t *node)
{
    list_insert(node, head, head->next);
}

void list_push_back(list_t *head, list_t *node)
{
    list_insert(node, head->prev, head);
}

static void list_delete(list_t *prev, list_t *next)
{
    prev->next = next;
    next->prev = prev;
}

void list_remove(list_t *node)
{
    list_delete(node->prev, node->next);
    node->prev = node->next = NULL;
}

/*
 链表接合 将list接合到head,新链表的头节点仍为head/
void list_splice(list_t *head, list_t *list)
{
    if (list_is_empty(list) == 0)
    {
        list->next->prev = head;
        head->prev = list->next;

        list->prev->next = head->next;
        head->next->prev = list->prev;
    }
}
*/
