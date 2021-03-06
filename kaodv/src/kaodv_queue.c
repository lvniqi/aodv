#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/netfilter_ipv4.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/icmp.h>

#include "kaodv_queue.h"
#include "kaodv_expl.h"
#include "kaodv_netlink.h"
#include "kaodv_ipenc.h"
#include "kaodv.h"

#define KAODV_QUEUE_QMAX_DEFAULT 1024
#define KAODV_QUEUE_PROC_FS_NAME "kaodv_queue"
#define NET_KAODV_QUEUE_QMAX 2088
#define NET_KAODV_QUEUE_QMAX_NAME "kaodv_queue_maxlen"

struct kaodv_rt_info
{
	__u8 tos;//type of service
	__u32 daddr;
	__u32 saddr;
};

struct kaodv_queue_entry
{
	struct list_head list;
	struct sk_buff *skb;
	int (*okfn)(struct sk_buff *);
	struct kaodv_rt_info rt_info;
};

struct queue_show
{
	unsigned int total;
	unsigned int maxlen;
};

struct proc_dir_entry *queue_proc;

typedef int (*kaodv_queue_cmpfn)(struct kaodv_queue_entry *, unsigned long);

static unsigned int queue_total;
static unsigned int queue_maxlen = KAODV_QUEUE_QMAX_DEFAULT;
static rwlock_t queue_lock;// = RW_LOCK_UNLOCKED;//linux 3.18 don`t have

static struct queue_show queue_show_buf[1];//for proc seq output

static LIST_HEAD(queue_list);

static void queue_show_buf_update(void);//for proc

static inline int __kaodv_queue_enqueue_entry(struct kaodv_queue_entry *entry)
{
	if(queue_total >= queue_maxlen)
	{
		if(net_ratelimit())
			printk(KERN_WARNING "kaodv-queue: full at %d entries, dropping packets.\n", queue_total);
		
		return -ENOSPC;
	}

	list_add(&entry->list, &queue_list);
	queue_total++;

	//proc_seq_buf[0] = queue_total;//for proc

	return 0;
}

static inline struct kaodv_queue_entry * __kaodv_queue_check_entry(kaodv_queue_cmpfn cmpfn, unsigned long data)
{
	struct list_head *p;
	struct kaodv_queue_entry *entry;

	list_for_each_prev(p, &queue_list)
	{
		entry = (struct kaodv_queue_entry *)p;

		if(!cmpfn || cmpfn(entry, data))
			return entry;
	}

	return NULL;
}

static inline struct kaodv_queue_entry * __kaodv_queue_check_delqueue_entry(kaodv_queue_cmpfn cmpfn, unsigned long data)//find and delete from the queue
{
	struct kaodv_queue_entry *entry;

	entry = __kaodv_queue_check_entry(cmpfn, data);
	if(entry == NULL)
		return NULL;

	list_del(&entry->list);
	queue_total--;

	//proc_seq_buf[0] = queue_total;//for proc

	return entry;
}

static inline void __kaodv_queue_flush(void)
{
	struct kaodv_queue_entry *entry;

	while((entry = __kaodv_queue_check_delqueue_entry(NULL, 0)))
	{
		kfree_skb(entry->skb);
		kfree(entry);
	}
}

static inline void __kaodv_queue_reset(void)
{
	__kaodv_queue_flush();
}

static struct kaodv_queue_entry * kaodv_queue_check_delqueue_entry(kaodv_queue_cmpfn cmpfn, unsigned long data)
{
	struct kaodv_queue_entry *entry;

	write_lock_bh(&queue_lock);

	entry = __kaodv_queue_check_delqueue_entry(cmpfn, data);

	write_unlock_bh(&queue_lock);

	return entry;
}

void kaodv_queue_flush(void)
{
	write_lock_bh(&queue_lock);

	__kaodv_queue_flush();

	write_unlock_bh(&queue_lock);
}

int kaodv_queue_enqueue_packet(struct sk_buff *skb, int (*okfn)(struct sk_buff *))
{
	int status = -EINVAL;
	struct kaodv_queue_entry *entry;
	struct iphdr *iph = SKB_NETWORK_HDR_IPH(skb);

	entry = kmalloc(sizeof(struct kaodv_queue_entry), GFP_ATOMIC);

	if(entry == NULL)
	{
		printk(KERN_ERR "kaodv_queue: kmalloc failed in kaodv_queue_enqueue_packet()\n");
		return -ENOMEM;
	}

	printk("enquing packet queue_len= %d\n", queue_total);

	entry->okfn = okfn;
	entry->skb = skb;
	entry->rt_info.tos = iph->tos;
	entry->rt_info.daddr = iph->daddr;
	entry->rt_info.saddr = iph->saddr;

	write_lock_bh(&queue_lock);

	status = __kaodv_queue_enqueue_entry(entry);
	
	write_unlock_bh(&queue_lock);

	if(status < 0)
		kfree(entry);
		
	return status;
}

