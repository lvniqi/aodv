#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <getopt.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include "defs.h"
#include "timer_queue.h"
#include "parameters.h"
#include "aodv_socket.h"
#include "aodv_timeout.h"
#include "routing_table.h"
#include "aodv_hello.h"
#include "nl.h"
#include "debug.h"

s32_t wait_on_reboot = 1;
s32_t ratelimit = 1;
s32_t expanding_ring_search = 1;
s32_t receive_n_hellos = 0;
s32_t local_repair = 0;
//s32_t qual_threshold = 0;
s32_t log_to_file = 1;
s32_t rt_log_interval = 10000;

s8_t *progname;

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
		DEBUG(LOG_WARNING, 0, "callback attach limited reached");
		return -1;
	}
	
	callbacks[nr_callbacks].fd = fd;
	callbacks[nr_callbacks].func = func;

	nr_callbacks++;

	return 0;
}

int set_kernel_options(void)
{
	s32_t fd = -1;
	s8_t on = '1', off = '0';
	s8_t path[64];

	if((fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY)) < 0)
		return -1;
	if(write(fd, &on, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	//only one interface which we used, no loop
	if(!this_host.dev.enabled)
		return -1;

///////////////////////////////////////////////////////////////////////////////////////
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/%s/send_redirects", this_host.dev.ifname);

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &off, sizeof(s8_t)) < 0)
		return -1;
	close(fd);
	
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/%s/accept_redirects", this_host.dev.ifname);

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &off, sizeof(s8_t)) < 0)
		return -1;
	close(fd);
///////////////////////////////////////////////////////////////////////////////////////
	
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/all/send_redirects");

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &off, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/all/accept_redirects");

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &off, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	return 0;
}

int reset_kernel_options(void)
{
	s32_t fd = -1;
	s8_t on = '1', off = '0';
	s8_t path[64];

	if((fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY)) < 0)
		return -1;
	if(write(fd, &on, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	//only one interface which we used, no loop
	if(!this_host.dev.enabled)
		return -1;

///////////////////////////////////////////////////////////////////////////////////////
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/%s/send_redirects", this_host.dev.ifname);

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &on, sizeof(s8_t)) < 0)
		return -1;
	close(fd);
	
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/%s/accept_redirects", this_host.dev.ifname);

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &on, sizeof(s8_t)) < 0)
		return -1;
	close(fd);
///////////////////////////////////////////////////////////////////////////////////////
	
	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/all/send_redirects");

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &on, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	memset(path, '\0', 64);
	sprintf(path, "/proc/sys/net/ipv4/conf/all/accept_redirects");

	if((fd = open(path, O_WRONLY)) < 0)
		return -1;
	if(write(fd, &off, sizeof(s8_t)) < 0)
		return -1;
	close(fd);

	return 0;
}

struct sockaddr_in *get_if_info(s8_t *ifname, s32_t type)
{
	s32_t skfd;
	struct sockaddr_in *ina;
	static struct ifreq ifr;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, ifname);

	if(ioctl(skfd, type, &ifr) < 0)
	{
		close(skfd);
		return NULL;
	}
	else
	{
		ina = (struct sockaddr_in *)&ifr.ifr_addr;
		close(skfd);
		return ina;
	}
}

void load_modules(s8_t *ifname)
{
	struct stat st;
	s8_t buf[64];

	memset(buf, '\0', 64);

	if(stat("./kaodv.ko", &st) == 0)
		sprintf(buf, "/usr/sbin/insmod kaodv.ko");

	if(system(buf) == -1)
	{
		DEBUG(LOG_WARNING, 0, "Could not load kaodv module");
		exit(-1);
	}
	else
	{
		DEBUG(LOG_DEBUG, 0, "Module loaded! Ifname= %s", ifname);
	}
}

void remove_modules(void)
{
	if(system("/usr/sbin/rmmod kaodv.ko") == -1)
		DEBUG(LOG_WARNING, 0, "Module unload failed");
	else
		DEBUG(LOG_DEBUG, 0, "Moudle unloaded");
}

