#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <syslog.h>

#include "defs.h"
#include "aodv_rreq.h"
#include "aodv_rrep.h"

typedef struct 
{
	s8_t *func_name;
	void (*action)(void *);
}func_struct_t;

extern s32_t debug;

void log_init(void);
void log_cleanup(void);

s8_t *packet_type(u32_t type);
void alog(s32_t type, s32_t errnum, const s8_t *function, s8_t *format, ...);
void log_pkt_fields(AODV_msg *msg);
void print_rt_table(void *arg);
void log_rt_table_init(void);
s8_t *ip_to_str(struct in_addr addr);
void write_to_log_file(s8_t *msg, s32_t len);
s8_t *devs_ip_to_str(void);
s8_t *rreq_flags_to_str(RREQ *rreq);
s8_t *rrep_flags_to_str(RREP *rrep);
s8_t *rt_flags_to_str(u_int16_t flags);
s8_t *state_to_str(u_int8_t state);
s8_t *ptr_to_func_name(void (*action)(void *));

#define DEBUG(l, s, args...) alog(l, s, __FUNCTION__, ##args)

#endif
