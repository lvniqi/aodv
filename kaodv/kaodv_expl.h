#ifndef _KAODV_EXPL_H_
#define _KAODV_EXPL_H_

#ifdef __KERNEL__

#include <linux/list.h>

struct expl_entry
{
	struct list_head l;
	unsigned long expires;
	unsigned short flags;
	__u32 daddr;
	__u32 nhop;
	int ifindex;
};

void kaodv_expl_init(void);
void kaodv_expl_flush(void);
int kaodv_expl_get(__u32 daddr, struct expl_entry *e_in);
int	kaodv_expl_add(__u32 daddr, __32 nhop, unsigned long time, unsigned short flags, int ifindex);
int kaodv_expl_update(__u32 daddr, __u32 nhop, unsigned long time, unsigned short flags, int ifindex);
int kaodv_expl_del(__u32 daddr);
void kaodv_expl_fini(void);

#endif

#endif
