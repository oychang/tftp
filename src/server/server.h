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
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../Boolean.h"
#include "../Logging.h"
//=============================================================================
#define TIMEOUT_SEC 10
#define MAX_BUFFER_LEN 540
#define MAX_STRING_LEN 128
// Masks for checking if all read/write flags are set
#define UGOR S_IRUSR | S_IRGRP | S_IROTH
// #define UGOW S_IWUSR | S_IWGRP | S_IWOTH
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
struct session_t {
    enum {IDLE, RECV, SEND} status;
    string                  fn;
    int                     tid[2];
    size_t                  block_n;
    FILE *                  file;

    ssize_t                 recvbytes;
    ssize_t                 sendbytes;
    // union packet            recvpkt;
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
// Assumes that packets are from the right host.
// Parses out packets and puts response packet into session, ready for
// sending.
// Return Values:
//    -1: do not send anything
//     0: send ready ack, reset connection information (i.e. transfer complete)
// error: send ready error, reset connection information
//  else: enum opcode for type of packet to create and send
int parse_packet(buffer buf, struct session_t * session);
// Builds an ERROR packet into the packet given.
// Assume errmsg is zero-terminated
void prepare_error_packet(union packet * pkt, char errcode, char * errmsg);
void prepare_data_packet(struct session_t * session);
void prepare_ack_packet(union packet * pkt, size_t block_n);
//=============================================================================
#endif
