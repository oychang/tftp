#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <unistd.h>
//=============================================================================
// Practically, the buffer should only be able to hold 2+2+512 = 516 bytes
// since that is the maximum size of the largest packet (a full DATA).
#define MAX_BUFFER_LEN 540
#define MAX_STRING_LEN 128
//-----------------------------------------------------------------------------
typedef char buffer[MAX_BUFFER_LEN];
typedef char string[MAX_STRING_LEN];
//-----------------------------------------------------------------------------
enum opcode {RRQ = 1, WRQ = 2, DATA = 3, ACK = 4, ERROR = 5};
enum response_action {SEND, SEND_RESET, RESET, NOOP};
enum session_action {IDLE, WRITE, READ};
typedef struct {
    enum session_action status;
    unsigned int        block_n;
    string              fn;
    FILE *              file;
    unsigned int        client_tid;

    ssize_t             recvbytes;
    buffer              recvbuf;
    ssize_t             sendbytes;
    buffer              sendbuf;
} session_t;
//=============================================================================
#endif
