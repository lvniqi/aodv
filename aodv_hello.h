#ifndef _AODV_HELLO_H_
#define _AODV_HELLO_H_

#include "defs.h"
#include "aodv_rrep.h"

#define ROUTE_TIMEOUT_SLACK 100
#define HELLO_DELAY         50

void hello_start(void);
void hello_stop(void);
void hello_send(void *arg);
void hello_process(RREP *hello, s32_t len);
void hello_update_timeout(rt_table_t *rt, struct timeval *now, long time);

#endif
