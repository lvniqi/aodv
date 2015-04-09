#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/netlink.h>
#include <linux/security.h>
#include <net/sock.h>
#include <net/netlink.h>

#include "kaodv_netlink.h"
#include "kaodv_expl.h"
#include "kaodv_queue.h"
#include "kaodv.h"

static int peer_pid;
static struct sock *kaodvnl;

//extern int active_route_timeout, qual_th, is_gateway;
extern int active_route_timeout;

/*
static struct sk_buff * kaodv_netlink_build_msg(int type, void *data, int len)
{
	unsigned char *old_tail;
	size_t size = 0;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	void *m;

	size = NLMSG_SPACE(len);

	skb = alloc_skb(size, GFP_ATOMIC);

	if(!skb)
	{
		if(skb)//???????????????????????????????????????
			kfree_skb(skb);

		printk(KERN_ERR "kaodv: failed to create rt timeout message\n");

		return NULL;
	}

	old_tail = SKB_TAIL_PTR(skb);//head of data
	
	//nlh = NLMSG_PUT(skb, 0, 0, type, size - sizeof(struct nlmsghdr));
	// linux 3.18 don`t have this macro
	nlh = nlmsg_put(skb, 0, 0, type, size - sizeof(struct nlmsghdr), 0);//flags = 0;

	m = NLMSG_DATA(nlh);

	memcpy(m, data, len);

	nlh->nlmsg_len = SKB_TAIL_PTR(skb) - old_tail;
	//NETLINK_CB(skb).pid = 0;
	//linux 3.18 have no pid
	NETLINK_CB(skb).portid = 0;//for compile, don`t know what it means

	return skb;
}
*/

static struct sk_buff * kaodv_netlink_build_msg(int type, void *data, int len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;

	skb = nlmsg_new(len, GFP_ATOMIC);

	if(!skb)
	{
		if(skb)//???????????????????????????????????????
			kfree_skb(skb);

		printk(KERN_ERR "kaodv: failed to create rt timeout message\n");

		return NULL;
	}

	nlh = nlmsg_put(skb, 0, 0, type, len, 0);//flags = 0;

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = AODVGRP_NOTIFY;

	memcpy(NLMSG_DATA(nlh), data, len);

	return skb;
}

void kaodv_netlink_send_debug_msg(char *buf, int len)
{
	struct sk_buff *skb = NULL;

	skb = kaodv_netlink_build_msg(KAODVM_DEBUG, buf, len);

	if(skb == NULL)
	{
		printk("kaodv_netlink_debug: skb = NULL\n");
		return;
	}

//	netlink_broadcast(kaodvnl, skb, peer_pid, AODVGRP_NOTIFY, GFP_USER);
	netlink_broadcast(kaodvnl, skb, 0, AODVGRP_NOTIFY, GFP_USER);
}

void kaodv_netlink_send_rt_msg(int type, __u32 src, __u32 dest)
{
	struct sk_buff *skb = NULL;
	struct kaodv_rt_msg m;

	memset(&m, 0, sizeof(m));

	m.src = src;
	m.dest = dest;

	skb = kaodv_netlink_build_msg(type, &m, sizeof(struct kaodv_rt_msg));

	if(skb == NULL)
	{
		printk("kaodv_netlink_rt_msg: skb = NULL\n");
		return;
	}

	netlink_broadcast(kaodvnl, skb, 0, AODVGRP_NOTIFY, GFP_USER);
}

void kaodv_netlink_send_rt_update_msg(int type, __u32 src, __u32 dest, int ifindex)
{
	struct sk_buff *skb = NULL;
	struct kaodv_rt_msg m;

	memset(&m, 0, sizeof(m));

	m.type = type;
	m.src = src;
	m.dest = dest;
	m.ifindex = ifindex;
	
	skb = kaodv_netlink_build_msg(KAODVM_ROUTE_UPDATE, &m, sizeof(struct kaodv_rt_msg));

	if(skb == NULL)
	{
		printk("kaodv_netlink_rt_update: skb = NULL\n");
		return;
	}
	
	netlink_broadcast(kaodvnl, skb, 0, AODVGRP_NOTIFY, GFP_USER);
}

void kaodv_netlink_send_rerr_msg(int type, __u32 src, __u32 dest, int ifindex)
{
	struct sk_buff *skb = NULL;
	struct kaodv_rt_msg m;

	memset(&m, 0, sizeof(m));

	m.type = type;
	m.src = src;
	m.dest = dest;
	m.ifindex = ifindex;

	skb = kaodv_netlink_build_msg(KAODVM_SEND_RERR, &m, sizeof(struct kaodv_rt_msg));

	if(skb == NULL)
	{
		printk("kaodv_netlink_rt_rerr: skb = NULL\n");
		return;
	}

	netlink_broadcast(kaodvnl, skb, 0, AODVGRP_NOTIFY, GFP_USER);
}

