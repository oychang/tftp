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
#include "../Boolean.h"

#define MAXBUFLEN 516
#define MAXDATALEN 512
#define MAXMODELEN 8
#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DAT 3
#define OPCODE_ACK 4
#define OPCODE_ERR 5
#define TIMEOUT_SEC 10

#define GET_HOB(n) ((n) >> 8)
#define GET_LOB(n) ((n) & 0xff)

int tftp_client(int, int, char *, char *);

#endif
