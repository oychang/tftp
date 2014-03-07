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
#define TIMEOUT_SEC    10
#define MAX_BUFFER_LEN 540
#define MAX_STRING_LEN 128
// Masks for checking if all read/write flags are set
#define UGOR (S_IRUSR | S_IRGRP | S_IROTH)
// #define UGOW S_IWUSR | S_IWGRP | S_IWOTH
typedef char buffer[MAX_BUFFER_LEN];
typedef char string[MAX_STRING_LEN];
//=============================================================================
enum opcode {RRQ = 1, WRQ = 2, DATA = 3, ACK = 4, ERROR = 5};
// union packet {
//     struct {
//         char opcode[2];
//         string filename;
//         string mode;
//     } rq;
//     struct {
//         char opcode[2];
//         char block_number[2];
//         char data[512];
//     } data;
//     struct {
//         char opcode[2];
//         char block_number[2];
//     } ack;
//     struct {
//         char opcode[2];
//         char errorcode[2];
//         string errmsg;
//     } error;
// };
struct session_t {
    enum {IDLE, RECV, SEND} status;
    unsigned int            block_n;
    string                  fn;
    FILE *                  file;

    ssize_t                 recvbytes;
    buffer                  recvbuf;
    ssize_t                 sendbytes;
    buffer                  sendbuf;
};
//=============================================================================
// Main jumping off point for operation.
// port: port to listen on for read/write requests
// is_verbose: true/false flag that indicates whether or not to log
int tftp_server(const int port, const int is_verbose);

// Returns: new, ready-to-use socket file descriptor
int get_udp_sockfd(void);

// Sets various operations on the socket with `setsockopt`.
// Sets port reuse, send/receive timeouts to macro TIMEOUT_SEC.
void set_socket_options(int sockfd);

// Sets up a sockaddr_in
void setup_my_sin(struct sockaddr_in * sin, int port);

// Listens for up to MAX_BUFFER_LEN - 1 bytes of data.
// Returns -1 on failure to get any data (most likely because of a timeout).
ssize_t packet_listener(int sockfd, buffer buf, struct sockaddr_in * fromaddr);

// Parse packet and prepare response (if applicable).
// Return Values:
//    -1: do not send anything (either error or timeout) & do NOT reset
//     0: send ready ack, reset connection information (i.e. transfer complete)
// error: send ready error, reset connection information
//  else: enum opcode for type of packet to send
int parse_packet(struct session_t * session);
int parse_request_packet(struct session_t * session, int is_read);
int parse_data_packet(struct session_t * session);
int parse_ack_packet(struct session_t * session);
int parse_error_packet(struct session_t * session);
void reset_session(struct session_t * session);

// These all build up their respective packets, setting sendbuf & sendbytes.
// prepare_error_packet() expects an errcode 0--7 and a valid C string.
void prepare_error_packet(struct session_t * session,
    char errcode, char * errmsg);
void prepare_ack_packet(struct session_t * session);
void prepare_data_packet(struct session_t * session);
//=============================================================================
#endif
