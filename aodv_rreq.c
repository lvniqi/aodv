#include <netinet/in.h>
#include <sys/time.h>

#include "aodv_rreq.h"
#include "aodv_socket.h"
#include "routing_table.h"
#include "timer_queue.h"
#include "parameters.h"
#include "list.h"
#include "seek_list.h"
#include "aodv_rrep.h"
#include "aodv_timeout.h"
#include "debug.h"

extern s32_t expanding_ring_search;

static list_t rreq_records = {&rreq_records, &rreq_records};
static list_t rreq_blacklists = {&rreq_blacklists, &rreq_blacklists};

RREQ *rreq_create(u8_t flags, struct in_addr dest_addr, u32_t dest_seqno, struct in_addr orig_addr)
{
	RREQ *rreq;

	rreq = (RREQ *)aodv_socket_new_msg();
	rreq->type = AODV_RREQ;	
	rreq->res1 = 0;
	rreq->res2 = 0;
	rreq->hopcnt = 0;
	rreq->rreq_id = htonl(this_host.rreq_id++);//this_host.rreq_id++ or ++this_host.rreq_id? same?
	rreq->dest_addr = dest_addr.s_addr;
	rreq->dest_seqno = htonl(dest_seqno);
	rreq->orig_addr = orig_addr.s_addr;

	seqno_incr(this_host.seqno);
	rreq->orig_seqno = htonl(this_host.seqno);

	if(flags & RREQ_JOIN)
		rreq->j = 1;
	if(flags & RREQ_REPAIR)
		rreq->r = 1;
	if(flags & RREQ_GRATUITOUS)
		rreq->g = 1;
	if(flags & RREQ_DEST_ONLY)
		rreq->d = 1;
	if(flags & RREQ_UNKNOWN_SEQNO)
		rreq->u = 1;

	return rreq;
}

void rreq_send(struct in_addr dest_addr, u32_t dest_seqno, s32_t ttl, u8_t flags)
{
	RREQ *rreq;
	struct in_addr dest;
	
	dest.s_addr = AODV_BROADCAST;

	////////////////////////////////////////////////////////////
	//Don`t konw if there should be a forcing -g option judgement now
	///////////////////////////////////////////////////////////
	
	if(this_host.dev.enabled)
	{
		rreq = rreq_create(flags, dest_addr, dest_seqno, this_host.dev.ipaddr);
		aodv_socket_send((AODV_msg *)rreq, dest, RREQ_SIZE, ttl, &this_host.dev); 
	} 
	else
		DEBUG(LOG_DEBUG, 0, "The device is not activited!");	
}

void rreq_forward(RREQ *rreq, s32_t len, s32_t ttl)
{
	struct in_addr dest, orig;

	dest.s_addr = AODV_BROADCAST;
	orig.s_addr = rreq->orig_addr;

	DEBUG(LOG_INFO, 0, "forwarding RREQ src= %s, rreq_id= %d", ip_to_str(orig), ntohl(rreq->rreq_id));	

	rreq = (RREQ *)aodv_socket_queue_msg((AODV_msg *)rreq, len);

	rreq->hopcnt++;

	aodv_socket_send((AODV_msg *) rreq, dest, len, ttl, &this_host.dev);
}

