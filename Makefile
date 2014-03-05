CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -O2
VPATH = src src/server


tftp: server.o tftp.c tftp.h Boolean.h Logging.h
server.o: server.c server.h Boolean.h Logging.h

.PHONY: clean test
clean:
	rm -f *.o
test:
	echo 'to be implemented'
