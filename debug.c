#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <net/if.h>

#include "debug.h"
#include "aodv_rreq.h"
#include "aodv_rrep.h"
#include "aodv_rerr.h"
#include "parameters.h"
#include "timer_queue.h"
#include "routing_table.h"
#include "aodv_timeout.h"

extern s32_t log_to_file, rt_log_interval;
extern s8_t *progname;
s32_t log_file_fd = -1;
s32_t log_rt_fd = -1;
s32_t log_nmsgs = 0;
s32_t debug = 0;
struct timer rt_log_timer;
func_struct_t timeout_func[10] = 
{
	{"rreq_record_timeout", rreq_record_timeout},
	{"rreq_blacklist_timeout", rreq_blacklist_timeout},
	{"route_discovery_timeout", route_discovery_timeout},
	{"local_repair_timeout", local_repair_timeout},
	{"route_expire_timeout", route_expire_timeout},
	{"route_delete_timeout", route_delete_timeout},
	{"hello_timeout", hello_timeout},
	{"rrep_ack_timeout", rrep_ack_timeout},
	{"wait_on_reboot_timeout", wait_on_reboot_timeout},
	{"print_rt_table", print_rt_table}
};

void log_init(void)
{
	if(log_to_file)
	{
		if((log_file_fd = open(AODV_LOG_PATH, O_RDWR | O_CREAT | O_TRUNC, S_IROTH | S_IWUSR | S_IRUSR | S_IRGRP)) < 0)
		{
			fprintf(stderr, "Open log file failed!");
			exit(-1);
		}
	}

	if(rt_log_interval)
	{
		if((log_rt_fd = open(AODV_RT_LOG_PATH, O_RDWR | O_CREAT | O_TRUNC, S_IROTH | S_IWUSR | S_IRUSR | S_IRGRP)) < 0)
		{
			fprintf(stderr, "Open rt log file failed!");
			exit(-1);
		}
	}

	openlog(progname, 0, LOG_DAEMON);
}

void log_rt_table_init(void)
{
	timer_init(&rt_log_timer, print_rt_table, NULL);
	timer_set_timeout(&rt_log_timer, rt_log_interval);
}

void log_cleanup(void)
{
	if(log_to_file && log_file_fd)
	{
		if(close(log_file_fd) < 0)
			fprintf(stderr, "Coule not close log_file_fd!\n");
		if(remove(AODV_LOG_PATH) < 0)
			fprintf(stderr, "Coule not remove log_file!\n");
	}

	if(rt_log_interval && log_rt_fd)
	{
		if(close(log_rt_fd) < 0)
			fprintf(stderr, "Could not close log_rt_fd!\n");
		if(remove(AODV_RT_LOG_PATH) < 0)
			fprintf(stderr, "Coule not remove rtlog_file!\n");
	}
}

void write_to_log_file(s8_t *msg, s32_t len)
{
	if(!log_file_fd)
	{
		fprintf(stderr, "Could not write to log file\n");
		return;
	}
	if(len <= 0)
	{
		fprintf(stderr, "len=0\n");
		return;
	}
	if(write(log_file_fd, msg, len) < 0)
	{
		fprintf(stderr, "Could not write to log file\n");
		return;
	}
}

s8_t *packet_type(u32_t type)
{
	static s8_t temp[50];

	switch(type)
	{
		case AODV_RREQ:
			return "AODV_RREQ";
		case AODV_RREP:
			return "AODV_RREP";
		case AODV_RERR:
			return "AODV_RERR";
		default:
			sprintf(temp, "Unknown packet type %d", type);
			return temp;
	}
}

