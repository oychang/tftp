CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -O2

VPATH = src/server

tftp: server.o
server.o: server.c server.h

clean:
	rm *.o