static inline int dest_cmp(struct kaodv_queue_entry *e, unsigned long daddr)
{
	return (daddr == e->rt_info.daddr);
}

int kaodv_queue_check(__u32 daddr)
{
	struct kaodv_queue_entry *entry;
	int res = 0;

	read_lock_bh(&queue_lock);
	
	entry = __kaodv_queue_check_entry(dest_cmp, daddr);

	if(entry != NULL)
		res = 1;

	read_unlock_bh(&queue_lock);

	return res;
}

int kaodv_queue_set_verdict(int verdict, __u32 daddr)
{
	struct kaodv_queue_entry *entry;
	int pkts = 0;

	if(verdict == KAODV_QUEUE_DROP)
	{
		while(1)
		{
			entry = kaodv_queue_check_delqueue_entry(dest_cmp, daddr);
			if(entry == NULL)
				return pkts;

			if(pkts == 0)//if find, send an ICMP informing the app that the dest was unreachable
				icmp_send(entry->skb, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH, 0);

			kfree_skb(entry->skb);
			kfree(entry);
			pkts++;
		}
	}
	else if(verdict == KAODV_QUEUE_SEND)
	{
		struct expl_entry e;

		while(1)
		{
			entry = kaodv_queue_check_delqueue_entry(dest_cmp, daddr);//just delete from the queue,not delete the sk_buff

			if(entry == NULL)
				return pkts;

			if(!kaodv_expl_get(daddr, &e))//timeout ---> delete the sk_buff
			{
				kfree_skb(entry->skb);
				kfree(entry);
				continue;
			}

			if(e.flags & KAODV_RT_GW_ENCAP)
			{
				entry->skb = ip_pkt_encapsulate(entry->skb, e.nhop);
				if(!entry->skb)
				{
					kfree(entry);
					continue;
				}
			}

			ip_route_me_harder(entry->skb, RTN_LOCAL);

			pkts++;

			entry->okfn(entry->skb);
			kfree(entry);
		}
	}

	return 0;
}

static void *queue_seq_start(struct seq_file *s, loff_t *pos)
{
	queue_show_buf_update();
	
	if(*pos >= 1)
		return NULL;
	return queue_show_buf + *pos;
}

static void *queue_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;

	if(*pos >= 1)
		return NULL;
	return queue_show_buf + *pos;
}

static void queue_seq_stop(struct seq_file *s, void *v)
{

}

static int queue_seq_show(struct seq_file *s, void *v)
{
	struct queue_show *tmp = (struct queue_show *)v;
	
	seq_printf(s, "Queue length      : %u\n"
				  "Queue max. length : %u\n", tmp->total, tmp->maxlen);

	return 0;
}

static struct seq_operations queue_seq_ops = 
{
	.start = queue_seq_start,
	.next = queue_seq_next,
	.stop = queue_seq_stop,
	.show = queue_seq_show
};

static int queue_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &queue_seq_ops);
}

static struct file_operations queue_proc_ops = 
{
	.owner = THIS_MODULE,
	.open = queue_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*static int kaodv_queue_get_info(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	int len;

	read_lock_bh(&queue_lock);

	len = sprintf(page, "Queue length      : %u\n"
						"Queue max. length : %u\n", queue_total, queue_maxlen);

	read_unlock_bh(&queue_lock);

	*start = page + offset;
	len -= offset;
	if(len > count)
		len = count;
	else if(len < 0)
		len = 0;

	return len;
}*/

static void queue_show_buf_update(void)
{
	queue_show_buf[0].total = queue_total;
	queue_show_buf[0].maxlen = queue_maxlen;
}

int kaodv_queue_init(void)
{
	queue_total = 0;

	queue_show_buf_update();

//	proc = create_proc_read_entry(KAODV_QUEUE_PROC_FS_NAME, 0, init_net.proc_net, kaodv_queue_get_info, NULL);

//different in 3.18
	queue_proc = proc_create(KAODV_QUEUE_PROC_FS_NAME, 0, init_net.proc_net, &queue_proc_ops);

	if(!queue_proc)
	{
		printk(KERN_ERR "kaodv_queue: failed to create proc entry\n");
		return -1;
	}

	return 1;
}

void kaodv_queue_finish(void)
{
	synchronize_net();
	
	kaodv_queue_flush();

	proc_remove(queue_proc);
}
