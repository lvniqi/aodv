#ifndef _KAODV_QUEUE_H_
#define _KAODV_QUEUE_H_

#define KAODV_QUEUE_DROP 1
#define KAODV_QUEUE_SEND 2

int kaodv_queue_check(__u32 daddr);
int kaodv_queue_enqueue_packet(struct sk_buff *skb, int(*okfn)(struct sk_buff *));
int kaodv_queue_set_verdict(int verdict, __u32 daddr);
void kaodv_queue_flush(void);
int kaodv_queue_init(void);
void kaodv_queue_finish(void);

#endif
