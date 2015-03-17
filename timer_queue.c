#include "timer_queue.h"
#include "list.h"
#include <sys/time.h>

list_t tq = {&tq, &tq};

s32_t timer_init(struct timer *t, timeout_func_t f, void *data)
{
	if(!t)
		return -1;
	
	list_init_head(&t->l);
	t->used = 0;
	t->handler = f;
	t->data = data;
	t->timeout.tv_sec = 0;
	t->timeout.tv_usec = 0;

	return 0;	
}

void timer_set_timeout(struct timer *t, long msec)
{
//	if(t->used)
//		timer_remove(t);//Is this necessary?!what will happened if add some new timeout?
	
	gettimeofday(&t->timeout, NULL);

	if(msec < 0)
	{
		printf("Negative timeout!\n");
		return;
	}

	t->timeout.tv_sec = msec / 1000;
	t->timeout.tv_usec = (msec - t->timeout.tv_sec * 1000) * 1000;

	timer_add(t);
}

s32_t timer_remove(struct timer *t)
{
	if(!t)
		return -1;	
	
	t->used = 0;

	if(list_unattached(&t->l))
		;
	else
		list_remove(&t->l);

	return 0;
}

void timer_add(struct timer *t)
{	
	if(!t)
	{
		printf("NULL timer!\n");
		return;
	}
	if(!t->handler)
	{
		printf("NULL handler!\n");
		return;
	}

	if(t->used)
		return;
	
	t->used = 1;
	
	if(list_is_empty(&tq))
		list_push_front(&tq, &t->l);	
	else
	{
		list_t *pos = NULL;
		struct timer *curr = NULL;

		list_for_each(pos, &tq)
		{
			curr = (struct timer *)pos;
			if(timeval_diff(&t->timeout, &curr->timeout) < 0)
				break;
		}

		list_add(pos->prev, &t->l);
	}
}