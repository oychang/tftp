#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// available on range [1024, 65535]
#define LISTEN_PORT 3333
#define MAX_BUFFER_LEN 100

enum {RRQ, WRQ, DATA, ACK, ERROR} tftp_op;

#endif
