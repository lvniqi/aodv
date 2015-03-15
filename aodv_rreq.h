#ifndef _AODV_RREQ_H_
#define _AODV_RREQ_H_

#include "defs.h"

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
	u8_t hop_cnt;

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

#endif