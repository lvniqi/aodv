#include "routing_table.h"

rt_table_t *rt_table_find(struct in_addr rreq_orig)
{
	rt_table_t rt;

	return &rt;
}

rt_table_t *rt_table_insert(struct in_addr rreq_orig, struct in_addr ip_src, u32_t rreq_new_hopcnt, u32_t rreq_orig_seqno, u32_t life, u8_t state, u8_t flags)
{
	rt_table_t rt;

	return &rt;
}

rt_table_t *rt_table_update(rt_table_t *rev_rt, struct in_addr ip_src, u32_t rreq_new_hopcnt, u32_t rreq_orig_seqno, u32_t life, u8_t state, u8_t flags)
{
	rt_table_t rt;

	return &rt;
}

