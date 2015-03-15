#include "aodv_rreq.h"
#include "aodv_socket.h"
#include "routing_table.h"
#include <netinet/in.h>
#include "parameters.h"

RREQ *rreq_create(u8_t flags, struct in_addr dest_addr, u32_t dest_seqno, struct in_addr orig_addr)
{
	RREQ *rreq;

	rreq = (RREQ *)aodv_socket_new_msg();
	rreq->type = AODV_RREQ;	
	rreq->res1 = 0;
	rreq->res2 = 0;
	rreq->hopcnt = 0;
	rreq->rreq_id = htonl(++this_host.rreq_id);//this_host.rreq_id++ or ++this_host.rreq_id? same?
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

void rreq_send(struct in_addr dest_addr, u32_t dest_seqno, u8_t ttl, u8_t flags)
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
		printf("The device is not activited!\n");	
}

void rreq_process(RREQ *rreq, u32_t len, struct in_addr ip_src, struct in_addr ip_dest, int ip_ttl)
{
	struct in_addr rreq_dest, rreq_orig;
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
		printf("The RREQ was sended by us!\n");
		return;
	}

	printf("ip_src= %s, rreq_orig= %s, rreq_dest= %s, ttl= %d\n", inet_ntoa(ip_src), inet_ntoa(rreq_orig), inet_ntoa(rreq_dest), ip_ttl);

	if(len < (u32_t)RREQ_SIZE)
	{
		printf("IP data too short, from %s to %s\n", inet_ntoa(ip_src), inet_ntoa(ip_dest));
		return;
	}

/*	if(rreq_check_blacklist(ip_src))
	{
		printf("prev hop of RREQ is in the blacklist, ignoring!\n");
		return;
	}*/

	/*if(rreq_check_record(rreq_orig, rreq_id))
	{
		printf("We have a visiter which we just sended him out!\n ");
		return;
	}*/

/*	rreq_record_insert(rreq_orig, rreq_id);*/
	
	
	/*
	 *
	 * we can check if there is any extension sended with the RREQ package, but I think I don`t want to add some new extensionnow, so write the check code when I use it.
	 *
	 *
	 */

	life = PATH_DISCOVERY_TIME - 2 * rreq_new_hopcnt * NODE_TRAVERSAL_TIME;

	rev_rt = rt_table_find(rreq_orig);

	if(rev_rt == NULL)
	{
		printf("Creating REVERSE route entry, RREQ orig: %s", inet_ntoa(rreq_orig));
		rev_rt = rt_table_insert(rreq_orig, ip_src, rreq_new_hopcnt, rreq_orig_seqno, life, VALID, 0);
	}	
	else
	{
		if(rev_rt->dest_seqno == 0 || (s32_t)rreq_orig_seqno > (s32_t)rev_rt->dest_seqno || (rreq_orig_seqno == rev_rt->dest_seqno && (rev_rt->state == INVALID || rreq_new_hopcnt < rev_rt->hopcnt)))
		{
			rev_rt = rt_table_update(rev_rt, ip_src, rreq_new_hopcnt, rreq_orig_seqno, life, VALID, rev_rt->flags);
		}
	}

/////////////////////////////////////////////////////	
}
