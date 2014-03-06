#ifndef SERVER_H
#define SERVER_H
//=============================================================================
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
//=============================================================================
#define TIMEOUT_SEC 10
#define MAX_BUFFER_LEN 540
#define MAX_STRING_LEN 128
typedef char buffer[MAX_BUFFER_LEN];
typedef char string[MAX_STRING_LEN];
//=============================================================================
enum opcode {RRQ = 1, WRQ = 2, DATA = 3, ACK = 4, ERROR = 5};
union packet {
    struct {
        char opcode[2];
        string filename;
        string mode;
    } rq;
    struct {
        char opcode[2];
        char block_number[2];
        char data[512];
    } data;
    struct {
        char opcode[2];
        char block_number[2];
    } ack;
    struct {
        char opcode[2];
        char errorcode[2];
        string errmsg;
    } error;
};
struct tftp_session {
    enum {IDLE, RECV, SEND} status;
    string                  fn;
    int                     tid[2];
    size_t                  block_n;
    union packet            recvpkt;
    union packet            sendpkt;
};
//=============================================================================
// Main jumping off point for operation.
// port: port to listen on for read/write requests
// is_verbose: true/false flag that indicates whether or not to log
int tftp_server(int port, int is_verbose);
int get_udp_sockfd(void);
void set_socket_options(int sockfd);
void setup_my_sin(struct sockaddr_in * sin, int port);
ssize_t packet_listener(int sockfd, buffer buf, struct sockaddr_in * fromaddr);
//=============================================================================
#endif