void alog(s32_t type, s32_t errnum, const s8_t *function, s8_t *format, ...)
{
	va_list ap;
	static s8_t buffer[256] = "";
	static s8_t log_buf[1024];
	s8_t *msg;
	struct timeval now;
	struct tm *time;
	s32_t len = 0;

	va_start(ap, format);

	if(type == LOG_WARNING)
		msg = &buffer[9];
	else
		msg = buffer;

	vsprintf(msg, format, ap);
	va_end(ap);

	if(!debug && !log_to_file)
		goto syslog;

	gettimeofday(&now, NULL);

	time = localtime(&now.tv_sec);

	len += sprintf(log_buf + len, "[%02d:%02d:%02d.%03ld] %s: %s", time->tm_hour, time->tm_min, time->tm_sec, now.tv_usec / 1000, function, msg);
	if(errnum == 0)
		len += sprintf(log_buf + len, "\n");
	else
		len += sprintf(log_buf + len, ": %s\n", strerror(errnum));

	if(len > 1024)
	{
		fprintf(stderr, "alog(): buffer data size to limit! len =%d\n", len);
		goto syslog;
	}

	if(log_to_file)
	{
		write_to_log_file(log_buf, len);
	}

syslog:
	if(type <= LOG_NOTICE)
	{
		if(errnum != 0)
		{
			errno = errnum;
			syslog(type, "%s: %s: %m", function, msg);
		}
		else
		{
			syslog(type, "%s: %s", function, msg);
		}
	}

	if(type <= LOG_ERR)
		exit(-1);
}

s8_t *rreq_flags_to_str(RREQ *rreq)
{
	static s8_t buf[5];
	s32_t len = 0;
	s8_t *str;

	if(rreq->j)
		buf[len++] = 'J';
	if(rreq->r)
		buf[len++] = 'R';
	if(rreq->g)
		buf[len++] = 'G';
	if(rreq->d)
		buf[len++] = 'D';

	buf[len] = '\0';

	str = buf;

	return str;
}

s8_t *rrep_flags_to_str(RREP *rrep)
{
	static s8_t buf[3];
	s32_t len = 0;
	s8_t *str;

	if(rrep->r)
		buf[len++] = 'R';
	if(rrep->a)
		buf[len++] = 'A';

	buf[len] = '\0';

	str = buf;

	return str;
}

void log_pkt_fields(AODV_msg *msg)
{
	RREQ *rreq;
	RREP *rrep;
	RERR *rerr;
	struct in_addr dest, orig;

	switch(msg->type)
	{
		case AODV_RREQ:
			rreq = (RREQ *)msg;
			dest.s_addr = rreq->dest_addr;
			orig.s_addr = rreq->orig_addr;
			DEBUG(LOG_DEBUG, 0, "rreq->flags:%s rreq->hopcount=%d rreq->rreq_id=%d", rreq_flags_to_str(rreq), rreq->hopcnt, ntohl(rreq->rreq_id));
			DEBUG(LOG_DEBUG, 0, "rreq->dest_addr:%s rreq->dest_seqno=%d", ip_to_str(dest), ntohl(rreq->dest_seqno));
			DEBUG(LOG_DEBUG, 0, "rreq->orig_addr:%s rreq->orig_seqno=%d", ip_to_str(orig), ntohl(rreq->orig_seqno));
			break;

		case AODV_RREP:
			rrep = (RREP *)msg;
			dest.s_addr = rrep->dest_addr;
			orig.s_addr = rrep->orig_addr;
			DEBUG(LOG_DEBUG, 0, "rrep->flags:%s rrep->hopcount=%d", rrep_flags_to_str(rrep), rrep->hopcnt);
			DEBUG(LOG_DEBUG, 0, "rrep->dest_addr:%s rrep->dest_seqno=%d", ip_to_str(dest), ntohl(rrep->dest_seqno));
			DEBUG(LOG_DEBUG, 0, "rrep->orig_addr:%s rrep->lifetime=%d", ip_to_str(orig), ntohl(rrep->lifetime));
			break;
			
		case AODV_RERR:
			rerr = (RERR *)msg;
			DEBUG(LOG_DEBUG, 0, "rerr->dest_count:%d rerr->flags=%s", rerr->dest_count, rerr->n ? "N" : "-");
			break;
	}
}

s8_t *rt_flags_to_str(u_int16_t flags)
{
	static s8_t buf[5];
	s32_t len = 0;
	s8_t *str;

	if(flags & RT_UNIDIR)
		buf[len++] = 'U';
	if(flags & RT_REPAIR)
		buf[len++] = 'R';
	if(flags & RT_INET_DEST)
		buf[len++] = 'I';
	
	buf[len] = '\0';

	str = buf; 

	return str;

}

