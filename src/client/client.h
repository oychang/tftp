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

#define MYPORT 3333 // Change to 3335 for real implementation
#define MAXBUFLEN 516
#define MAXDATALEN 512
#define MAXMODELEN 8
#define OPCODE_RRQ "01"
#define OPCODE_WRQ "02"
#define OPCODE_DAT "03"
#define OPCODE_ACK "04"

#endif
