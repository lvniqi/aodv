CC = mipsel-linux-gcc
CCFLAGS = -Wstrict-prototypes
INCLUDES =
LIBS = 
SRCS = $(shell echo *.c)
OBJS = $(SRCS:.c = .o)
TARGET = aodv

$(TARGET) : $(OBJS)
	$(CC) $^ -o $@ $(INCLUDES) $(LIBS)

%.o : %.c
	$(CC) -c $< $(CCFLAGS)

aodv_rreq.o: aodv_rreq.c aodv_rreq.h aodv_socket.h defs.h parameters.h
aodv_socket.o: aodv_socket.c aodv_socket.h defs.h aodv_rreq.h
main.o: main.c defs.h

clean:
	rm -f *.o $(TARGET)


