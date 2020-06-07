ROOTDIR		:= ../../../src
TARGETDIR	:= .
BINDIR		:= ../../../bin/linux/
TARGET		:= athrill_remote

WFLAGS		:= -g -Wall
GCC		:= gcc

IFLAGS		:= -I$(ROOTDIR)/inc
IFLAGS		+= -I$(ROOTDIR)/lib

VPATH		:= $(TARGETDIR)
VPATH		+= $(ROOTDIR)/lib/winsock_wrapper
VPATH		+= $(ROOTDIR)/lib/udp



CFLAGS		:= $(WFLAGS)
CFLAGS		+= $(IFLAGS)
CFLAGS		+= -DOS_LINUX

LFLAGS		:= -Wl,--allow-multiple-definition

LIBS		:= -lpthread

OBJS		:= main.o
OBJS		+= udp_comm.o


.SUFFIXES:	.c .o

all:	$(TARGET)

$(TARGET):	$(OBJS)
	$(GCC) -O3 $(LFLAGS) $(OBJS) -o $(TARGET)  $(LIBS)
	cp $(TARGET) $(BINDIR)/$(TARGET)

.c.o:	$<
	$(GCC) -O3 -c $(CFLAGS) $<

clean:
	rm -f $(OBJS) $(TARGET) $(BINDIR)/$(TARGET)
