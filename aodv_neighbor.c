#include "aodv_neighbor.h"
#include "aodv_rerr.h"
#include "aodv_hello.h"
#include "aodv_socket.h"
#include "routing_table.h"
#include "parameters.h"
#include "list.h"
#include "debug.h"

//extern s32_t llfeedback;

void neighbor_add(AODV_msg *aodv_msg, struct in_addr src)
{
	struct timeval now;
	rt_table_t *rt = NULL;
	u32_t seqno = 0;

	gettimeofday(&now, NULL);

	rt = rt_table_check(src);

	if(!rt)
	{
		DEBUG(LOG_DEBUG, 0, "%s new NEIGHBOR", ip_to_str(src));
		rt = rt_table_insert(src, src, 1, 0, ACTIVE_ROUTE_TIMEOUT, VALID, 0);
	}
	else
	{
		if(rt->flags & RT_UNIDIR)
			return;
		if(rt->dest_seqno != 0)
			seqno = rt->dest_seqno;

		rt_table_update(rt, src, 1, seqno, ACTIVE_ROUTE_TIMEOUT, VALID, rt->flags);
	}

	if(rt->hello_timer.used)
		hello_update_timeout(rt, &now, ALLOWED_HELLO_LOSS * HELLO_INTERVAL);
}

void neighbor_link_break(rt_table_t *rt)
{
	RERR *rerr = NULL;
	rt_table_t *rt_u;
	struct in_addr rerr_unicast_dest;
	s32_t i;

	rerr_unicast_dest.s_addr = 0;

	if(!rt)
		return;

	if(rt->hopcnt != 1)
	{
		DEBUG(LOG_DEBUG, 0, "%s is not a neighbor, hcnt=%d", ip_to_str(rt->dest_addr), rt->hopcnt);
		return;
	}

	DEBUG(LOG_DEBUG, 0, "Link %s down", ip_to_str(rt->dest_addr));

	rt_table_invalidate(rt);
	
	if(rt->nprecursor && !(rt->flags & RT_REPAIR))
	{
		rerr = rerr_create(0, rt->dest_addr, rt->dest_seqno);
		DEBUG(LOG_DEBUG, 0, "Added %s as a unreachable, seqno= %d", ip_to_str(rt->dest_addr), rt->dest_seqno);

		if(rt->nprecursor == 1)
			rerr_unicast_dest = FIRST_PREC(rt->precursors)->neighbor;
	}
	if(!(rt->flags &RT_REPAIR))
		precursor_list_destroy(rt);

	list_t *pos;
	
	for(i = 0; i < RT_TABLESIZE; i++)
	{
		list_for_each(pos, &rt_tbl.tbl[i])
		{
			rt_u = (rt_table_t *)pos;

			if(rt_u->state == VALID && rt_u->next_hop.s_addr == rt->dest_addr.s_addr && rt_u->dest_addr.s_addr != rt->dest_addr.s_addr)
			{
				if((rt->flags & RT_REPAIR) && rt_u->hopcnt <= MAX_REPAIR_TTL)
				{
					rt_u->flags |= RT_REPAIR;
					DEBUG(LOG_DEBUG, 0, "Marking %s for REPAIR", ip_to_str(rt->dest_addr));

					rt_table_invalidate(rt_u);
					continue;
				}

				rt_table_invalidate(rt_u);

				if(rt_u->nprecursor)
				{
					if(!rerr)
					{
						rerr = rerr_create(0, rt_u->dest_addr, rt_u->dest_seqno);

						if(rt_u->nprecursor == 1)
							rerr_unicast_dest = FIRST_PREC(rt_u->precursors)->neighbor;

						DEBUG(LOG_DEBUG, 0, "Added %s as unreachable, seqno= %d", ip_to_str(rt_u->dest_addr), rt_u->dest_seqno);
					}
					else
					{
						rerr_add_udest(rerr, rt_u->dest_addr, rt_u->dest_seqno);

						if(rerr_unicast_dest.s_addr)
						{
							list_t *pos;
							precursor_t *ptr;
							list_for_each(pos, &rt_u->precursors)
							{
								ptr = (precursor_t *)pos;
								if(ptr->neighbor.s_addr != rerr_unicast_dest.s_addr)
								{
									rerr_unicast_dest.s_addr = 0;
									break;
								}
							}
						}
						
						DEBUG(LOG_DEBUG, 0, "Added %s as unreachable, seqno= %d", ip_to_str(rt_u->dest_addr), rt_u->dest_seqno);
					}
				}
				precursor_list_destroy(rt_u);
			}
		}
	}

	if(rerr)
	{
		DEBUG(LOG_DEBUG, 0, "RERR created, %d bytes", RERR_CALC_SIZE(rerr));

		if(rerr_unicast_dest.s_addr)
		{
			rt_u = rt_table_check(rerr_unicast_dest);
			if(!rt)
				DEBUG(LOG_DEBUG, 0, "No route for RERR %s to send", ip_to_str(rerr_unicast_dest));
			else
				aodv_socket_send((AODV_msg *)rerr, rerr_unicast_dest, RERR_CALC_SIZE(rerr), 1, &this_host.dev);
		}
		else
		{
			rerr_unicast_dest.s_addr = AODV_BROADCAST;
			aodv_socket_send((AODV_msg *)rerr, rerr_unicast_dest, RERR_CALC_SIZE(rerr), 1, &this_host.dev);

		}		
	}
}
