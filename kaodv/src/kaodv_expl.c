#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "kaodv_expl.h"
#include "kaodv_netlink.h"
#include "kaodv_queue.h"

#define EXPL_MAX_LEN 1024
#define KAODV_EXPL_PROC_FS_NAME "kaodv_expl"

static unsigned int expl_len;
static rwlock_t expl_lock;// = RW_LOCK_UNLOCKED;//linux 3.18 don`t have
static LIST_HEAD(expl_head);
struct proc_dir_entry *expl_proc;

#define list_is_first(e) (&e->l == expl_head.next)

#ifdef EXPL_TIMER
static struct timer_list expl_timer;

static void kaodv_expl_timeout(unsigned long data);

static inline void __kaodv_expl_set_next_timeout(void)
{
	struct expl_entry *ne;

	if(list_empty(&expl_head))
		return;

	ne = (struct expl_entry *)expl_head.next;

	if(timer_pending(&expl_timer))
	{
		mod_timer(&expl_timer, ne->expires);
	}
	else
	{
		expl_timer.function = kaodv_expl_timeout;
		expl_timer.expires = ne->expires;
		expl_timer.data = 0;
		add_timer(&expl_timer);
	}
}

static void kaodv_expl_timeout(unsigned long data)
{
	struct list_head *pos, *tmp;
	int time = jiffies;
	struct expl_entry *e;

	write_lock_bh(&expl_lock);

	list_for_each_safe(pos, tmp, &expl_head)
	{
		e = (struct expl_entry *)pos;

		if(e->expires > time)//have not reach the deadline
			break;

		list_del(&e->l);
		expl_len--;

		kaodv_queue_set_verdict(KAODV_QUEUE_DROP, e->daddr);
		//drop all packets for this dest
		
		kaodv_netlink_send_rt_msg(KAODVM_TIMEOUT, 0, e->daddr);
		//we have a bug here, set 0 for compile
		printk("expl_timeout: sending timeout msg to usrspace!\n");
	}

	__kaodv_expl_set_next_timeout();
	write_unlock_bh(&expl_lock);
}
#endif    /* EXPL_TIMER */

static inline void __kaodv_expl_flush(void)
{
	struct list_head *pos, *tmp;
	struct expl_entry *e;

	list_for_each_safe(pos, tmp, &expl_head)
	{
		e = (struct expl_entry *)pos;
		list_del(&e->l);
		expl_len--;
		kfree(e);
	}
}

static inline int __kaodv_expl_add(struct expl_entry *e)
{
	if(expl_len > EXPL_MAX_LEN)
	{
		printk("kaodv_expl: Max list len reached\n");
		return -ENOSPC;
	}

	if(list_empty(&expl_head))
	{
		list_add(&e->l, &expl_head);
	}
	else
	{
		struct list_head *pos;
		struct expl_entry *curr;

		list_for_each(pos, &expl_head)
		{
			curr = (struct expl_entry *)pos;

			if(curr->expires > e->expires)
				break;
		}

		list_add(&e->l, pos->prev);
	}

	return 1;
}

static inline struct expl_entry *__kaodv_expl_check(__u32 daddr)
{
	struct list_head *pos;
	struct expl_entry *e;

	list_for_each(pos, &expl_head)
	{
		e = (struct expl_entry *)pos;

		if(e->daddr == daddr)
			return e;
	}

	return NULL;
}

static inline int __kaodv_expl_del(struct expl_entry *e)
{
	if(e == NULL)
		return 0;

	if(list_is_first(e))
	{
		list_del(&e->l);
#ifdef EXPL_TIMER
		if(!list_empty(&expl_head))
		{
			struct expl_entry *f = (struct expl_entry *)expl_head.next;

			mod_timer(&expl_timer, f->expires);
		}
#endif
	}
	else
		list_del(&e->l);

	expl_len--;

	return 1;	
}

int kaodv_expl_del(__u32 daddr)
{
	int res;
	struct expl_entry *e;

	write_lock_bh(&expl_lock);

	e = __kaodv_expl_check(daddr);

	if(e == NULL)
	{
		write_unlock_bh(&expl_lock);
		printk("No route to del, maybe it deleted by expl_timeout\n");
		return 0;
	}

	res = __kaodv_expl_del(e);

	if(res)
	{
		kfree(e);
	}

	write_unlock_bh(&expl_lock);

	return 1;
}

