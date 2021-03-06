#ifndef _SEEK_LIST_H_
#define _SEEK_LIST_H_

#include "defs.h"
#include "list.h"
#include "timer_queue.h"

typedef struct seek_list
{
	list_t l;
	struct in_addr dest_addr;
	u32_t dest_seqno;
	u8_t flags;
	s32_t rreq_cnt;
	s32_t ttl;
	struct timer seek_timer;
}seek_list_t;

seek_list_t *seek_list_insert(struct in_addr dest_addr, u32_t dest_seqno, s32_t ttl, u8_t flags);
s32_t seek_list_remove(seek_list_t *entry);
seek_list_t *seek_list_check(struct in_addr dest_addr);

#endif
