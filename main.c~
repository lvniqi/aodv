#include "defs.h"
#include "aodv_socket.h"
#include "parameters.h"
#include "aodv_rreq.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

s32_t wait_on_reboot = 1;
s32_t ratelimit = 1;
s32_t expanding_ring_search = 1;
s32_t receive_n_hellos = 0;
s32_t local_repair = 0;
s32_t qual_threshold = 0;

s32_t active_route_timeout = ACTIVE_ROUTE_TIMEOUT_HELLO;
s32_t ttl_start = TTL_START_HELLO;
s32_t delete_period = DELETE_PERIOD_HELLO;

struct timer worb_timer;

#define CALLBACK_FUNCS 3

static s32_t nr_callbacks = 0;

static struct callback
{
	s32_t fd;
	callback_func_t func;
}callbacks[CALLBACK_FUNCS];

s32_t attach_callback_func(s32_t fd, callback_func_t func)
{
	if(nr_callbacks >= CALLBACK_FUNCS)
	{
		printf("callback attach limited reached!\n");
		return -1;
	}
	
	callbacks[nr_callbacks].fd = fd;
	callbacks[nr_callbacks].func = func;

	nr_callbacks++;

	return 0;
}

int main(int argc, char *argv[])
{
	fd_set listener;
	struct timeval tv;
	s32_t retval;
	struct in_addr dest;

	this_host.seqno = 10;
	this_host.rreq_id = 10;
	this_host.last_broadcast_time.tv_sec = 0;
	this_host.last_broadcast_time.tv_usec = 0;
	this_host.dev.enabled = 1;

#ifdef MT
	strcpy(this_host.dev.ifname, "br-lan");
	this_host.dev.ipaddr.s_addr = inet_addr("192.168.1.1");
#else
	strcpy(this_host.dev.ifname, "wlan0");
	this_host.dev.ipaddr.s_addr = inet_addr("192.168.1.100");
#endif

	aodv_socket_init();
	rt_table_init();

	printf("Init succeed!\n");

	if(argc > 1)
	{
		dest.s_addr = inet_addr(argv[1]);
		rreq_send(dest, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	}

	while(1)
	{
		FD_ZERO(&listener);
		FD_SET(this_host.dev.sock, &listener);

		tv.tv_sec = tv.tv_usec = 0;	
		retval = select(this_host.dev.sock + 1, &listener, NULL, NULL, &tv);
		if(retval < 0)
			printf("Select failed!\n");
		if(FD_ISSET(this_host.dev.sock, &listener))
			aodv_socket_read(this_host.dev.sock);
	}
	return 0;
}
