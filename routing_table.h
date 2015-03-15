#ifndef _ROUTING_TABLE_H_
#define _ROUTING_TABLE_H_

#include "defs.h"

#define INVALID 0
#define VALID   1

typedef struct
{
	//list_t l;
	struct in_addr dest_addr;
	u32_t dest_seqno;
	struct in_addr next_hop;
	u8_t hopcnt;
	u8_t flags;
	u8_t state;
	//struct timer rt_timer;
	//struct timer ack_timer;
	//struct timer hello_timer;
	//struct timeval last_hello_time;
	u8_t hello_cnt;
	//hash_value hash;
	u32_t nprenode;
	//list_t prenode;
}rt_table_t;

rt_table_t *rt_table_find(struct in_addr rreq_orig);
rt_table_t *rt_table_insert(struct in_addr rreq_orig, struct in_addr ip_src, u32_t rreq_new_hopcnt, u32_t rreq_orig_seqno, u32_t life, u8_t state, u8_t flags);
rt_table_t *rt_table_update(rt_table_t *rev_rt, struct in_addr ip_src, u32_t rreq_new_hopcnt, u32_t rreq_orig_seqno, u32_t life, u8_t state, u8_t flags);

#endif