void host_init(s8_t *ifname)
{
	struct sockaddr_in *ina;
	s8_t buf[1024], tmp_ifname[16];
	struct ifconf ifc;
	struct ifreq ifr, *ifrp;
	s32_t i, iw_sock, if_sock = 0;
	
	memset(&this_host, 0, sizeof(struct host_info));

	if(!ifname)
	{
		iw_sock = socket(PF_INET, SOCK_DGRAM, 0);
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = buf;
		if(ioctl(iw_sock, SIOCGIFCONF, &ifc) < 0)
		{
			DEBUG(LOG_WARNING, 0, "Could not get wireless info");
			exit(-1);
		}

		ifrp = ifc.ifc_req;
		for(i = ifc.ifc_len / sizeof(struct ifreq); i>=0; i--, ifrp++)
		{
			struct iwreq req;

			strcpy(req.ifr_name, ifrp->ifr_name);
			if(ioctl(iw_sock, SIOCGIWNAME, &req) >= 0)
			{
				strcpy(tmp_ifname, ifrp->ifr_name);
				break;
			}
		}

		if(!strlen(tmp_ifname))
		{
			DEBUG(LOG_WARNING, 0, "Could find a wireless interface");
			exit(-1);
		}
		else
		{
			DEBUG(LOG_DEBUG, 0, "Found wireless interface %s", tmp_ifname);
		}

		strcpy(ifr.ifr_name, tmp_ifname);
		if(ioctl(iw_sock, SIOCGIFINDEX, &ifr) < 0)
		{
			DEBUG(LOG_WARNING, 0, "Could not get index of %s", tmp_ifname);
			close(if_sock);
			exit(-1);
		}
		close(iw_sock);

		ifname = tmp_ifname;

		DEBUG(LOG_DEBUG, 0, "Attaching to %s", ifname);
	}

	this_host.seqno = 0;
	this_host.rreq_id = 0;

	gettimeofday(&this_host.last_broadcast_time, NULL);

	if_sock = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, ifname);
	if(ioctl(if_sock, SIOCGIFINDEX, &ifr) < 0)
	{
		DEBUG(LOG_WARNING, 0, "Could not get index of %s", ifname);
		close(if_sock);
		exit(-1);
	}
	this_host.dev.ifindex = ifr.ifr_ifindex;

	strcpy(this_host.dev.ifname, ifname);

	ina = get_if_info(ifname, SIOCGIFADDR);
	if(ina == NULL)
		exit(-1);
	this_host.dev.ipaddr.s_addr = ina->sin_addr.s_addr;

	ina = get_if_info(ifname, SIOCGIFNETMASK);
	if(ina == NULL)
		exit(-1);
	this_host.dev.netmask.s_addr = ina->sin_addr.s_addr;	

	ina = get_if_info(ifname, SIOCGIFBRDADDR);
	if(ina == NULL)
		exit(-1);
	this_host.dev.broadcast.s_addr = ina->sin_addr.s_addr;

	this_host.dev.enabled = 1;

//////////////////////////////////////////////////////////////
	DEBUG(LOG_DEBUG, 0, "Interface: %s, ifindex: %d, ipaddr: %s, netmask: %s, broadcast: %s, state: %d", this_host.dev.ifname, this_host.dev.ifindex, ip_to_str(this_host.dev.ipaddr), ip_to_str(this_host.dev.netmask), ip_to_str(this_host.dev.broadcast), this_host.dev.enabled);
///////////////////////for test///////////////////////////////

	load_modules(this_host.dev.ifname);

	if(set_kernel_options() < 0)
	{
		DEBUG(LOG_WARNING, 0, "Kernel setting failed");
		exit(-1);
	}
}

static void cleanup(void)
{
	rt_table_destroy();
	aodv_socket_cleanup();
	nl_cleanup();
	remove_modules();
	reset_kernel_options();
	log_cleanup();

	printf("All clean up!\n");
}

void signal_handler(s32_t sign_no)
{
	switch(sign_no)
	{
		case SIGINT: 
			cleanup();
		default:
			exit(0);
	}
}

int main(int argc, char *argv[])
{
	fd_set rfds, readers;
	s32_t nfds = 0, i;
	struct timeval *timeout;
	s32_t retval;

	if(geteuid() != 0)
	{
		printf("Must be root!\n");
		exit(-1);
	}

	progname = strrchr(argv[0], '/');
	if(progname)
		progname++;
	else
		progname = argv[0];

	signal(SIGINT, signal_handler);
//	if(fork() != 0)//Detach from terminal
//		exit(0);
//	close(1);
//	close(2);
//	setsid();

	log_init();
	log_rt_table_init();

	rt_table_init();

	host_init("br-lan");

	nl_init();
	nl_send_conf_msg();/////////////
	aodv_socket_init();

	//No wait on reboot now
	
	//hello_start();

	FD_ZERO(&readers);

	for(i = 0; i < nr_callbacks; i++)
	{
		FD_SET(callbacks[i].fd, &readers);
		if(callbacks[i].fd >= nfds)
			nfds = callbacks[i].fd + 1;
	}
	
	printf("All init succeed!\n");

	while(1)
	{
		memcpy((s8_t *)&rfds, (s8_t *)&readers, sizeof(rfds));
		
		timeout = timer_age_queue();
/**************************for debug*******************************/
		if(timeout != NULL)
			DEBUG(LOG_DEBUG, 0, "set sec: %ld, set usec %ld", timeout->tv_sec, timeout->tv_usec);
		else
			DEBUG(LOG_DEBUG, 0, "set NULL");
/******************************************************************/
		retval = select(nfds, &rfds, NULL, NULL, timeout);
		if(retval < 0)
			DEBUG(LOG_WARNING, 0, "Select failed");

		for(i = 0; i < nr_callbacks; i++)
		{
			if(FD_ISSET(callbacks[i].fd, &rfds))
				(*callbacks[i].func)(callbacks[i].fd);
		}
	}

	return 0;
}
