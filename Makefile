SHELL=/bin/sh

CC = cc
#CFLAGS = -g -Wall
CFLAGS = -O3
LIBS =

SRCS = main.c \
	cpnet11.c \
	cpnet12.c \
	cpmutl.c \
	netio.c \
	sio.c \
	inifile.c

OBJS = $(SRCS:.c=.o)

PROG = cpnet

.SUFFIX: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $<

${PROG}: ${OBJS}
	$(CC) -g -o $@ ${OBJS} $(LIBS)

install: 

clean:
	rm -f ${OBJS} $(PROG) *~ core *.bak
