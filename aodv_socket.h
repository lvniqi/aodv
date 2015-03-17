#ifndef _AODV_SOCKET_H_
#define _AODV_SOCKET_H_

#include "defs.h"

#define RECE_BUF_SIZE 1024
#define SEND_BUF_SIZE RECE_BUF_SIZE

void aodv_socket_init(void);
s8_t *aodv_socket_new_msg(void);
void aodv_socket_send(AODV_msg *aodv_msg, struct in_addr dest, s32_t len, s32_t ttl, struct dev_info *dev);
void aodv_socket_read(s32_t fd);
s8_t *aodv_socket_queue_msg(AODV_msg *aodv_msg, s32_t len);

#endif
