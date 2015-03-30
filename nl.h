#ifndef _NL_H_
#define _NL_H_

#include "defs.h"

void nl_init(void);
void nl_cleanup(void);
int nl_send_add_route_msg(struct in_addr dest, struct in_addr next_hop, s32_t metric, u32_t lifetime, s32_t rt_flags, s32_t ifindex);
int nl_send_del_route_msg(struct in_addr dest, struct in_addr next_hop, s32_t metric);
int nl_send_no_route_found_msg(struct in_addr dest);
int nl_send_conf_msg(void);

#endif
