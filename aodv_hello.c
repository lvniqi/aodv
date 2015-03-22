#include "aodv_hello.h"
#include "timer_queue.h"
#include "aodv_timeout.h"
#include "aodv_rrep.h"
#include "aodv_rreq.h"
#include "routing_table.h"
#include "aodv_socket.h"
#include "parameters.h"
#include <memory.h>

static struct timer hello_timer;
extern s32_t receive_n_hellos;

void hello_start(void)
{
	if(hello_timer.used)
		return;

	gettimeofday(&this_host.last_forward_time, NULL);

	printf("Starting to send HELLO!\n");

	timer_init(&hello_timer, hello_send, NULL);

	hello_send(NULL);
}

void hello_stop(void)
{
	printf("Stopping sending HELLO\n");
	timer_remove(&hello_timer);
}

void hello_send(void *arg)
{
	RREP *rrep;
	u8_t flags = 0;
	struct in_addr dest;
	struct timeval now;
	long time_diff;
	
	gettimeofday(&now, NULL);

	if(timeval_diff(&now, &this_host.last_forward_time) > ACTIVE_ROUTE_TIMEOUT)
	{
		hello_stop();
		return;
	}

	time_diff = timeval_diff(&now, &this_host.last_broadcast_time);

	if(time_diff >= HELLO_INTERVAL)
	{
		rrep = rrep_create(flags, 0, 0, this_host.dev.ipaddr, this_host.seqno, this_host.dev.ipaddr, ALLOWED_HELLO_LOSS * HELLO_INTERVAL);

	/////////No ext	
	
		dest.s_addr = AODV_BROADCAST;
		aodv_socket_send((AODV_msg *)rrep, dest, RREP_SIZE, 1, &this_host.dev);

		timer_set_timeout(&hello_timer, HELLO_INTERVAL);
	}
	else
	{
		timer_set_timeout(&hello_timer, HELLO_INTERVAL - time_diff);
	}
}

void hello_process(RREP *hello, s32_t len)
{
	u32_t hello_seqno, timeout, hello_interval = HELLO_INTERVAL;
	u8_t state, flags = 0;
	struct in_addr hello_dest;
	rt_table_t *rt;
	struct  timeval now;

	gettimeofday(&now, NULL);

	hello_dest.s_addr = hello->dest_addr;
	hello_seqno = ntohl(hello->dest_seqno);

	rt = rt_table_check(hello_dest);

	if(rt)
		flags = rt->flags;

	if(receive_n_hellos)
		state = INVALID;
	else
		state = VALID;

	timeout = ALLOWED_HELLO_LOSS * hello_interval + ROUTE_TIMEOUT_SLACK;

	if(!rt)
	{
		rt = rt_table_insert(hello_dest, hello_dest, 1, hello_seqno, timeout, state, flags);

		if(flags & RT_UNIDIR)
		{
			printf("%s new NEIGHBOR, link UNI_DIR\n", inet_ntoa(rt->dest_addr));
		}
		else
		{
			printf("%s new NEIGHBOR!\n", inet_ntoa(rt->dest_addr));
		}

		rt->hello_cnt = 1;
	}
	else
	{
		if((flags & RT_UNIDIR) && rt->state == VALID && rt->hopcnt > 1)
		{
			hello_update_timeout(rt, &now, ALLOWED_HELLO_LOSS *hello_interval);
		}

		if(receive_n_hellos && rt->hello_cnt < (receive_n_hellos - 1))
		{
			if(timeval_diff(&now, &rt->last_hello_time) < (long)(hello_interval + hello_interval / 2))
				rt->hello_cnt++;
			else
				rt->hello_cnt = 1;

			memcpy(&rt->last_hello_time, &now, sizeof(struct timeval));
			return;
		}

		rt_table_update(rt, hello_dest, 1, hello_seqno, timeout, VALID, flags);

		hello_update_timeout(rt, &now, ALLOWED_HELLO_LOSS * hello_interval);
	}	
}

void hello_update_timeout(rt_table_t *rt, struct timeval *now, long time)
{
	timer_set_timeout(&rt->hello_timer, time + HELLO_DELAY);
	memcpy(&rt->last_hello_time, now, sizeof(struct timeval));
}
