#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>

#include <kaodv_expl.h>
#include <kaodv_netlink.h>
#include <kaodv_queue.h>

#define EXPL_MAX_LEN 1024

struct unsigned int expl_len;
static rwlock_t expl_lock = RW_LOCK_UNLOCKED;
static LIST_HEAD(expl_head);

#define list_is_first(e) (&e->l == expl_head.next)