void rreq_process(RREQ *rreq, s32_t len, struct in_addr ip_src, struct in_addr ip_dest, s32_t ip_ttl)
{
	struct in_addr rreq_dest, rreq_orig;
	RREP *rrep;
	s32_t rrep_size = RREP_SIZE;
	u32_t rreq_id, rreq_new_hopcnt, life;
	u32_t rreq_orig_seqno, rreq_dest_seqno;
	rt_table_t *rev_rt, *fwd_rt = NULL;
	
	rreq_dest.s_addr = rreq->dest_addr;
	rreq_orig.s_addr = rreq->orig_addr;
	rreq_id = ntohl(rreq->rreq_id);
	rreq_dest_seqno = ntohl(rreq->dest_seqno);
	rreq_orig_seqno = ntohl(rreq->orig_seqno);
	rreq_new_hopcnt = rreq->hopcnt + 1;

	if(rreq_orig.s_addr == this_host.dev.ipaddr.s_addr)
	{
		DEBUG(LOG_DEBUG, 0, "The RREQ was sended by us");
		return;
	}

//	DEBUG(LOG_DEBUG, 0, "ip_src= %s, rreq_orig= %s, rreq_dest= %s, rreq_id= %d, ttl= %d, dest_seqno= %d, orig_seqno= %d", ip_to_str(ip_src), ip_to_str(rreq_orig), ip_to_str(rreq_dest), rreq_id, ip_ttl, rreq_dest_seqno, rreq_orig_seqno);

	if(len < (s32_t)RREQ_SIZE)
	{
		alog(LOG_WARNING, 0, __FUNCTION__, "IP data too short, from %s to %s", ip_to_str(ip_src), ip_to_str(ip_dest));
		return;
	}

	if(rreq_blacklist_check(ip_src))
	{
		DEBUG(LOG_DEBUG, 0, "prev hop of RREQ is in the blacklist, ignoring!");
		return;
	}

	if(rreq_record_check(rreq_orig, rreq_id))
	{
		DEBUG(LOG_DEBUG, 0, "Receive RREQ buffered already!");
		return;
	}

	rreq_record_insert(rreq_orig, rreq_id);
	
	
	/*
	 *
	 * we can check if there is any extension sended with the RREQ package, but I think I don`t want to add some new extension now, so write the check code when I use it.
	 *
	 *
	 */

	life = PATH_DISCOVERY_TIME - 2 * rreq_new_hopcnt * NODE_TRAVERSAL_TIME;

	rev_rt = rt_table_check(rreq_orig);
	
	if(rev_rt == NULL)
	{
		DEBUG(LOG_DEBUG, 0, "Creating REVERSE route entry, RREQ orig: %s", ip_to_str(rreq_orig));
		rev_rt = rt_table_insert(rreq_orig, ip_src, rreq_new_hopcnt, rreq_orig_seqno, life, VALID, 0);
	}	
	else
	{
		if(rev_rt->dest_seqno == 0 || (s32_t)rreq_orig_seqno > (s32_t)rev_rt->dest_seqno || (rreq_orig_seqno == rev_rt->dest_seqno && (rev_rt->state == INVALID || rreq_new_hopcnt < rev_rt->hopcnt)))
		{
			rev_rt = rt_table_update(rev_rt, ip_src, rreq_new_hopcnt, rreq_orig_seqno, life, VALID, rev_rt->flags);
		}
	}
	
	////////////
	///////////
	
	if(rreq_dest.s_addr == this_host.dev.ipaddr.s_addr)//We are the destination
	{
		if(rreq_dest_seqno != 0)
		{
			if((s32_t)this_host.seqno < (s32_t)rreq_dest_seqno)
				this_host.seqno = rreq_dest_seqno;
			else if(this_host.seqno == rreq_dest_seqno)
				seqno_incr(this_host.seqno);
		}

		rrep = rrep_create(0, 0, 0, this_host.dev.ipaddr, this_host.seqno, rev_rt->dest_addr, MY_ROUTE_TIMEOUT);
		rrep_send(rrep, rev_rt, NULL, RREP_SIZE);
	}
	else
	{
		fwd_rt = rt_table_check(rreq_dest);
		if(fwd_rt && fwd_rt->state == VALID && !rreq->d)
		{
			struct timeval now;
			u32_t lifetime;

			gettimeofday(&now, NULL);
			if(fwd_rt->dest_seqno != 0 && (s32_t)fwd_rt->dest_seqno >= (s32_t)rreq_dest_seqno)
			{
				lifetime = timeval_diff(&fwd_rt->rt_timer.timeout, &now);
				rrep = rrep_create(0, 0, fwd_rt->hopcnt, fwd_rt->dest_addr, fwd_rt->dest_seqno, rev_rt->dest_addr, lifetime);
				rrep_send(rrep, rev_rt, fwd_rt, rrep_size);
			}
			else
			{
				goto forward;
			}

			if(rreq->g)
			{
				rrep = rrep_create(0, 0, rev_rt->hopcnt, rev_rt->dest_addr, rev_rt->dest_seqno, fwd_rt->dest_addr, lifetime);
				rrep_send(rrep, fwd_rt, rev_rt, RREP_SIZE);

				DEBUG(LOG_DEBUG, 0, "Sending G_RREP to %s with rte to %s", ip_to_str(rreq_dest), ip_to_str(rreq_orig));
			}

			return;
		}

forward:
		if(ip_ttl > 1)
		{
			if(fwd_rt && !(fwd_rt->flags & RT_INET_DEST) && (s32_t)fwd_rt->dest_seqno > (s32_t)rreq_dest_seqno)
			rreq->dest_seqno = htonl(fwd_rt->dest_seqno);

			rreq_forward(rreq, len, --ip_ttl);
		}
		else
		{
			DEBUG(LOG_WARNING, 0, "RREQ not forwarded");
		}
	}
}

