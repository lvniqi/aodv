#include <string.h>
#include <asm/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/select.h>
#include <linux/rtnetlink.h>

#include "nl.h"
#include "kaodv/src/kaodv_netlink.h"
#include "aodv_rreq.h"
#include "aodv_timeout.h"
#include "routing_table.h"
#include "aodv_hello.h"
#include "parameters.h"
#include "aodv_socket.h"
#include "aodv_rerr.h"
#include "debug.h"

struct nlsock
{
	s32_t sock;
	s32_t seqno;
	struct sockaddr_nl local;
};

struct sockaddr_nl peer;

struct nlsock aodvnl;
struct nlsock rtnl;

static void nl_kaodv_callback(s32_t sock);
static void nl_rt_callback(s32_t sock);

//extern s32_t llfeedback, active_route_timeout, qual_threshold, wait_on_reboot;
extern s32_t active_route_timeout, wait_on_reboot;

extern struct timer worb_timer;

#define BUFLEN 256

void nl_init(void)
{
	s32_t status;
	u32_t addrlen;

	memset(&peer, 0, sizeof(struct sockaddr_nl));
	peer.nl_family = AF_NETLINK;
	peer.nl_pid = 0;
	peer.nl_groups = 0;

	memset(&aodvnl, 0, sizeof(struct nlsock));
	aodvnl.seqno = 0;
	aodvnl.local.nl_family = AF_NETLINK;
	aodvnl.local.nl_groups = AODVGRP_NOTIFY;
	aodvnl.local.nl_pid = getpid();

	aodvnl.sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_AODV);

	if(aodvnl.sock < 0)
	{
		perror("Unable to create AODV netlink socket");
		exit(-1);
	}

	status = bind(aodvnl.sock, (struct sockaddr *)&aodvnl.local, sizeof(aodvnl.local));

	if(status < 0)
	{
		perror("Bind for AODV netlink failed");
		exit(-1);
	}

	addrlen = sizeof(aodvnl.local);

	if(getsockname(aodvnl.sock, (struct sockaddr *)&aodvnl.local, &addrlen) < 0)
	{
		perror("Getsockname failed");
		exit(-1);	
	}

	if(attach_callback_func(aodvnl.sock, nl_kaodv_callback) < 0)
	{
		DEBUG(LOG_WARNING, 0, "Attach failed");
	}

	memset(&rtnl, 0, sizeof(struct nlsock));
	rtnl.seqno = 0;
	rtnl.local.nl_family = AF_NETLINK;
	rtnl.local.nl_groups = RTMGRP_NOTIFY | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;
	rtnl.local.nl_pid = getpid();

	rtnl.sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	if(rtnl.sock < 0)
	{
		perror("Unable to create RT netlink socket");
		exit(-1);
	}

	addrlen = sizeof(rtnl.local);

	status = bind(rtnl.sock, (struct sockaddr *)&rtnl.local, addrlen);

	if(status < 0)
	{
		perror("Bind for RT netlink socket failed");
		exit(-1);
	}

	if(getsockname(rtnl.sock, (struct sockaddr *)&rtnl.local, &addrlen) < 0)
	{
		perror("Getsockname failed");
		exit(-1);
	}

	if(attach_callback_func(rtnl.sock, nl_rt_callback) < 0)
	{
		DEBUG(LOG_WARNING, 0, "Attach failed");
	}
}

void nl_cleanup(void)
{
	close(aodvnl.sock);
	close(rtnl.sock);
}

