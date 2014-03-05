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

#include "../Boolean.h"
#include "../Logging.h"

// available on range [1024, 65535]
#define MAX_BUFFER_LEN 128
#define MAX_STRING_LEN 128
typedef char tftp_packet[MAX_BUFFER_LEN];
typedef char string[MAX_STRING_LEN];

enum tftp_opcode {RRQ, WRQ, DATA, ACK, ERROR, INVALID};
enum error_codes {
    NOTDEFINED, FILENOUTFOUND,
    ACCESSVIOLATION, DISKFULL,
    ILLEGALOP, UNKNOWNTID,
    FILEALREADYEXISTS, NOSUCHUSER
};
struct tftp_session {
    enum {IDLE, RECV, SEND} status;
    string                  fn;
    int                     ports[2];
    size_t                  block_n;
};


// Main jumping off point for operation.
// port: port to listen on for read/write requests
// is_verbose: true/false flag that indicates whether or not to log
int tftp_server(int port, int is_verbose);
int get_udp_sockfd(void);
void set_reusable_ports(int sockfd);
void setup_my_sin(struct sockaddr_in * sin, int port);
// void listen_respond_loop(int sockfd, struct sockaddr_in * my_addr);

#endif
