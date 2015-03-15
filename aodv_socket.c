#include "aodv_socket.h"
#include "aodv_rreq.h"
#include <memory.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/udp.h>
#include <sys/uio.h>

#define SOCK_RECVBUF_SIZE 256*1024

extern wait_on_reboot;
extern ratelimit;

u8_t rece_buf[RECE_BUF_SIZE];
u8_t send_buf[SEND_BUF_SIZE];

u32_t num_rreq;
u32_t num_rerr;

struct timeval rreq_ratelimit[RREQ_RATELIMIT];//limit the package num send every second
struct timeval rerr_ratelimit[RERR_RATELIMIT];

static inline long timeval_diff(struct timeval *t1, struct timeval *t2)
{
    long long res;		// avoid overflows while calculating

    if (!t1 || !t2)
		return -1;
    else 
	{
		res = ((t1->tv_sec - t2->tv_sec) * 1000 + t1->tv_usec - t2->tv_usec) / 1000;
		return (long) res;
    }
}

void aodv_socket_init(void)
{
	struct sockaddr_in aodv_addr;
	struct ifreq ifval;
	u8_t on = 1;
	u8_t tos = IPTOS_LOWDELAY;
	u32_t bufsize = SOCK_RECVBUF_SIZE;
	socklen_t optlen = sizeof(bufsize);
	
	if(!this_host.dev.enabled)
		printf("Interface not enabled!\n");

	this_host.dev.sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(this_host.dev.sock < 0)
	{
		printf("Socket init failed!\n");
		exit(-1);
	}

	//No gateway now
	
	memset(&aodv_addr, 0, sizeof(struct sockaddr_in));
	aodv_addr.sin_family = AF_INET;
	aodv_addr.sin_port = htons(AODV_PORT);
	aodv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(this_host.dev.sock, (struct sockaddr *)&aodv_addr, sizeof(struct sockaddr)))
	{
		printf("Bind error!\n");
		exit(-1);
	}

	if(setsockopt(this_host.dev.sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(u8_t)) < 0)
	{
		printf("SO_BROADCAST failed!\n");
		exit(-1);
	}	

	memset(&ifval, 0, sizeof(struct ifreq));
	strcpy(ifval.ifr_name, &this_host.dev.ifname);

	if(setsockopt(this_host.dev.sock, SOL_SOCKET, SO_BINDTODEVICE, &ifval, sizeof(struct ifreq)) < 0)
	{
		printf("SO_BINDTODEVICE %s failed!\n", this_host.dev.ifname);
		exit(-1);
	}

	if(setsockopt(this_host.dev.sock, SOL_SOCKET, SO_PRIORITY, &tos, sizeof(u8_t)) < 0)//u8_t or s8_t
	{
		printf("Set SO_PRIORITY failed!\n");
		exit(-1);
	}
	
	if(setsockopt(this_host.dev.sock, SOL_IP, IP_RECVTTL, &on, sizeof(u8_t)) < 0)
	{
		printf("Set IP_RECVTTL failed!\n");
		exit(-1);
	}

	if(setsockopt(this_host.dev.sock, SOL_IP, IP_PKTINFO, &on, sizeof(u8_t)) < 0)
	{
		printf("Set IP_PKTINFO failed!\n");
		exit(-1);
	}

	for(;; bufsize -= 1024)
	{
		if(setsockopt(this_host.dev.sock, SOL_SOCKET, SO_RCVBUF, (s8_t *)&bufsize, optlen) == 0)
		{
			printf("Receive buffer size set to %d\n", bufsize);
			break;
		}
		if(bufsize < RECE_BUF_SIZE)
		{
			printf("Could not set receive buffer size!\n");
			exit(-1);
		}
	}

	//bind_callback_fun// I use only one interface, it have not to bind the callback_fun, it calls when use 
	

	num_rreq = 0;
	num_rerr = 0;
}

u8_t *aodv_socket_new_msg(void)
{
	memset(send_buf, '\0', SEND_BUF_SIZE);
	return send_buf;
}

