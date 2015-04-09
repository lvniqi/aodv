#ifndef _DEFINE_H_
#define _DEFINE_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <errno.h>

typedef unsigned char u8_t;
typedef signed   char s8_t;
typedef unsigned int  u32_t;
typedef signed   int  s32_t;

#define AODV_HELLO    0
#define AODV_RREQ     1
#define AODV_RREP     2
#define AODV_RERR     3
#define AODV_RREP_ACK 4

#define AODV_BROADCAST ((in_addr_t)(0xffffffff))

#define AODV_PORT 654

#define RREQ_RATELIMIT 10
#define RERR_RATELIMIT 10

#define IFNAMESIZE 16

#define max(x, y) ((x) > (y) ? (x):(y))

#define NIPQUAD(addr) \
	    ((unsigned char *)&addr)[0], \
		((unsigned char *)&addr)[1], \
		((unsigned char *)&addr)[2], \
		((unsigned char *)&addr)[3]

struct dev_info 
{
	u8_t enabled;
	s32_t sock;           //socket associated with this device

	unsigned int ifindex;
	s8_t ifname[IFNAMESIZE];
	struct in_addr ipaddr;
	struct in_addr netmask;
	struct in_addr broadcast;
};

struct host_info// no need for other variables in example
{
	u32_t seqno;
	u32_t rreq_id;
	struct timeval last_forward_time;
	struct timeval last_broadcast_time;
	struct dev_info dev;
};

struct host_info this_host;

typedef struct 
{
	u8_t type;
}AODV_msg;

typedef void (*callback_func_t)(s32_t);
extern s32_t attach_callback_func(s32_t fd, callback_func_t func);

#endif
