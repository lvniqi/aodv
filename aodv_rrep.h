#ifndef _AODV_RREP_H_
#define _AODV_RREP_H_

#include "defs.h"
#include "routing_table.h"
#include "aodv_rrep.h"

typedef struct
{
	u8_t type;
	u8_t r:1;
	u8_t a:1;
	u8_t res1:6;
	u8_t res2:3;
	u8_t prefix:5;
	u8_t hopcnt;
	u32_t dest_addr;
	u32_t dest_seqno;
	u32_t orig_addr;
	u32_t lifetime;
}RREP;

typedef struct 
{
	u8_t type;
	u8_t res;	
}RREP_ack;

#define RREP_SIZE sizeof(RREP)

#define RREP_REPAIR (1 << 7)
#define RREP_ACK (1 << 6)

RREP *rrep_create(u8_t flags, u8_t prefix, u8_t hopcnt, struct in_addr dest_addr, u32_t dest_seqno, struct in_addr orig_addr, u32_t life);
RREP_ack *rrep_ack_create(void);
void rrep_ack_process(RREP_ack *rrep_ack, s32_t len, struct in_addr ip_src, struct in_addr ip_dest);
void rrep_send(RREP *rrep, rt_table_t *rev_rt, rt_table_t *fwd_rt,s32_t len);
void rrep_forward(RREP *rrep, s32_t len, rt_table_t *rev_rt, rt_table_t *fwd_rt, s32_t ttl);
void rrep_process(RREP *rrep, s32_t len, struct in_addr ip_src, struct in_addr ip_dest, s32_t ip_ttl);

#endif
