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
#include "Types.h"
//=============================================================================
// if  0: will stay open for one request until termination
// if ~0: will loop forever until sigkill -- daemon behavior
#define LOOP_FOREVER 1
// The amount of time a socket will block without data until it gives up
#define TIMEOUT_SEC 10
// Check if everyone can read (these are defined in sys/stat.h)
#define UGOR (S_IRUSR | S_IRGRP | S_IROTH)
//=============================================================================
// ===== Main server portion
// port: the listen port for initial requests to go to
int tftp_server(const int port);

// ===== Network interaction functions
// Utility function that does the socket -> options -> sockaddr setup -> bind
// processes all in one step, returning the new socket file descriptor.
// port: what port to bind to (0 => ephemeral port) & sockaddr for data
int get_bound_sockfd(const int port, struct sockaddr_in * sin);
// Returns a new, ready-to-use socket file descriptor for UDP (socket() step)
int get_udp_sockfd(void);
// Sets various operations on the socket with `setsockopt`.
// Sets port reuse, send/receive timeouts to macro TIMEOUT_SEC.
void set_socket_options(int sockfd);
// Sets up a sockaddr_in to listen on the port given with IP and to any
// network interface the device has with INADDR_ANY.
void setup_my_sin(struct sockaddr_in * sin, int port);

// Wrapper around recvfrom() setup and logging. Blocks until data or timeout.
// Listens for up to MAX_BUFFER_LEN - 1 bytes of data.
void packet_listener(int sockfd, session_t * session,
    struct sockaddr_in * fromaddr);

// Sends the TFTP data held within session's sendbuf to fromaddr with my
// sockfd. Wrapper around sendto() and accompanying logging.
void send_packet(int sockfd, struct sockaddr_in * fromaddr,
 session_t * session);

// ===== Parsing functions
// These functions interact with the received data and act to build up
// the session struct by looking at what response needs to be sent
// and creating it in the sendbuf.

// Parse received packet and prepare response (if applicable).
// These will return with the type of action that the server should take
// based on the contents of the received packet. In addition,
// the sendbuf will be filled with all applicable return data on return.
enum response_action parse_packet(session_t * session);
enum response_action parse_request_packet(session_t * session, int is_read);
enum response_action parse_data_packet(session_t * session);
enum response_action parse_ack_packet(session_t * session);
enum response_action parse_error_packet(session_t * session);

// These all build up their respective packets, setting sendbuf & sendbytes.
// prepare_error_packet() expects an errcode 0--7 and a valid C string.
void prepare_error_packet(session_t * session, char errcode, char * errmsg);
void prepare_ack_packet(session_t * session);
void prepare_data_packet(session_t * session);

// Resets the contents of the session struct (mostly...does not change bufs).
// Make sure initialized to something or else fclose will cause a segfault.
void reset_session(session_t * session);
//=============================================================================
#endif