s8_t *state_to_str(u_int8_t state)
{
	if(state == VALID)
		return "VAL";
	else if(state == INVALID)
		return "INV";
	else
		return "?";
}

s8_t *devs_ip_to_str(void)
{
	return ip_to_str(this_host.dev.ipaddr);//only one now
}

void print_rt_table(void *arg)
{
	s8_t rt_buf[2048], seqno_str[11];
	s32_t len = 0;
	s32_t i = 0;
	struct timeval now;
	struct tm *time;
	ssize_t written;

	if(rt_tbl.num_entries == 0)
		goto schedule;

	gettimeofday(&now, NULL);

	time = localtime(&now.tv_sec);

	len += sprintf(rt_buf, "# Time: %02d:%02d:%02d.%03ld IP: %s seqno %u entries/active: %u/%u\n", time->tm_hour, time->tm_min, time->tm_sec, now.tv_usec / 1000, devs_ip_to_str(), this_host.seqno, rt_tbl.num_entries, rt_tbl.num_active);

	len += sprintf(rt_buf + len, "%-15s %-15s %-3s %-3s %-5s %-6s %-5s %-5s %-15s\n", "Destination", "Next hop", "HC", "St.", "Seqno", "Expire", "Flags", "Iface", "Precursors");

	written = write(log_rt_fd, rt_buf, len);

	len = 0;

	for(i = 0; i < RT_TABLESIZE; i++)
	{
		list_t *pos;
		list_for_each(pos, &rt_tbl.tbl[i])
		{
			rt_table_t *rt = (rt_table_t *)pos;

			if(rt->dest_seqno == 0)
				sprintf(seqno_str, "-");
			else
				sprintf(seqno_str, "%u", rt->dest_seqno);

			if(list_is_empty(&rt->precursors))
			{
				len += sprintf(rt_buf + len, "%-15s %-15s %-3d %-3s %-5s %-6lu %-5s %-5s\n", ip_to_str(rt->dest_addr), ip_to_str(rt->next_hop), rt->hopcnt, state_to_str(rt->state), seqno_str, (rt->hopcnt == 255) ? 0 : timeval_diff(&rt->rt_timer.timeout, &now), rt_flags_to_str(rt->flags), this_host.dev.ifname);
			}
			else
			{
				list_t *pos2;
				len += sprintf(rt_buf + len, "%-15s %-15s %-3d %-3s %-5s %-6lu %-5s %-5s %-15s\n", ip_to_str(rt->dest_addr), ip_to_str(rt->next_hop), rt->hopcnt, state_to_str(rt->state), seqno_str, (rt->hopcnt == 255) ? 0 : timeval_diff(&rt->rt_timer.timeout, &now), rt_flags_to_str(rt->flags), this_host.dev.ifname, ip_to_str(((precursor_t *)rt->precursors.next)->neighbor));

				list_for_each(pos2, &rt->precursors)
				{
					precursor_t *pr = (precursor_t *)pos2;

					if(pos2->prev == &rt->precursors)
						continue;

					len += sprintf(rt_buf + len, "%64s %-15s\n", " ", ip_to_str(pr->neighbor));

					written = write(log_rt_fd, rt_buf, len);

					len = 0;
				}
			}

			if(len > 0)
			{
				written = write(log_rt_fd, rt_buf, len);
				len = 0;
			}
		}
	}

schedule:
	timer_set_timeout(&rt_log_timer, rt_log_interval);
}

s8_t *ip_to_str(struct in_addr addr)
{
	static s8_t buf[16 * 4];
	static s32_t index = 0;
	s8_t *str;

	strcpy(&buf[index], inet_ntoa(addr));
	str = &buf[index];
	index += 16;
	index %= 64;

	return str;
}

s8_t *ptr_to_func_name(void (*action)(void *))
{
	static s8_t buf[32];
	s8_t *str;
	s32_t i;
	
	memset(buf, '\0', sizeof(buf));

	for(i = 0; i < 10; i++)
	{
		if(action == timeout_func[i].action)
		{
			strcpy(buf, timeout_func[i].func_name);
			str = buf;
			return str;
		}
	}

	strcpy(buf, "Unkown function");
	str = buf;
	return str;
}
