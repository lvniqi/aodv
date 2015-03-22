#ifndef _AODV_RERR_H_
#define _AODV_RERR_H_

#include "defs.h"

#define RERR_NODELETE (1 << 7)

typedef struct 
{
	u8_t type;
	u8_t n:1;
	u8_t res1:7;
	u8_t res2:8;
	u8_t dest_count;
	u32_t dest_addr;
	u32_t dest_seqno;
}RERR;

#define RERR_SIZE sizeof(RERR)

typedef struct 
{
	u32_t dest_addr;
	u32_t dest_seqno;
}RERR_udest;

#define RERR_UDEST_SIZE sizeof(RERR_udest)

#define RERR_CALC_SIZE(rerr)   (RERR_SIZE + (rerr->dest_count - 1) * RERR_UDEST_SIZE)
#define RERR_UDEST_FIRST(rerr) (RERR_udest *)(&rerr->dest_addr)
#define RERR_UDEST_NEXT(udest) (RERR_udest *)((s8_t *)udest + RERR_UDEST_SIZE)

RERR *rerr_create(u8_t flags, struct in_addr dest_addr, u32_t dest_seqno);
void rerr_add_udest(RERR *rerr, struct in_addr udest, u32_t udest_seqno);
void rerr_process(RERR *rerr, s32_t len, struct in_addr ip_src, struct in_addr ip_dest);

#endif
