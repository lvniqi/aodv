#ifndef _AODV_TIMEOUT_H_
#define _AODV_TIMEOUT_H_

#include "defs.h"

void rreq_record_timeout(void *arg);
void rreq_blacklist_timeout(void *arg);
void route_discovery_timeout(void *arg);
void local_repair_timeout(void *arg);
void route_expire_timeout(void *arg);
void route_delete_timeout(void *arg);
void hello_timeout(void *arg);
void rrep_ack_timeout(void *arg);
void wait_on_reboot_timeout(void *arg);

#endif