static void nl_kaodv_callback(s32_t sock)
{
	s32_t len;
	socklen_t addrlen;
	struct nlmsghdr *nlm;
	struct nlmsgerr *nlmerr;
	s8_t buf[BUFLEN];
	struct in_addr dest_addr, src_addr;
	kaodv_rt_msg_t *m;
	rt_table_t *rt, *fwd_rt, *rev_rt = NULL;
	
	addrlen = sizeof(struct sockaddr_nl);

	len = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&peer, &addrlen);

	if(len < 0)
	{
		return;
	}

	nlm = (struct nlmsghdr *)buf;

	switch(nlm->nlmsg_type)
	{
		case NLMSG_ERROR:
			nlmerr = NLMSG_DATA(nlm);
			if(nlmerr->error == 0)
			{
				DEBUG(LOG_DEBUG, 0, "NLMSG_ACK");
			}	
			else
			{
				DEBUG(LOG_WARNING, 0, "NLMSG_ERROR, error= %d, type= %s", nlmerr->error, kaodv_msg_type_to_str(nlmerr->msg.nlmsg_type));
			}
			break;

		case KAODVM_DEBUG:
			DEBUG(LOG_DEBUG, 0, "kaodv: %s", (s8_t *)NLMSG_DATA(nlm));
			break;

		case KAODVM_TIMEOUT:
			m = NLMSG_DATA(nlm);
			dest_addr.s_addr = m->dest;

			DEBUG(LOG_DEBUG, 0, "Got timeout msg form kernel for %s", ip_to_str(dest_addr));

			rt = rt_table_check(dest_addr);

			if(rt && rt->state == VALID)
			{
				route_expire_timeout(rt);
			}
			else
			{
				DEBUG(LOG_DEBUG, 0, "Got rt timeout msg but there is no route");
			}
			break;

		case KAODVM_ROUTE_REQ:
			m = NLMSG_DATA(nlm);
			dest_addr.s_addr = m->dest;

			DEBUG(LOG_DEBUG, 0, "Got ROUTE_REQ from kernel for %s", ip_to_str(dest_addr));

			rreq_route_discovery(dest_addr, 0);
			break;

		case KAODVM_REPAIR:
			m = NLMSG_DATA(nlm);
			dest_addr.s_addr = m->dest;
			src_addr.s_addr = m->src;

			DEBUG(LOG_DEBUG, 0, "Got ROUTE_REPAIR from kernel for %s", ip_to_str(dest_addr));

			fwd_rt = rt_table_check(dest_addr);

			if(fwd_rt)
			{
				rreq_local_repair(fwd_rt, src_addr);
			}
			break;

		case KAODVM_ROUTE_UPDATE:
			m = NLMSG_DATA(nlm);
			dest_addr.s_addr = m->dest;
			src_addr.s_addr = m->src;

			DEBUG(LOG_DEBUG, 0, "Got ROUTE_UPDATE from kernel, S= %s, D= %s", ip_to_str(src_addr), ip_to_str(dest_addr));


			if(dest_addr.s_addr == AODV_BROADCAST || dest_addr.s_addr == this_host.dev.broadcast.s_addr)
				return;
			
			fwd_rt = rt_table_check(dest_addr);
			rev_rt = rt_table_check(src_addr);

			rt_table_update_route_timeouts(fwd_rt, rev_rt);
			break;

		case KAODVM_SEND_RERR:
			m = NLMSG_DATA(nlm);
			dest_addr.s_addr = m->dest;
			src_addr.s_addr = m->src;

			DEBUG(LOG_DEBUG, 0, "Got SEND_RERR from kernel, S= %s, D= %s", ip_to_str(src_addr), ip_to_str(dest_addr));

			if(dest_addr.s_addr == AODV_BROADCAST || dest_addr.s_addr == this_host.dev.broadcast.s_addr)
				return;
			
			fwd_rt = rt_table_check(dest_addr);
			rev_rt = rt_table_check(src_addr);
			
			do
			{
				struct in_addr rerr_dest;
				RERR *rerr;

				DEBUG(LOG_DEBUG, 0, "Sending RERR for unsolicited message from %s to %s", ip_to_str(src_addr), ip_to_str(dest_addr));

				if(fwd_rt)
				{
					rerr = rerr_create(0, fwd_rt->dest_addr, fwd_rt->dest_seqno);
					rt_table_update_timeout(fwd_rt, DELETE_PERIOD);
				}
				else
				{
					rerr = rerr_create(0, dest_addr, 0);
				}

				if(rev_rt && rev_rt->state == VALID)
					rerr_dest = rev_rt->next_hop;
				else
					rerr_dest.s_addr = AODV_BROADCAST;

				aodv_socket_send((AODV_msg *)rerr, rerr_dest, RERR_CALC_SIZE(rerr), 1, &this_host.dev);
				if(wait_on_reboot)
				{
					DEBUG(LOG_DEBUG, 0, "Wait on reboot timer reset");
					timer_set_timeout(&worb_timer, DELETE_PERIOD);
				}
			}while(0);
			break;
		default:
			DEBUG(LOG_DEBUG, 0, "Got msg type= %d", nlm->nlmsg_type);
	}
}