struct rreq_record *rreq_record_insert(struct in_addr orig_addr, u32_t rreq_id)
{
	struct rreq_record *rec;

	rec = rreq_record_check(orig_addr, rreq_id);

	if(rec)
	{
		DEBUG(LOG_DEBUG, 0, "RREQ record already exsited");
		return rec;
	}

	if((rec = (struct rreq_record *)malloc(sizeof(struct rreq_record))) == NULL)
	{
		DEBUG(LOG_WARNING, 0, "RREQ record insert malloc failed");
		perror("");
		exit(-1);
	}

	rec->orig_addr = orig_addr;
	rec->rreq_id = rreq_id;

	timer_init(&rec->rec_timer, rreq_record_timeout, rec);

	list_push_front(&rreq_records, &rec->l);

	DEBUG(LOG_DEBUG, 0, "Buffering RREQ %s rreq_id= %d time= %u", ip_to_str(orig_addr), rreq_id, PATH_DISCOVERY_TIME);

	timer_set_timeout(&rec->rec_timer, PATH_DISCOVERY_TIME);

	return rec;
}

struct rreq_record *rreq_record_check(struct in_addr orig_addr, u32_t rreq_id)
{
	list_t *pos;
	struct rreq_record *curr;
	
	list_for_each(pos, &rreq_records)
	{
		curr = (struct rreq_record *)pos;
		if(curr->orig_addr.s_addr == orig_addr.s_addr && curr->rreq_id == rreq_id)
			return curr;
	}

	return NULL;
}

struct rreq_blacklist *rreq_blacklist_insert(struct in_addr dest_addr)
{
	struct rreq_blacklist *bl;

	bl = rreq_blacklist_check(dest_addr);

	if(bl)
	{
		DEBUG(LOG_DEBUG, 0, "Blacklist record already exsited");
		return bl;
	}

	if((bl = (struct rreq_blacklist *)malloc(sizeof(struct rreq_blacklist))) == NULL)
	{
		DEBUG(LOG_WARNING, 0, "Blacklist record insert malloc failed");
		perror("");
		exit(-1);
	}

	bl->dest_addr = dest_addr;

	timer_init(&bl->bl_timer, rreq_blacklist_timeout, bl);

	list_push_front(&rreq_blacklists, &bl->l);

	DEBUG(LOG_DEBUG, 0, "Buffering blacklist %s time= %u", ip_to_str(dest_addr), BLACKLIST_TIMEOUT);

	timer_set_timeout(&bl->bl_timer, BLACKLIST_TIMEOUT);

	return bl;
}

struct rreq_blacklist *rreq_blacklist_check(struct in_addr dest_addr)
{
	list_t *pos;
	struct rreq_blacklist *curr;
	