static int kaodv_netlink_receive_peer(unsigned char type, void *msg, unsigned int len)
{
	int ret = 0;
	struct kaodv_rt_msg *m;
	struct kaodv_conf_msg *cm;
	struct expl_entry e;

	printk("Received msg %s\n", kaodv_msg_type_to_str(type));

	switch(type)
	{
		case KAODVM_ADDROUTE:
			if(len < sizeof(struct kaodv_rt_msg))
				return -EINVAL;
			
			m = (struct kaodv_rt_msg *)msg;

			ret = kaodv_expl_get(m->dest, &e);
			
			if(ret)//no negative value
				ret = kaodv_expl_update(m->dest, m->nhop, m->time, m->flags, m->ifindex);
			else
				ret = kaodv_expl_add(m->dest, m->nhop, m->time, m->flags, m->ifindex);

			kaodv_queue_set_verdict(KAODV_QUEUE_SEND, m->dest);
			break;
		
		case KAODVM_DELROUTE:
			if(len < sizeof(struct kaodv_rt_msg))
				return -EINVAL;
			
			m = (struct kaodv_rt_msg *)msg;

			kaodv_expl_del(m->dest);
			kaodv_queue_set_verdict(KAODV_QUEUE_DROP, m->dest);
			break;

		case KAODVM_NOROUTE_FOUND:
			if(len < sizeof(struct kaodv_rt_msg))
				return -EINVAL;
			
			m = (struct kaodv_rt_msg *)msg;

			printk("No route found for %s\n", print_ip(m->dest));
			kaodv_queue_set_verdict(KAODV_QUEUE_DROP, m->dest);
			break;

		case KAODVM_CONFIG:
			if(len < sizeof(struct kaodv_conf_msg))
				return -EINVAL;
			
			cm = (struct kaodv_conf_msg *)msg;
			active_route_timeout = cm->active_route_timeout;	
			//qual_th = cm->qual_th;
			//is_gateway = cm->is_gateway;
			break;
		default:
			printk("kaodv-netlink: Unkown message type\n");
			ret = -EINVAL;
	}

	return ret;
}

static int kaodv_netlink_rcv_nl_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct netlink_notify *n = ptr;

//	if(event == NETLINK_URELEASE && n->protocol == NETLINK_AODV && n->pid)
//linux 3.18 have no pid

	if(event == NETLINK_URELEASE && n->protocol == NETLINK_AODV && n->portid)
	{
	//	if(n->pid == peer_pid)
		if(n->portid == peer_pid)
		{
			peer_pid = 0;
			kaodv_expl_flush();
			kaodv_queue_flush();
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block kaodv_nl_notifier = 
{
	.notifier_call = kaodv_netlink_rcv_nl_event,
};

#define RCV_SKB_FAIL(err) do {netlink_ack(skb, nlh, (err)); return;} while(0)

static inline void kaodv_netlink_rcv_skb(struct sk_buff *skb)
{
	int status, type, pid, flags, nlmsglen, skblen;
	struct nlmsghdr *nlh;

	skblen = skb->len;
	if(skblen < sizeof(struct nlmsghdr))
	{
		printk("skblen too small!\n");
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlmsglen = nlh->nlmsg_len;

	if(nlmsglen < sizeof(struct nlmsghdr) || skblen < nlmsglen)
	{
		printk("nlmsglen= %d, skblen= %d too small\n", nlmsglen, skblen);
		return;
	}

	pid = nlh->nlmsg_pid;
	flags = nlh->nlmsg_flags;

	if(pid <= 0 || !(flags & NLM_F_REQUEST) || flags & NLM_F_MULTI)
		RCV_SKB_FAIL(-EINVAL);

	if(flags & MSG_TRUNC)
		RCV_SKB_FAIL(-ECOMM);//error in communication

	type = nlh->nlmsg_type;

	printk("kaodv_netlink: type= %d\n", type);

	//if(security_netlink_recv(skb, CAP_NET_ADMIN))
	//	RCV_SKB_FAIL(-EPERM);
	//don`t have in 3.18

	if(peer_pid)
	{
		if(peer_pid != pid)
		{
			RCV_SKB_FAIL(-EBUSY);
		}
	}
	else
	{
		peer_pid = pid;
	}

	status = kaodv_netlink_receive_peer(type, NLMSG_DATA(nlh), skblen - NLMSG_LENGTH(0));
	
	if(status < 0)
		RCV_SKB_FAIL(status);

	if(flags & NLM_F_ACK)
		netlink_ack(skb, nlh, 0);
}

int kaodv_netlink_init(void)
{
	struct netlink_kernel_cfg kcfg = 
	{
		.groups = AODVGRP_MAX,
		.input = kaodv_netlink_rcv_skb,
		.cb_mutex = NULL,
	};

	netlink_register_notifier(&kaodv_nl_notifier);

	kaodvnl = netlink_kernel_create(&init_net, NETLINK_AODV, &kcfg);

	if(kaodvnl == NULL)
	{
		printk(KERN_ERR "kaodv_netlink: failed to create netlink socket!\n");
		netlink_unregister_notifier(&kaodv_nl_notifier);

		return -1;
	}

	return 0;
}

void kaodv_netlink_finish(void)
{
	sock_release(kaodvnl->sk_socket);

	netlink_unregister_notifier(&kaodv_nl_notifier);
}
