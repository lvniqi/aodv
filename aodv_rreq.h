#ifndef _AODV_RREQ_H_
#define _AODV_RREQ_H_

#include "defs.h"
#include "list.h"
#include "timer_queue.h"
#include "aodv_timeout.h"

typedef struct
{
	u8_t type;
	u8_t j:1;
	u8_t r:1;
	u8_t g:1;
	u8_t d:1;
	u8_t u:1;
	u8_t res1:3;
	u8_t res2;
	u8_t hopcnt;

	u32_t rreq_id;
	u32_t dest_addr;
	u32_t dest_seqno;
	u32_t orig_addr;
	u32_t orig_seqno;
}RREQ;
 
#define RREQ_SIZE sizeof(RREQ)

#define RREQ_JOIN          (1 << 7)
#define RREQ_REPAIR        (1 << 6)
#define RREQ_GRATUITOUS    (1 << 5)
#define RREQ_DEST_ONLY     (1 << 4)
#define RREQ_UNKNOWN_SEQNO (1 << 3)

#define seqno_incr(s) ((s == 0) ? 0 : ((s == 0xffffffff) ? s = 1 : s++))

struct rreq_record
{
	list_t l;
	struct in_addr orig_addr;
	u32_t rreq_id;
	struct timer rec_timer;
};

struct rreq_blacklist
{
	list_t l;
	struct in_addr dest_addr;
	struct timer bl_timer;
};

void rreq_process(RREQ *rreq, s32_t len, struct in_addr ip_src, struct in_addr ip_dest, s32_t ip_ttl);
void rreq_send(struct in_addr dest_addr, u32_t dest_seqno, s32_t ttl, u8_t flags);
//void rreq_route_discovery(struct in_addr dest_addr, u8_t flags, struct ip_data *ipd);
void rreq_forward(RREQ *rreq, s32_t len, s32_t ttl);
struct rreq_record *rreq_record_insert(struct in_addr orig_addr, u32_t rreq_id);
struct rreq_record *rreq_check_record(struct in_addr orig_addr, u32_t rreq_id);
struct rreq_blacklist *rreq_blacklist_insert(struct in_addr dest_addr);
struct rreq_blacklist *rreq_check_blacklist(struct in_addr dest_addr);
//void rreq_local_repair(rt_table_t *rt, struct in_addr src_addr, struct ip_data *ipd);

#endif
