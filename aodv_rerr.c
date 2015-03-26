#include "aodv_rerr.h"
#include "aodv_socket.h"
#include "routing_table.h"
#include "list.h"

RERR *rerr_create(u8_t flags, struct in_addr dest_addr, u32_t dest_seqno)
{
	RERR *rerr;

	printf("Create RERR about %s, seqno= %d\n", inet_ntoa(dest_addr), dest_seqno);

	rerr = (RERR *)aodv_socket_new_msg();
	rerr->type = AODV_RERR;
	rerr->n = (flags & RERR_NODELETE ? 1:0);
	rerr->res1 = 0;
	rerr->res2 = 0;
	rerr->dest_addr = dest_addr.s_addr;
	rerr->dest_seqno = htonl(dest_seqno);
	rerr->dest_count = 1;

	return rerr;
}

void rerr_add_udest(RERR *rerr, struct in_addr udest, u32_t udest_seqno)
{
	RERR_udest *ud;

	ud = (RERR_udest *)((s8_t *)rerr + RERR_CALC_SIZE(rerr));
	ud->dest_addr = udest.s_addr;
	ud->dest_seqno = htonl(udest_seqno);

	rerr->dest_count++;
}

void rerr_process(RERR *rerr, s32_t len, struct in_addr ip_src, struct in_addr ip_dest)
{
	RERR* new_rerr = NULL;
	RERR_udest *udest;
	rt_table_t *rt;
	u32_t rerr_dest_seqno;
	struct in_addr udest_addr, rerr_unicast_dest;

	rerr_unicast_dest.s_addr = 0;

	printf("rerr_process ip_src= %s\n", inet_ntoa(ip_src));

	if(len < ((s32_t)RERR_CALC_SIZE(rerr)))
	{
		printf("IP data too short (%u bytes) from %s to %s. Should be %d bytes\n", len, inet_ntoa(ip_src), inet_ntoa(ip_dest), RERR_CALC_SIZE(rerr));
		return;
	}

	udest = RERR_UDEST_FIRST(rerr);

	while(rerr->dest_count)
	{
		udest_addr.s_addr = udest->dest_addr;
		rerr_dest_seqno = ntohl(udest->dest_seqno);
		printf("unreachable dest= %s seqno= %d\n", inet_ntoa(udest_addr), rerr_dest_seqno);

		rt = rt_table_check(udest_addr);

		if(rt && rt->state == VALID && rt->next_hop.s_addr == ip_src.s_addr)
		{
			//Need to test the seqno, if the seqno is not larger than the seqno in route_table, the route in route_table is a new way to the destination, the rerr break cannot affect the route in route_table
			if((s32_t)rt->dest_seqno > (s32_t)rerr_dest_seqno)
			{
				printf("Udest ignored because of the seqno\n");
				udest = RERR_UDEST_NEXT(udest);
				rerr->dest_count--;
				continue;
			}

			if(!rerr->n)
			{
				rt_table_invalidate(rt);
				printf("Invalidating rte %s - IN ERR!\n", inet_ntoa(udest_addr));
			}
			
			rt->dest_seqno = rerr_dest_seqno;//Update the seqno

			if(rt->nprecursor && !(rt->flags & RT_REPAIR))
			{
				if(!new_rerr)
				{
					u8_t flags = 0;

					if(rerr->n)
						flags |= RERR_NODELETE;

					new_rerr = rerr_create(flags, rt->dest_addr, rt->dest_seqno);

					printf("Added %s as unreachable, seqno= %d\n", inet_ntoa(rt->dest_addr), rt->dest_seqno);
					if(rt->nprecursor == 1)//if the dest_addr is all the same(single one), use unicast, otherwise all use boardcast
						rerr_unicast_dest = FIRST_PREC(rt->precursors)->neighbor;

				}
				else
				{
					rerr_add_udest(new_rerr, rt->dest_addr, rt->dest_seqno);

					printf("Added %s as unreachable, seqno= %d\n", inet_ntoa(rt->dest_addr), rt->dest_seqno);
					if(rerr_unicast_dest.s_addr)
					{
						list_t *pos;
						precursor_t *ptr;
						list_for_each(pos, &rt->precursors)
						{
							ptr = (precursor_t *)pos;
							if(ptr->neighbor.s_addr != rerr_unicast_dest.s_addr)
							{
								rerr_unicast_dest.s_addr = 0;
								break;
							}
						}
					}
				}		
			}
			else
			{
				printf("Not sending RERR, no precursors or route is in repairing!\n");
			}

			if(rt->state == INVALID)
				precursor_list_destroy(rt);
		}
		else
		{
			printf("Ignoring UDEST %s\n", inet_ntoa(udest_addr));
		}

		udest = RERR_UDEST_NEXT(udest);
		rerr->dest_count--;
	}//end of while

	if(new_rerr)
	{
		if(rerr_unicast_dest.s_addr)
		{	
			rt = rt_table_check(rerr_unicast_dest);
			if(!rt)
				printf("No route for RERR %s to send!\n", inet_ntoa(rerr_unicast_dest));
			else
				aodv_socket_send((AODV_msg *)new_rerr, rerr_unicast_dest, RERR_CALC_SIZE(new_rerr), 1, &this_host.dev);
		}
		else
		{
			rerr_unicast_dest.s_addr = AODV_BROADCAST;
			aodv_socket_send((AODV_msg *)new_rerr, rerr_unicast_dest, RERR_CALC_SIZE(new_rerr), 1, &this_host.dev);
		}

	}
}