	list_for_each(pos, &rreq_blacklists)
	{
		curr = (struct rreq_blacklist *)pos;
		if(curr->dest_addr.s_addr == dest_addr.s_addr)
			return curr;
	}

	return NULL;

}

void rreq_route_discovery(struct in_addr dest_addr, u8_t flags)
{
	struct timeval now;
	rt_table_t *rt;
	seek_list_t *seek_entry;
	u32_t dest_seqno;
	s32_t ttl;

#define TTL_VALUE ttl

	gettimeofday(&now, NULL);

	if(seek_list_check(dest_addr))
	{
		DEBUG(LOG_DEBUG, 0, "We have sended a RREQ already!");
		return;
	}

	rt = rt_table_check(dest_addr);//We have an route entry

	ttl = NET_DIAMETER;

	if(!rt)//No route, set dest_seqno = 0,why not set the u flag?
	{
		dest_seqno = 0;
		if(expanding_ring_search)
			ttl = TTL_START;
	}
	else
	{
		dest_seqno = rt->dest_seqno;
		if(expanding_ring_search)
			ttl = rt->hopcnt + TTL_INCREMENT;
		
		if(timeval_diff(&rt->rt_timer.timeout, &now) < (2 * NET_TRAVERSAL_TIME))
			rt_table_update_timeout(rt, 2 * NET_TRAVERSAL_TIME);
	}

	rreq_send(dest_addr, dest_seqno, ttl, flags);

	seek_entry = seek_list_insert(dest_addr, dest_seqno, ttl, flags);

	if(expanding_ring_search)
		timer_set_timeout(&seek_entry->seek_timer, RING_TRAVERSAL_TIME);
	else
		timer_set_timeout(&seek_entry->seek_timer, NET_TRAVERSAL_TIME);

	DEBUG(LOG_DEBUG, 0, "Seeking %s ttl= %d", ip_to_str(dest_addr), ttl);
} 

void rreq_local_repair(rt_table_t *rt, struct in_addr src_addr)
{
	struct timeval now;
	seek_list_t *seek_entry;
	rt_table_t *rt_entry;
	s32_t ttl;
	u8_t flags = 0;

	if(!rt)
		return;
	if(seek_list_check(rt->dest_addr))
		return;
	if(!(rt->flags & RT_REPAIR))//yes or no?flags:do not permit to repair?
		return;

	gettimeofday(&now, NULL);

	DEBUG(LOG_DEBUG, 0, "Repairing route to %s", ip_to_str(rt->dest_addr));

	rt_entry = rt_table_check(src_addr);

	if(rt_entry)
		ttl = (s32_t)(max(rt->hopcnt, 0.5 * rt_entry->hopcnt) + LOCAL_ADD_TTL);
	else
		ttl = rt->hopcnt + LOCAL_ADD_TTL;

	DEBUG(LOG_DEBUG, 0, "%s, rreq_ttl= %d, dest_hcnt= %d", ip_to_str(rt->dest_addr), ttl, rt->hopcnt);

	rt->rt_timer.handler = route_expire_timeout;

	if(timeval_diff(&rt->rt_timer.timeout, &now) < (2 * NET_TRAVERSAL_TIME))
		rt_table_update_timeout(rt, 2 * NET_TRAVERSAL_TIME);

	rreq_send(rt->dest_addr, rt->dest_seqno, ttl, flags);

	seek_entry = seek_list_insert(rt->dest_addr, rt->dest_seqno, ttl, flags);

	if(expanding_ring_search)
		timer_set_timeout(&seek_entry->seek_timer, 2 * ttl * NODE_TRAVERSAL_TIME);
	else
		timer_set_timeout(&seek_entry->seek_timer, NET_TRAVERSAL_TIME);

	DEBUG(LOG_DEBUG, 0, "Seeking %s ttl= %d", ip_to_str(rt->dest_addr), ttl);
}
