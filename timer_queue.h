#ifndef _TIMER_QUEUE_H_
#define _TIMER_QUEUE_H_

#include "defs.h"
#include "list.h"
#include <sys/time.h>

typedef void (*timeout_func_t)(void *);

struct timer//timer_t conflicting declaration of sys-define
{
	list_t l;
	u8_t used;
	struct timeval timeout;
	timeout_func_t handler;
	void *data;
};

static inline long timeval_diff(struct timeval *t1, struct timeval *t2)
{
    long long res;		// avoid overflows while calculating

    if (!t1 || !t2)
		return -1;
    else 
	{
		res = ((t1->tv_sec - t2->tv_sec) * 1000 + t1->tv_usec - t2->tv_usec) / 1000;//My param is not the param in example???????
		return (long) res;
    }
}

s32_t timer_init(struct timer *t, timeout_func_t f, void *data);
void timer_set_timeout(struct timer *t, long msec);
s32_t timer_remove(struct timer *t);
void timer_add(struct timer *t);

#endif
