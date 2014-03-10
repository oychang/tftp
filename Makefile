CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -O2 -Werror -std=gnu99
VPATH = src src/server src/client


tftp: server.o client.o tftp.c tftp.h Boolean.h Logging.h
server.o: server.c server.h Boolean.h Logging.h Types.h
client.o: client.c client.h Logging.h


.PHONY: clean test
clean:
	rm -f *.o
test: tftp
	sh test.sh
