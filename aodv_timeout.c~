#include "aodv_timeout.h"
#include "aodv_rreq.h"
#include "list.h"
#include "seek_list.h"
#include "routing_table.h"
#include "parameters.h"
#include "aodv_rerr.h"
#include "aodv_neighbor.h"
#include "aodv_hello.h"
#include "aodv_rrep.h"
#include "aodv_socket.h"
#include "timer_queue.h"

extern s32_t expanding_ring_search, local_repair, delete_period;

void rreq_record_timeout(void *arg)
{
	struct rreq_record *rec = (struct rreq_record *)arg;

	list_remove(&rec->l);
	free(rec); 
}

void rreq_blacklist_timeout(void *arg)
{
	struct rreq_blacklist *bl = (struct rreq_blacklist *)arg;

	list_remove(&bl->l);
	free(bl);
}

void route_discovery_timeout(void *arg)
{
	struct timeval now;
	rt_table_t *rt, *repair_rt;
	seek_list_t *entry = (seek_list_t *)arg;

#define TTL_VALUE entry->ttl

	if(!entry)	
		return;

	gettimeofday(&now, NULL);

	printf("Route discovery timeout %s\n", inet_ntoa(entry->dest_addr));

	if(entry->rreq_cnt < RREQ_RETRIES)
	{
		if(expanding_ring_search)
		{
			if(entry->ttl < TTL_THRESHOLD)
				entry->ttl += TTL_INCREMENT;
			else
				entry->ttl = NET_DIAMETER;	

			timer_set_timeout(&entry->seek_timer, RING_TRAVERSAL_TIME);
		}
		else
		{
			timer_set_timeout(&entry->seek_timer, entry->rreq_cnt * 2 * NET_TRAVERSAL_TIME);
		}

		printf("Seeking %s ttl= %d wait= %d\n", inet_ntoa(entry->dest_addr), entry->ttl, 2 * entry->ttl * NODE_TRAVERSAL_TIME);

		rt = rt_table_check(entry->dest_addr);

		if(rt && (timeval_diff(&rt->rt_timer.timeout, &now) < (2 * NET_TRAVERSAL_TIME)))
			rt_table_update_timeout(rt, 2 * NET_TRAVERSAL_TIME);

		rreq_send(entry->dest_addr, entry->dest_seqno, entry->ttl, entry->flags);

		entry->rreq_cnt++;
	}
	else
	{
		printf("No route found!\n");

		//nl_send_no_route_found_msg(entry->dest_addr);
		
		repair_rt = rt_table_check(entry->dest_addr);

		seek_list_remove(entry);

		if(repair_rt && (repair_rt->flags & RT_REPAIR))
		{
			printf("REPAIR for %s failed!\n", inet_ntoa(repair_rt->dest_addr));
			local_repair_timeout(repair_rt);
		}
	}
}

void local_repair_timeout(void *arg)
{
	rt_table_t *rt;
	struct in_addr rerr_dest;
	RERR *rerr = NULL;

	rt = (rt_table_t *)arg;

	if(!rt)
		return;

	rerr_dest.s_addr = AODV_BROADCAST;

	rt->flags &= ~RT_REPAIR;

	//nl_send_del_route_msg(rt->dest_addr, rt->next_hop, rt->hopcnt);
	

	if(rt->nprenode)//if we have prenode //Write myself
	{
		rerr = rerr_create(0, rt->dest_addr, rt->dest_seqno);
		
		if(rt->nprenode == 1)
			rerr_dest = FIRST_PREC(rt->prenodes)->neighbor;
		else
			rerr_dest.s_addr = AODV_BROADCAST;
		
		aodv_socket_send((AODV_msg *)rerr, rerr_dest, RERR_CALC_SIZE(rerr), 1, &this_host.dev);

		printf("Sending RERR about %s to %s\n", inet_ntoa(rt->dest_addr), inet_ntoa(rerr_dest));
	}

	prenode_list_destroy(rt);

	rt->rt_timer.handler = route_delete_timeout;
	timer_set_timeout(&rt->rt_timer, DELETE_PERIOD);

	printf("%s removed in %d msecs\n", inet_ntoa(rt->dest_addr), DELETE_PERIOD);
}

void route_expire_timeout(void *arg)
{
	rt_table_t *rt;

	rt = (rt_table_t *)arg;

	if(!rt)
		return;

	printf("Route %s down, seqno= %d\n", inet_ntoa(rt->dest_addr), rt->dest_seqno);
	
	if(rt->hopcnt == 1)
		neighbor_link_break(rt);
	else
	{
		rt_table_invalidate(rt);
		prenode_list_destroy(rt);
	}
}

void route_delete_timeout(void *arg)
{
	rt_table_t *rt;

	rt = (rt_table_t *)arg;

	if(!rt)
		return;

	printf("%s delete!\n", inet_ntoa(rt->dest_addr));

	rt_table_delete(rt);
}

void hello_timeout(void *arg)
{
	rt_table_t *rt;
	struct timeval now;

	rt = (rt_table_t *)arg;

	if(!rt)
		return;

	gettimeofday(&now, NULL);

	printf("HELLO FAILURE %s, last HELLO: %ld\n", inet_ntoa(rt->dest_addr), timeval_diff(&now, &rt->last_hello_time));

	if(rt && rt->state == VALID && !(rt->flags & RT_UNIDIR))
	{
		if(local_repair && rt->hopcnt <= MAX_REPAIR_TTL)
		{
			rt->flags |= RT_REPAIR;
			printf("Marking %s for REPAIR!\n", inet_ntoa(rt->dest_addr));
		}

		neighbor_link_break(rt);
	}
}

void rrep_ack_timeout(void *arg)
{
	rt_table_t *rt;

	rt = (rt_table_t *)arg;

	if(!rt)
		return;

	rreq_blacklist_insert(rt->dest_addr);

	printf("%s add in rreq_blacklist\n", inet_ntoa(rt->dest_addr));
}

void wait_on_reboot_timeout(void *arg)
{
	*((s32_t *)arg) = 0;

	printf("Wait on reboot over!!\n");
}