static void nl_rt_callback(s32_t sock)
{
	s32_t len, attrlen;
	socklen_t addrlen;
	struct nlmsghdr *nlm;
	struct nlmsgerr *nlmerr;
	s8_t buf[BUFLEN];
	struct ifaddrmsg *ifm;
	struct rtattr *rta;

	addrlen = sizeof(struct sockaddr_nl);

	len = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&peer, &addrlen);

	if(len < 0)
	{
		return;
	}

	nlm = (struct nlmsghdr *)buf;

	switch(nlm->nlmsg_type)
	{
		case NLMSG_ERROR:
			nlmerr = NLMSG_DATA(nlm);
			if(nlmerr == 0)
				DEBUG(LOG_DEBUG, 0, "NLMSG_ACK");
			else
				DEBUG(LOG_WARNING, 0, "NLMSG_ERROR, error= %d, type= %d", nlmerr->error, nlmerr->msg.nlmsg_type);
			break;
		case RTM_NEWLINK:
			DEBUG(LOG_DEBUG, 0, "RTM_NEWADDR");
			break;
		case RTM_NEWADDR:
			ifm = NLMSG_DATA(nlm);
			rta = (struct rtattr *)((s8_t *)ifm + sizeof(ifm));

			attrlen = nlm->nlmsg_len - sizeof(struct nlmsghdr) - sizeof(struct ifaddrmsg);

			for(; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen))
			{
				if(rta->rta_type == IFA_ADDRESS)
				{
					struct in_addr ifaddr;

					memcpy(&ifaddr, RTA_DATA(rta), RTA_PAYLOAD(rta));

					DEBUG(LOG_DEBUG, 0, "Interface index %d changed address to %s", ifm->ifa_index, ip_to_str(ifaddr));
				}
			}
			break;
		case RTM_NEWROUTE:
			DEBUG(LOG_DEBUG, 0, "RTM_NEWROUTE");
			break;
		default:
			DEBUG(LOG_DEBUG, 0, "Unkown type of kernel message");
			break;
	}
}

s32_t prefix_length(s32_t family, void *nm)
{
	s32_t prefix = 0;

	if(family == AF_INET)
	{
		u32_t tmp;

		memcpy(&tmp, nm, sizeof(u32_t));

		while(tmp)
		{
			tmp = tmp << 1;
			prefix++;
		}

		return prefix;
	}
	else
	{
		DEBUG(LOG_WARNING, 0, "Unsupported address family");
	}

	return 0;
}

s32_t addattr(struct nlmsghdr *n, s32_t type, void *data, s32_t alen)
{
	struct rtattr *attr;
	s32_t len = RTA_LENGTH(alen);

	attr = (struct rtattr *)(((s8_t *)n) + NLMSG_LENGTH(n->nlmsg_len));
	attr->rta_type = type;
	attr->rta_len = len;
	memcpy(RTA_DATA(attr), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;

	return 0;
}

#define ATTR_BUFLEN 512

s32_t nl_send(struct nlsock *nl, struct nlmsghdr *n)
{
	s32_t res;
	struct iovec iov = {(void *)n, n->nlmsg_len};
	struct msghdr msg = {(void *)&peer, sizeof(peer), &iov, 1, NULL, 0, 0};

	if(!nl)
		return -1;

	n->nlmsg_seq = ++nl->seqno;
	n->nlmsg_pid = nl->local.nl_pid;

	n->nlmsg_flags |= NLM_F_ACK;

	res = sendmsg(nl->sock, &msg, 0);

	if(res < 0)
	{
		perror("sendmsg error");
		return -1;
	}

	return 0;
}

s32_t nl_kernel_route(s32_t action, s32_t flags, s32_t family, s32_t index, struct in_addr *dest, struct in_addr *gw, struct in_addr *nm, s32_t metric)
{
	struct 
	{
		struct nlmsghdr nlh;
		struct rtmsg rtm;
		s8_t attrbuf[ATTR_BUFLEN];
	}req;

	if(!dest || !gw)
		return -1;

	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.nlh.nlmsg_type = action;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | flags;
	req.nlh.nlmsg_pid = 0;

	req.rtm.rtm_family = family;

	if(!nm)
		req.rtm.rtm_dst_len = sizeof(struct in_addr) * 8;
	else
		req.rtm.rtm_dst_len = prefix_length(AF_INET, nm);

	req.rtm.rtm_src_len = 0;
	req.rtm.rtm_tos = 0;
	req.rtm.rtm_table = RT_TABLE_MAIN;
	req.rtm.rtm_protocol = 100;
	req.rtm.rtm_scope = RT_SCOPE_LINK;
	req.rtm.rtm_type = RTN_UNICAST;
	req.rtm.rtm_flags = 0;
/*
 *RT_SCOPE_UNIVERSE=0,  //任意的地址路由
 *RT_SCOPE_SITE=200,    //用户自定义
 *RT_SCOPE_LINK=253,    //本地直连的路由
 *RT_SCOPE_HOST=254,    //主机路由
 *RT_SCOPE_NOWHERE=255  //不存在的路由
*/
	addattr(&req.nlh, RTA_DST, dest, sizeof(struct in_addr));

	if(memcmp(dest, gw, sizeof(struct in_addr)) != 0)
	{
		req.rtm.rtm_scope = RT_SCOPE_UNIVERSE;
		addattr(&req.nlh, RTA_GATEWAY, gw, sizeof(struct in_addr));
	}
	
	if(index > 0)
	{
		addattr(&req.nlh, RTA_OIF, &index, sizeof(index));
	}

	addattr(&req.nlh, RTA_PRIORITY, &metric, sizeof(metric));

	return nl_send(&rtnl, &req.nlh);
}

s32_t nl_send_add_route_msg(struct in_addr dest, struct in_addr next_hop, s32_t metric, u32_t lifetime, s32_t rt_flags, s32_t ifindex)
{
	struct 
	{
		struct nlmsghdr n;
		struct kaodv_rt_msg m;
	}areq;

	DEBUG(LOG_DEBUG, 0, "ADD/UPDATE: %s:%s ifindex= %d", ip_to_str(dest), ip_to_str(next_hop), ifindex);

	memset(&areq, 0, sizeof(areq));

	areq.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct kaodv_rt_msg));
	areq.n.nlmsg_type = KAODVM_ADDROUTE;
	areq.n.nlmsg_flags = NLM_F_REQUEST;

	areq.m.dest = dest.s_addr;
	areq.m.nhop = next_hop.s_addr;
	areq.m.time = lifetime;
	areq.m.ifindex = ifindex;

	if(rt_flags & RT_INET_DEST)
	{
		areq.m.flags |= KAODV_RT_GW_ENCAP;
	}
	
	if(rt_flags & RT_REPAIR)
	{
		areq.m.flags |= KAODV_RT_REPAIR;
	}

	if(nl_send(&aodvnl, &areq.n) < 0)
	{
		perror("Failed to send netlink message!\n");
		return -1;
	}

	return nl_kernel_route(RTM_NEWROUTE, NLM_F_CREATE, AF_INET, ifindex, &dest, &next_hop, NULL, metric);
}

