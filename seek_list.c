#include "seek_list.h"
#include "list.h"
#include "timer_queue.h"
#include "aodv_timeout.h"
#include "debug.h"

list_t sl = {&sl, &sl};

seek_list_t *seek_list_insert(struct in_addr dest_addr, u32_t dest_seqno, s32_t ttl, u8_t flags)
{
	seek_list_t *entry;

	if((entry = (seek_list_t *)malloc(sizeof(seek_list_t))) == NULL)
	{
		DEBUG(LOG_WARNING, 0, "seek_list malloc failed");
		exit(-1);
	}

	entry->dest_addr = dest_addr;
	entry->dest_seqno = dest_seqno;
	entry->ttl = ttl;
	entry->flags = flags;
	entry->rreq_cnt = 1;//add myself

	timer_init(&entry->seek_timer, route_discovery_timeout, entry);
	list_add(&sl, &entry->l);

	return entry;
}

s32_t seek_list_remove(seek_list_t *entry)
{
	if(!entry)
		return -1;
	
	list_remove(&entry->l);

	timer_remove(&entry->seek_timer);

	free(entry);

	return 0;
}

seek_list_t *seek_list_check(struct in_addr dest_addr)
{
	list_t *pos;
	seek_list_t *entry;

	list_for_each(pos, &sl)
	{
		entry = (seek_list_t *)pos;

		if(entry->dest_addr.s_addr == dest_addr.s_addr)
			return entry;	
	}

	return NULL;
}
