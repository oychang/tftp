CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -O2
VPATH = src/server


default: server
server: server.c server.h
	$(CC) $(CFLAGS) -c $< -o bin/$@


.PHONY: clean test
clean:
	rm -f *.o
test:
	echo 'to be implemented'
