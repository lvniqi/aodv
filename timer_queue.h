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
		res = ((t1->tv_sec - t2->tv_sec) * 1000000 + t1->tv_usec - t2->tv_usec) / 1000;// 1 sec = 10^6 usec (microsecond)
		return (long) res;
    }
}

static inline s32_t timeval_add_msec(struct timeval *t, unsigned long msec)
{
	unsigned long long add;

	if(!t)
		return -1;

	add = t->tv_usec + (msec * 1000);
	t->tv_sec += add / 1000000;
	t->tv_usec = add % 1000000;

	return 0;
}

void timer_queue_init(void);
s32_t timer_remove(struct timer *t);
void timer_set_timeout(struct timer *t, long msec);
s32_t timer_timeout_now(struct timer *t);
struct timeval *timer_age_queue(void);
s32_t timer_init(struct timer *t, timeout_func_t f, void *data);
void timer_add(struct timer *t);
void timer_timeout(struct timeval *now);

#endif