int kaodv_expl_get(__u32 daddr, struct expl_entry *e_in)
{
	struct expl_entry *e;
	int res = 0;

	read_lock_bh(&expl_lock);
	e = __kaodv_expl_check(daddr);

	if(e)
	{
		res = 1;
		if(e_in)
			memcpy(e_in, e, sizeof(struct expl_entry));
	}

	read_unlock_bh(&expl_lock);
	return res;
}

int kaodv_expl_add(__u32 daddr, __u32 nhop, unsigned long time, unsigned short flags, int ifindex)
{
	struct expl_entry *e;
	int status = 0;

	if(kaodv_expl_get(daddr, NULL))
	{
		return 0;
	}

	e = kmalloc(sizeof(struct expl_entry), GFP_ATOMIC);

	if(e == NULL)
	{
		printk(KERN_ERR "expl: kmalloc failed!\n");
		return -ENOMEM;
	}

	e->daddr = daddr;
	e->nhop = nhop;
	e->flags = flags;
	e->ifindex = ifindex;
	e->expires = jiffies + (time * HZ) / 1000;//HZ in mt7620a = 100, 3s
	
	write_lock_bh(&expl_lock);

	status = __kaodv_expl_add(e);

	if(status)
		expl_len++;

#ifdef EXPL_TIMER
	if(status && list_is_first(e))
	{
		if(timer_pending(&expl_timer))
			mod_timer(&expl_timer, e->expires);
		else
		{
			expl_timer.function = kaodv_expl_timeout;
			expl_timer.expires = e->expires;
			expl_timer.data = 0;
			add_timer(&expl_timer);
		}
	}
#endif

	write_unlock_bh(&expl_lock);

	if(status < 0)
		kfree(e);

	return status;
}

static void *expl_seq_start(struct seq_file *s, loff_t *pos)
{
	/*if(*pos == 0)
	{
		seq_printf(s, "# %-15s %-15s %-5s %-5s Expires\n", "Addr", "Nhop", "Flags", "Iface");
		return expl_head.next; 
	}
	else
		return NULL;*/
	if(list_empty(&expl_head))
		printk("No data in expl_list!\n");
	
	if(*pos == 0)
		seq_printf(s, "# %-15s %-15s %-5s %-5s Expires\n", "Addr", "Nhop", "Flags", "Iface");

	return seq_list_start(&expl_head, *pos);
}

static void *expl_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	/*struct list_head *p = (struct list_head *)v;
	
	if((*pos != 0) && (p->next == &expl_head))
	{
		return NULL;
	}
	
	(*pos)++;

	return p->next;*/
	return seq_list_next(v, &expl_head, pos);
}

static void expl_seq_stop(struct seq_file *s, void *v)
{

}

static int expl_seq_show(struct seq_file *s, void *v)
{
	//struct expl_entry *e = (struct expl_entry *)v;
	struct expl_entry *e = list_entry(v, struct expl_entry, l);
	char addr[16], nhop[16], flags[4];
	struct net_device *dev;
	int num_flags = 0;

	dev = dev_get_by_index(&init_net, e->ifindex);

	if(!dev)
		return 0;

	sprintf(addr, "%d.%d.%d.%d", 
			0x0ff & e->daddr, 
			0x0ff & (e->daddr >> 8),
			0x0ff & (e->daddr >> 16),
			0x0ff & (e->daddr >> 24));

	sprintf(nhop, "%d.%d.%d.%d", 
			0x0ff & e->nhop, 
			0x0ff & (e->nhop >> 8),
			0x0ff & (e->nhop >> 16),
			0x0ff & (e->nhop >> 24));

	if(e->flags & KAODV_RT_GW_ENCAP)
		flags[num_flags++] = 'E';
	if(e->flags & KAODV_RT_REPAIR)
		flags[num_flags++] = 'R';

	flags[num_flags] = '\0';

	seq_printf(s, "  %-15s %-15s %-5s %-5s %lu\n", addr, nhop, flags, dev->name, (e->expires - jiffies) * 1000 / HZ);

	dev_put(dev);

	return 0;
}