s32_t nl_send_no_route_found_msg(struct in_addr dest)
{
	struct 
	{
		struct nlmsghdr n;
		struct kaodv_rt_msg m;
	}areq;

	memset(&areq, 0, sizeof(areq));

	areq.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct kaodv_rt_msg));
	areq.n.nlmsg_type = KAODVM_NOROUTE_FOUND;
	areq.n.nlmsg_flags = NLM_F_REQUEST;

	areq.m.dest = dest.s_addr;

	DEBUG(LOG_DEBUG, 0, "Send NOROUTE_FOUND to kernel: %s", ip_to_str(dest));

	return nl_send(&aodvnl, &areq.n);
}

s32_t nl_send_del_route_msg(struct in_addr dest, struct in_addr next_hop, s32_t metric)
{
	s32_t index = -1;
	struct
	{
		struct nlmsghdr n;
		struct kaodv_rt_msg m;
	}areq;

	DEBUG(LOG_DEBUG, 0, "Send DEL_ROUTE to kernel: %s", ip_to_str(dest));

	memset(&areq, 0, sizeof(areq));

	areq.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct kaodv_rt_msg));
	areq.n.nlmsg_type = KAODVM_DELROUTE;
	areq.n.nlmsg_flags = NLM_F_REQUEST;

	areq.m.dest = dest.s_addr;
	areq.m.nhop = next_hop.s_addr;
	areq.m.time = 0;
	areq.m.flags = 0;

	if(nl_send(&aodvnl, &areq.n) < 0)
	{
		perror("Failed to send netlink message");
		return -1;
	}

	return nl_kernel_route(RTM_DELROUTE, 0, AF_INET, index, &dest, &next_hop, NULL, metric);
}

s32_t nl_send_conf_msg(void)
{
	struct
	{
		struct nlmsghdr n;
		struct kaodv_conf_msg cm;
	}areq;

	DEBUG(LOG_DEBUG, 0, "Send CONF_MSG to kernel");

	memset(&areq, 0, sizeof(areq));

	areq.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct kaodv_rt_msg));
	areq.n.nlmsg_type = KAODVM_CONFIG;
	areq.n.nlmsg_flags = NLM_F_REQUEST;

	//areq.cm.qual_th = qual_threshold;
	areq.cm.active_route_timeout = active_route_timeout;
	
	return nl_send(&aodvnl, &areq.n);
}
