CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -O2

VPATH = src/server

server: server.c server.h

clean:
	rm *.o