void aodv_socket_send(AODV_msg *aodv_msg, struct in_addr dest, u32_t len, u8_t ttl, struct dev_info *dev)
{
	struct timeval now;

	struct sockaddr_in dest_addr;
	
	if(wait_on_reboot && aodv_msg->type == AODV_RREP)//We cannot send any RREP when rebooting
		return;

	memset(&dest_addr, 0, sizeof(struct sockaddr_in));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr = dest;
	dest_addr.sin_port = htons(AODV_PORT);	

	if(setsockopt(dev->sock, SOL_IP, IP_TTL, &ttl, sizeof(u8_t)) < 0)
	{	
		printf("Error setting ttl!\n");
		return;
	}
	
	if(ratelimit)//RATELIMIT enable
	{
		gettimeofday(&now, NULL);

		switch(aodv_msg->type)
		{
			case AODV_RREQ:
							if(num_rreq == RREQ_RATELIMIT)//after RREQ_RATELIMIT, it should always be this branch
							{
								if(timeval_diff(&now, &rreq_ratelimit[0]) < 1000)
								{
									printf("RATELIMIT: Droppping RREQ %ld ms!\n", timeval_diff(&now, &rreq_ratelimit[0]));
									return;
								}
								else
								{
									memmove(rreq_ratelimit, &rreq_ratelimit[1], sizeof(struct timeval) * (num_rreq - 1));
									memcpy(&rreq_ratelimit[num_rreq - 1], &now, sizeof(struct timeval));
									//move rreq_ratelimit[1]-[9] to rreq_ratelimit[0]-[8], copy now to rreq_ratelimit[9]
								}
							}
							else
							{
								memcpy(&rreq_ratelimit[num_rreq], &now, sizeof(struct timeval));
								num_rreq++;//it seems like num_rreq never reset after init
							}
							break;
			case AODV_RERR:
							if(num_rerr == RERR_RATELIMIT)//after RERR_RATELIMIT, it should always be this branch
							{
								if(timeval_diff(&now, &rerr_ratelimit[0]) < 1000)
								{
									printf("RATELIMIT: Droppping RERR %ld ms!\n", timeval_diff(&now, &rerr_ratelimit[0]));
									return;
								}
								else
								{
									memmove(rerr_ratelimit, &rerr_ratelimit[1], sizeof(struct timeval) * (num_rerr - 1));
									memcpy(&rerr_ratelimit[num_rerr - 1], &now, sizeof(struct timeval));
									//move rerr_ratelimit[1]-[9] to rerr_ratelimit[0]-[8], copy now to rerr_ratelimit[9]
								}
							}
							else
							{
								memcpy(&rerr_ratelimit[num_rerr], &now, sizeof(struct timeval));
								num_rerr++;//it seems like num_rerr never reset after init
							}
							break;

		}
	}

	if(dest.s_addr == AODV_BROADCAST)
		gettimeofday(&this_host.last_broadcast_time, NULL);
		
	if(sendto(dev->sock, send_buf, len, 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0)
	{
		printf("Failed send to %s\n", inet_ntoa(dest));
		return;
	}

	//Why the example do not print hello message here?...
}

void aodv_socket_package_process(AODV_msg *aodv_msg, s32_t len, struct in_addr src, struct in_addr dest, s32_t ttl)
{
	if((aodv_msg->type == AODV_RREP) && (ttl == 1) && (dest.s_addr == AODV_BROADCAST))
		aodv_msg->type = AODV_HELLO;

	neighbor_add(aodv_msg, src);

	switch(aodv_msg->type)
	{
		case AODV_HELLO :   hello_process((RREP *)aodv_msg, len);
							printf("Received HELLO package!\n");
							break;
		case AODV_RREQ :	rreq_process((RREQ *)aodv_msg, len);
							printf("Received RREQ package!\n");
							break;
		case AODV_RREP :	rrep_process((RREP *)aodv_msg, len);
							printf("Received RREP package!\n");
							break;
		case AODV_RERR :	rerr_process((RERR *)aodv_msg, len);
							printf("Received RERR package!\n");
							break;
		case AODV_RREP_ACK :rrep_ack_process((RREP_ACK *)aodv_msg, len);
							printf("Received RREP_ACK package!\n");
							break;
		default :			printf("Unknown msg type %u received from %s to %s", aodv_msg->type, inet_ntoa(src), inet_ntoa(dest));
							break;
	}
}

void aodv_socket_read(s32_t fd)
{
	struct in_addr src, dest;
	s32_t len, ttl = -1;
	AODV_msg* aodv_msg;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	s8_t ctrlbuf[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct in_pktinfo))];
	struct sockaddr_in src_addr;

	dest.s_addr = -1;

	iov.iov_base = rece_buf;
	iov.iov_len = RECE_BUF_SIZE;
	msg.msg_name = &src_addr;
	msg.msg_namelen = sizeof(src_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = ctrlbuf;
	msg.msg_controllen = sizeof(ctrlbuf);

	len = recvmsg(fd, &msg, 0);
	if(len < 0)
	{
		printf("Receive data error!\n");
		return;
	}

	src.s_addr = src_addr.sin_addr.s_addr;

	if(this_host.dev.enabled && !memcmp(&src, &this_host.dev.ipaddr, sizeof(struct in_addr)))
	{
		printf("A local package returns to us again!\n");
		return;
	}
	
	aodv_msg = (AODV_msg *)rece_buf;

	aodv_socket_package_process(aodv_msg, len, src, dest, ttl);

	//Only one device, no device choose
}
