#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/route.h>

#include "kaodv_mod.h"
#include "kaodv_expl.h"
#include "kaodv_netlink.h"
#include "kaodv_queue.h"
#include "kaodv_ipenc.h"
#include "kaodv.h"

#define ACTIVE_ROUTE_TIMEOUT active_route_timeout
#define MAX_INTERFACES 10

//static int qual = 0;
//static unsigned long pkts_dropped = 0;
//int qual_th = 0;
//int is_gateway = 1;
int active_route_timeout = 3000;

MODULE_DESCRIPTION("aodv-project");
MODULE_AUTHOR("lbw");
MODULE_LICENSE("GPL");

#define ADDR_HOST 1
#define AODV_BROADCAST 2

void kaodv_update_route_timeouts(int hooknum, const struct net_device *dev, struct iphdr *iph)
{
	struct expl_entry e;
	struct in_addr bcaddr;
	int res;

	bcaddr.s_addr = 0;

	res = if_info_from_ifindex(NULL, &bcaddr, dev->ifindex);

	if(res < 0)
		return;
	if(hooknum == NF_INET_PRE_ROUTING)
	{
		kaodv_netlink_send_rt_update_msg(PKT_INBOUND, iph->saddr, iph->daddr, dev->ifindex);
		
	}
	else if(iph->daddr != INADDR_BROADCAST && iph->daddr != bcaddr.s_addr)//not broadcast address
		kaodv_netlink_send_rt_update_msg(PKT_OUTBOUND, iph->saddr, iph->daddr, dev->ifindex);

	if(kaodv_expl_get(iph->daddr, &e))//just update timeout forward
	{
		kaodv_expl_update(e.daddr, e.nhop, ACTIVE_ROUTE_TIMEOUT, e.flags, dev->ifindex);

		if(e.nhop != e.daddr && kaodv_expl_get(e.nhop, &e))//not a neighbor
		{
			kaodv_expl_update(e.daddr, e.nhop, ACTIVE_ROUTE_TIMEOUT, e.flags, dev->ifindex);
		}
	}

	if(kaodv_expl_get(iph->saddr, &e))//update timeout reverse
	{
		kaodv_expl_update(e.daddr, e.nhop, ACTIVE_ROUTE_TIMEOUT, e.flags, dev->ifindex);

		if(e.nhop != e.daddr && kaodv_expl_get(e.nhop, &e))//not a neighbor
		{
			kaodv_expl_update(e.daddr, e.nhop, ACTIVE_ROUTE_TIMEOUT, e.flags, dev->ifindex);
		}
	}
}

static unsigned int kaodv_hook(const struct nf_hook_ops *ops, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph = SKB_NETWORK_HDR_IPH(skb);
	struct expl_entry e;
	struct in_addr ifaddr, bcaddr;
	int res = 0;

	memset(&ifaddr, 0, sizeof(struct in_addr));
	memset(&bcaddr, 0, sizeof(struct in_addr));

	if(iph == NULL)
		return NF_ACCEPT;

	if(iph && iph->protocol == IPPROTO_UDP)
	{
		struct udphdr *udph;

		udph = (struct udphdr *)((char *)iph + (iph->ihl << 2));//ihl: the length of ip_header, 1 bit  in ihl represent 32-bit data, so << 2 = bytes

		if(ntohs(udph->dest) == AODV_PORT || ntohs(udph->source) == AODV_PORT)
		{
			//no qual, I don`t know what its means and I can`t find 'iwq' in sk_buff structure	
			if(ops->hooknum == NF_INET_PRE_ROUTING && in)
				kaodv_update_route_timeouts(ops->hooknum, in, iph);

				return NF_ACCEPT;
		}
	}
	
	if(ops->hooknum == NF_INET_PRE_ROUTING)
		res = if_info_from_ifindex(&ifaddr, &bcaddr, in->ifindex);
	else
		res = if_info_from_ifindex(&ifaddr, &bcaddr, out->ifindex);
	
	if(res < 0)
		return NF_ACCEPT;

	if(iph->daddr == INADDR_BROADCAST || IN_MULTICAST(ntohl(iph->daddr)) || iph->daddr == bcaddr.s_addr)//ignore broadcast or multicast
		return NF_ACCEPT;

	switch(ops->hooknum)
	{
		case NF_INET_PRE_ROUTING:
			kaodv_update_route_timeouts(ops->hooknum, in, iph);

			//No gateway
			
			if(iph->saddr == ifaddr.s_addr || iph->daddr == ifaddr.s_addr)//to us or from us
			{
				return NF_ACCEPT; 
			}
			else if(!kaodv_expl_get(iph->daddr, &e))//no record
			{
				kaodv_netlink_send_rerr_msg(PKT_INBOUND, iph->saddr, iph->daddr, in->ifindex);
			
				return NF_DROP;
			}
			else if(e.flags & KAODV_RT_REPAIR)//need repair
			{
				kaodv_netlink_send_rt_msg(KAODVM_REPAIR, iph->saddr, iph->daddr);
				kaodv_queue_enqueue_packet(skb, okfn);

				return NF_STOLEN;
			}
			break;

		case NF_INET_LOCAL_OUT:
			if(!kaodv_expl_get(iph->daddr, &e) || (e.flags & KAODV_RT_REPAIR))
			{
				if(!kaodv_queue_check(iph->daddr))
					kaodv_netlink_send_rt_msg(KAODVM_ROUTE_REQ, 0, iph->daddr);

				kaodv_queue_enqueue_packet(skb, okfn);
				
				return NF_STOLEN;
			}
			else if(e.flags & KAODV_RT_GW_ENCAP)//completely don`t understand
			{
				//tcp_sock?

				kaodv_update_route_timeouts(ops->hooknum, out, iph);
				
				skb = ip_pkt_encapsulate(skb, e.nhop);

				if(!skb)
					return NF_STOLEN;

				ip_route_me_harder(skb, RTN_LOCAL);
			}
			break;

		case NF_INET_POST_ROUTING:
			kaodv_update_route_timeouts(ops->hooknum, out, iph);
			break;
	}
	
	return NF_ACCEPT;
 }

