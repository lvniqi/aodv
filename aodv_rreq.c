#include "aodv_rreq.h"
#include "aodv_socket.h"
#include <netinet/in.h>

RREQ *rreq_create(u8_t flags, struct in_addr dest_addr, u32_t dest_seqno, struct in_addr orig_addr)
{
	RREQ *rreq;

	rreq = (RREQ *)aodv_socket_new_msg();
	rreq->type = AODV_RREQ;	
	rreq->res1 = 0;
	rreq->res2 = 0;
	rreq->hop_cnt = 0;
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
