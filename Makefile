CC = mipsel-linux-gcc
CCFLAGS = -Wall
TARGET = aodv_mt7620

INCLUDES = -I./kaodv/src
LIBS = 
SRCS = $(shell echo *.c)
OBJS = $(SRCS:.c = .o)

$(TARGET) : $(OBJS)
	@rm -f *.o $(TARGET)
	$(CC) $(CCFLAGS) $(INCLUDES) $(LIBS) $^ -o $@ 
	@cd ~/openWrt/trunk;\
	make V=s package/kaodv/compile;\
	rm -f ~/ftproot/kaodv.ko ~/ftproot/aodv_mt7620;\
	cp ~/repos/aodv/aodv_mt7620 ~/ftproot;\
	cp ~/openWrt/trunk/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/linux-ramips_mt7620/kaodv/kaodv.ko ~/ftproot/

aodv_hello.o: aodv_hello.c main.c defs.h aodv_hello.h timer_queue.h aodv_timeout.h aodv_rrep.h aodv_rreq.h routing_table.h aodv_socket.h parameters.h debug.h	
aodv_neighbor.o: aodv_neighbor.c main.c defs.h aodv_neighbor.h aodv_rerr.h aodv_hello.h aodv_socket.h routing_table.h parameters.h list.h debug.h
aodv_rerr.o: aodv_rerr.c defs.h aodv_rerr.h aodv_socket.h routing_table.h list.h debug.h
aodv_rrep.o: aodv_rrep.c main.c defs.h aodv_rrep.h aodv_socket.h routing_table.h timer_queue.h aodv_rerr.h parameters.h aodv_neighbor.h debug.h
aodv_rreq.o: aodv_rreq.c main.c defs.h aodv_rreq.h list.h aodv_socket.h routing_table.h timer_queue.h parameters.h aodv_timeout.h seek_list.h aodv_rrep.h debug.h
aodv_socket.o: aodv_socket.c main.c defs.h aodv_socket.h aodv_rreq.h timer_queue.h parameters.h aodv_rrep.h aodv_rerr.h aodv_hello.h aodv_neighbor.h aodv_timeout.h debug.h
aodv_timeout.o: aodv_timeout.c main.c defs.h aodv_timeout.h aodv_rreq.h list.h seek_list.h routing_table.h parameters.h aodv_rerr.h aodv_neighbor.h aodv_hello.h aodv_rrep.h aodv_socket.h timer_queue.h nl.h debug.h
debug.o: debug.c main.c defs.h debug.h aodv_rreq.h aodv_rrep.h aodv_rerr.h parameters.h timer_queue.h routing_table.h
list.o: list.c defs.h list.h
main.o: main.c defs.h timer_queue.h aodv_socket.h parameters.h aodv_timeout.h routing_table.h aodv_hello.h nl.h debug.h
nl.o: nl.c main.c defs.h nl.h kaodv_netlink.h aodv_rreq.h aodv_timeout.h routing_table.h aodv_hello.h parameters.h aodv_socket.h aodv_rerr.h debug.h
routing_table.o: routing_table.c main.c defs.h routing_table.h list.h timer_queue.h aodv_timeout.h parameters.h seek_list.h aodv_neighbor.h nl.h debug.h
seek_list.o: seek_list.c defs.h seek_list.h list.h timer_queue.h aodv_timeout.h debug.h
timer_queue.o: timer_queue.c defs.h timer_queue.h list.h debug.h

.PHONY:clean 
clean:
	-rm -f *.o $(TARGET)
