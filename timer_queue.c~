#include <sys/time.h>

#include "timer_queue.h"
#include "list.h"
#include "debug.h"

list_t tq = {&tq, &tq};

s32_t timer_init(struct timer *t, timeout_func_t f, void *data)
{
	if(!t)
		return -1;
	
	list_init_head_null(&t->l);
	t->used = 0;
	t->handler = f;
	t->data = data;
	t->timeout.tv_sec = 0;
	t->timeout.tv_usec = 0;

	return 0;	
}

s32_t timer_remove(struct timer *t)
{
	s32_t res = 1;
	
	if(!t)
		return -1;	
	
	if(list_unattached(&t->l))
		res = 0;
	else
		list_remove(&t->l);

	t->used = 0;

	return res;
}

/***************seems no use*******************/
/*
s32_t timer_timeout_now(struct timer *t)
{
	if(timer_remove(t))
	{
		t->handler(t->data);
		
		return 1;
	}

	return -1;
}

long timer_left(struct timer *t)
{
	struct timeval now;

	if(!t)
		return -1;

	gettimeofday(&now, NULL);

	return timeval_diff(&now, &t->timeout);
}
*/
/**********************************************/

void timer_timeout(struct timeval *now)
{
	list_t exptq = {&exptq, &exptq};
	list_t *pos, *tmp;
	struct timer *t;

	list_for_each_safe(pos, tmp, &tq)
	{
		t = (struct timer *)pos;

		if(timeval_diff(&t->timeout, now) > 0)
			break;

		list_remove(&t->l);
		list_push_back(&exptq, &t->l);
	}

	while(!list_is_empty(&exptq))
	{
		struct timer *t = (struct timer *)list_first(&exptq);
		list_remove(&t->l);
		t->used = 0;

		if(t->handler)
		{
			DEBUG(LOG_DEBUG, 0, "%s called", ptr_to_func_name(t->handler));
			t->handler(t->data);
		}
	}
}

void timer_set_timeout(struct timer *t, long msec)
{
	if(t->used)
		timer_remove(t);//Is this necessary?!what will happened if add some new timeout?   I know, it have to be like this,because the list have to sort, so remove and re_add can sort
	
	gettimeofday(&t->timeout, NULL);

	if(msec < 0)
	{
		DEBUG(LOG_WARNING, 0, "Negative timeout");
		return;
	}

	t->timeout.tv_usec += msec * 1000;
	t->timeout.tv_sec += t->timeout.tv_usec / 1000000;
	t->timeout.tv_usec = t->timeout.tv_usec % 1000000;

	timer_add(t);
}

void timer_add(struct timer *t)
{	
	if(!t)
	{
		DEBUG(LOG_WARNING, 0, "NULL timer");
		return;
	}
	if(!t->handler)
	{
		DEBUG(LOG_WARNING, 0, "NULL handler");
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

		list_push_front(pos->prev, &t->l);
	}
}

struct timeval *timer_age_queue(void)
{
	struct timeval now;
	struct timer *t;
	static struct timeval remaining;

	gettimeofday(&now, NULL);

	//fflush(stdout);//?????????????
	
	if(list_is_empty(&tq))
		return NULL;

	timer_timeout(&now);

	if(list_is_empty(&tq))
		return NULL;

	t = (struct timer *)tq.next;

	remaining.tv_usec = (t->timeout.tv_usec - now.tv_usec);
	remaining.tv_sec = (t->timeout.tv_sec - now.tv_sec);

	if(remaining.tv_usec < 0)
	{
		remaining.tv_usec += 1000000;
		remaining.tv_sec -= 1;
	}
	
	return (&remaining);
}
