#ifndef CLIENT_H
#define CLIENT_H

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
#include "../Logging.h"

#define MYPORT 3335
#define MAXBUFLEN 516
#define MAXDATALEN 512
#define MAXMODELEN 8
#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DAT 3
#define OPCODE_ACK 4

int tftp_client(int, int, char *, char *);

#endif