/*
 * seems that I don`t need this funtion 
 *
 * int kaodv_proc_info(char *buffer, char **start, off_t offset, int length)
{

}
*/

//#ifdef MT
static char *ifname[MAX_INTERFACES] = {"br-lan"}; 
//#else
//static char *ifname[MAX_INTERFACES] = {"wlan0"};
//#endif

////static int num_parms = 0;

////module_param_array(ifname, charp, &num_parms, 0444);
//module_param(qual_th, int, 0); //????????


static struct nf_hook_ops kaodv_ops[] = 
{
	{
		.hook = kaodv_hook,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_PRE_ROUTING,
		.priority = NF_IP_PRI_FIRST,
	},
	{
		.hook = kaodv_hook,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_LOCAL_OUT,
		.priority = NF_IP_PRI_FILTER,
	},
	{
		.hook = kaodv_hook,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_POST_ROUTING,
		.priority = NF_IP_PRI_FILTER,
	},
};

static int __init kaodv_init(void)
{
	struct net_device *dev = NULL;
	int i, ret = -ENOMEM;

	kaodv_expl_init();

	ret = kaodv_queue_init();

	if(ret < 0)
		return ret;

	ret = kaodv_netlink_init();

	if(ret < 0)
		goto cleanup_queue;

	ret = nf_register_hook(&kaodv_ops[0]);

	if(ret < 0)
		goto cleanup_netlink;

	ret = nf_register_hook(&kaodv_ops[1]);

	if(ret < 0)
		goto cleanup_hook0;

	ret = nf_register_hook(&kaodv_ops[2]);

	if(ret < 0)
		goto cleanup_hook1;

	for(i = 0; i < MAX_INTERFACES; i++)
	{
		if(!ifname[i])
			break;

		dev = dev_get_by_name(&init_net, ifname[i]);
		
		if(!dev)
		{
			printk("No device %s available, ignoring!\n", ifname[i]);
			continue;
		}

		if_info_add(dev);

		dev_put(dev);
	}

	//No proc
		
	printk("Module init OK!\n");

	return ret;

cleanup_hook1:
	nf_unregister_hook(&kaodv_ops[1]);
cleanup_hook0:
	nf_unregister_hook(&kaodv_ops[0]);
cleanup_netlink:
	kaodv_netlink_finish();
cleanup_queue:
	kaodv_queue_finish();

	return ret;
}

static void __exit kaodv_exit(void)
{
	unsigned int i;

	if_info_purge();

	for(i = 0; i < sizeof(kaodv_ops) / sizeof(struct nf_hook_ops); i++)
		nf_unregister_hook(&kaodv_ops[i]);

	//No proc
	
	kaodv_queue_finish();
	kaodv_expl_finish();
	kaodv_netlink_finish();

	printk("Exit kaodv\n");
}

module_init(kaodv_init);
module_exit(kaodv_exit);