static struct seq_operations expl_seq_ops = 
{
	.start = expl_seq_start,
	.next = expl_seq_next,
	.stop = expl_seq_stop,
	.show = expl_seq_show
};

static int expl_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &expl_seq_ops);
}

static struct file_operations expl_proc_ops = 
{
	.owner = THIS_MODULE,
	.open = expl_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};
/*
static int kaodv_expl_print(char *buf)
{
	struct list_head *pos;
	int len = 0;

	read_lock_bh(&expl_lock);

	len += sprintf(buf, "# Total entries: %u\n", expl_len);
	len += sprintf(buf + len, "# %-15s %-15s %-5s %-5s Expires\n", "Addr", "Nhop", "Flags", "Iface");

	list_for_each(pos, &expl_head)
	{
		char addr[16], nhop[16], flags[4];
		struct net_device *dev;
		int num_flags = 0;
		struct expl_entry *e = (struct expl_entry *)pos;

		dev = dev_get_by_index(&init_net, e->ifindex);

		if(!dev)
			continue;

		sprintf(addr, "%d.%d.%d.%d", 
				0x0ff & e->daddr, 
				0x0ff & (e->daddr >> 8),
				0x0ff & (e->daddr >> 16),
				0x0ff & (e->daddr >> 24));

		sprintf(nhop, "%d.%d.%d.%d", 
				0x0ff & e->nhop, 
				0x0ff & (e->nhop >> 8),
				0x0ff & (e->nhop >> 16),
				0x0ff & (e->nhop >> 24));

		if(e->flags & KAODV_RT_GW_ENCAP)
			flags[num_flags++] = 'E';
		if(e->flags & KAODV_RT_REPAIR)
			flags[num_flags++] = 'R';

		flags[num_flags] = '\0';

		len += sprintf(buf + len, "  %-15s %-15s %-5s %-5s %lu\n", addr, nhop, flags, dev->name, (e->expires - jiffies) * 1000 / HZ);

		dev_put(dev);
	}

	read_unlock_bh(&expl_lock);

	return len;
}

static int kaodv_expl_proc_info(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	int len;

	len = kaodv_expl_print(page);

	*start = page + offset;
	len -= offset;
	if(len > count)
		len = count;
	else if(len < 0)
		len = 0;

	return len;
}
*/
int kaodv_expl_update(__u32 daddr, __u32 nhop, unsigned long time, unsigned short flags, int ifindex)
{
	//int ret = 0;
	struct expl_entry *e;

	write_lock_bh(&expl_lock);

	e = __kaodv_expl_check(daddr);

	if(e == NULL)
	{
		printk("No route to update!\n");
		write_unlock_bh(&expl_lock);
		return -1;
	}

	e->nhop = nhop;
	e->flags = flags;
	e->ifindex = ifindex;
	e->expires = jiffies + (time *HZ) / 1000;

	list_del(&e->l);
	__kaodv_expl_add(e);//re-add to sort

#ifdef EXPL_TIMER	
	__kaodv_expl_set_next_timeout();
#endif

	write_unlock_bh(&expl_lock);

	return 0;
}

void kaodv_expl_flush(void)
{
#ifdef EXPL_TIMER
	if(timer_pending(&expl_timer))
		del_timer(&expl_timer);
#endif

	write_lock_bh(&expl_lock);

	__kaodv_expl_flush();
	
	write_unlock_bh(&expl_lock);
}

void kaodv_expl_init(void)
{
	expl_proc = proc_create(KAODV_EXPL_PROC_FS_NAME, 0, init_net.proc_net, &expl_proc_ops);

	if(!expl_proc)
	{
		printk(KERN_ERR "kaodv_expl: failed to create proc entry\n");
	}

	expl_len = 0;
#ifdef EXPL_TIMER
	init_timer(&expl_timer);
#endif
}

void kaodv_expl_finish(void)
{
	kaodv_expl_flush();

	proc_remove(expl_proc);
}
