#include "server.h"
//=============================================================================
int
tftp_server(const int port)
{
    // Setup listening on our pseudo well-known port.
    int                current_sockfd = get_bound_sockfd(port);
    struct sockaddr_in client_addr;
    // Hold information about block numbers, buffers, and byte counts.
    session_t          session = {IDLE, 0, "", NULL, 0, {}, 0, {}};

    // In this loop, we have to make sure that TIDs get sorted out.
    // Cases:
    // (1) if session is idle, listen on our "well-known port" (default
    // & occurs on reset of session)
    // (2) if session is active, listen on the ephemeral session port for
    // a singular TID (i.e. only respond to things on one port, ostensibly
    // from the same client)
    enum response_action act = SEND;
    // If LOOP_FOREVER set to true in header file, will continue indefinitely.
    while (act == SEND || act == NOOP || LOOP_FOREVER) {
        packet_listener(current_sockfd, &session, &client_addr);
        // Check for proper TID (i.e. that port matches up to what we expect)
        if (session.status == IDLE && session.recvbytes > 0) {
            log("initial connection...assigning ports\n");
            log("client sends packets from %d\n",
                ntohs(client_addr.sin_port));
            close(current_sockfd);
            current_sockfd = get_bound_sockfd(0);
        }

        // Parse packet and prepare response
        switch ((act = parse_packet(&session))) {
        case SEND:
            send_packet(current_sockfd, &client_addr, &session);
            break;
        case SEND_RESET:
            send_packet(current_sockfd, &client_addr, &session);
            reset_session(&session);
            close(current_sockfd);
            current_sockfd = get_bound_sockfd(port);
            break;
        case RESET:
            reset_session(&session);
            close(current_sockfd);
            current_sockfd = get_bound_sockfd(port);
            break;
        case NOOP: default:
            break;
        }
    }

    return EXIT_SUCCESS;
}
//=============================================================================
enum response_action
parse_packet(session_t * session)
{
    // Do a basic check for opcode validity
    if (session->recvbytes == -1) {
        if (session->status != IDLE) {
            // TODO: retransmission limit
            log("no response...retransmitting\n");
            return SEND;
        } else {
            log("no signal & idle...ignoring\n");
            return NOOP;
        }
    } else if (session->recvbuf[0] != 0
        || session->recvbuf[1] < 1
        || session->recvbuf[1] > 5) {
        log("got invalid opcode.\n");
        prepare_error_packet(session, 0, "invalid opcode");
        return SEND_RESET;
    }

    log("found opcode as 0%d\n", session->recvbuf[1]);
    // These enum values are set to their opcodes, so we can switch
    // on either one--this is easier to read.
    switch (session->recvbuf[1]) {
    case RRQ:   return parse_request_packet(session, true);
    case WRQ:   return parse_request_packet(session, false);
    case DATA:  return parse_data_packet(session);
    case ACK:   return parse_ack_packet(session);
    case ERROR: return parse_error_packet(session);
    default:    return NOOP;
    }
}
//-----------------------------------------------------------------------------
enum response_action
parse_request_packet(session_t * session, int is_read)
{
    // Check that the packet is a good size
    // Opcode + 1 Char filename + \0 + 1 Char mode + \0
    static const int min_req_length = 2 + 1 + 1 + 1 + 1;
    if (session->status != IDLE) {
        prepare_error_packet(session, 5, "only one transfer at a time");
        return SEND;
    } else if (session->recvbytes < min_req_length) {
        prepare_error_packet(session, 0, "request too short");
        return SEND_RESET;
    }

    // Set status
    session->status = (is_read ? READ : WRITE);
    log("parsing %s packet\n", is_read ? "RRQ" : "WRQ");

    // Set filename field
    strncpy(session->fn, &session->recvbuf[2], MAX_STRING_LEN);
    session->fn[MAX_STRING_LEN-1] = '\0'; // ensure null-terminated
    log("got filename as %s\n", session->fn);

    // Check mode equal to octet.
    const size_t filename_len = strlen(session->fn); // get for offset
    static const char * octet = "octet";
    const size_t octet_len = strlen(octet);
    if (strncasecmp(&session->recvbuf[2+filename_len+1], octet, octet_len)) {
        prepare_error_packet(session, 0, "only octet supported");
        return SEND_RESET;
    }

    // Check filename is accessible
    // (1): only current directory allowed (no ../foo or /foo)
    // (2): only do if accessible to everyone
    // XXX: Assume that if the file exists, we'll be able to stat it
    struct stat statbuf;
    int statret = stat(session->fn, &statbuf);
    if (strstr(session->fn, "..") != NULL || session->fn[0] == '/') {
        prepare_error_packet(session, 2, "no up traversal");
        return SEND_RESET;
    } else if (statret == -1 && is_read) {
        log("could not stat file\n");
        prepare_error_packet(session, 2, "could not stat file");
        return SEND_RESET;
    } else if (!statret) {
        if (is_read && (statbuf.st_mode & UGOR) != UGOR) {
            prepare_error_packet(session, 2, "bad read permissions");
            return SEND_RESET;
        } else {
            log("warn: file exists on this machine; overwrite possible\n");
        }
    }

    // So we can access the file with a good filename at this point.
    log("opening file '%s' mode '%s'\n", session->fn, is_read ? "r" : "w");
    session->file = fopen(session->fn, is_read ? "r" : "w");

    // Response depends on type of request
    if (is_read) {
        session->block_n = 1;
        prepare_data_packet(session);
        return (session->sendbytes < 516) ? SEND_RESET : SEND;
    } else {
        session->block_n = 0;
        prepare_ack_packet(session);
        return SEND;
    }
}
//-----------------------------------------------------------------------------
enum response_action
parse_data_packet(session_t * session)
{
    // Check that packet is a good size
    // Opcode (2B) + Block Number (2B) + n bytes (max 512)
    static const int min_data_len = 2 + 2 + 0; // can get 0 bytes
    static const int max_data_len = 2 + 2 + 512;
    if (session->status != WRITE) {
        prepare_error_packet(session, 4, "send WRQ before data");
        return SEND_RESET;
    } else if (session->recvbytes > max_data_len
        || session->recvbytes < min_data_len) {
        prepare_error_packet(session, 0, "bad data packet size");
        return SEND_RESET;
    }

    // Get block number
    log("parsing data packet\n");
    unsigned int block_number = (session->recvbuf[2]<<8) + session->recvbuf[3];
    log("got block number as %u\n", block_number);
    // Check for delayed duplicate packet. If we've already gotten this
    // data block, safely ignore it.
    if (block_number <= session->block_n) {
        log("Got block %u while expecting block %u\n",
            block_number, session->block_n);
        return NOOP;
    }

    // Otherwise, save the data to disk
    log("writing out %zd bytes to %s\n", session->recvbytes - 4, session->fn);
    fwrite(&session->recvbuf[4], sizeof(char),
        session->recvbytes - 4, session->file);

    session->block_n++;
    prepare_ack_packet(session);
    return (session->recvbytes < 516) ? SEND_RESET : SEND;
}
//-----------------------------------------------------------------------------
enum response_action
parse_ack_packet(session_t * session)
{
    log("parsing ack packet\n");

    // Get block number and check that it's what we're expecting.
    // If not, then it must be less than our number and gotten delayed.
    // So, we can feel confident ignoring it (and should to avoid
    // Sorcerer's Apprentice Syndrome).
    unsigned int block_number = (session->recvbuf[2]<<8) + session->recvbuf[3];
    log("got ack for block number %u\n", block_number);
    if (block_number != session->block_n) {
        log("got duplicate ack...ignoring\n");
        return NOOP;
    }

    session->block_n++;
    prepare_data_packet(session);
    return (session->sendbytes < 516) ? SEND_RESET : SEND;
}
//-----------------------------------------------------------------------------
enum response_action
parse_error_packet(session_t * session)
{
    log("parsing error packet\n");

    // Get error code & message
    unsigned int error_code = (session->recvbuf[2]<<8) + session->recvbuf[3];
    fprintf(stderr, "Could not transfer %s; got code %u, message: '%s'\n",
        session->fn, error_code, &session->recvbuf[4]);

    // Any error constitutes an immediate premature termination and
    // requires no response.
    return RESET;
}
//=============================================================================
void
prepare_error_packet(session_t * session, char errcode, char * errmsg)
{
    log("preparing error packet with code 0%d and message %s\n",
        errcode, errmsg);
    memcpy(session->sendbuf, (char [4]){
        0, ERROR,  // opcode
        0, errcode // error code
    }, 4*sizeof(char));
    // error message (assume errmsg is null-terminated)
    strcpy(&session->sendbuf[4], errmsg);
    session->sendbytes = 2 + 2 + strlen(errmsg) + 1;
    return;
}
//-----------------------------------------------------------------------------
void
prepare_ack_packet(session_t * session)
{
    // Separate out the high-order byte and the low-order byte
    // byte of what is assumed to be a 16-bit number
    char hob = session->block_n >> 8;
    char lob = session->block_n & 0xff;
    log("preparing ack packet for block# %u (high:%d low:%d)\n",
        session->block_n, hob, lob);
    memcpy(session->sendbuf, (char [4]){
        0, ACK,  // opcode
        hob, lob // block number
    }, 4*sizeof(char));
    session->sendbytes = 2 + 2;

    return;
}
//-----------------------------------------------------------------------------
void
prepare_data_packet(session_t * session)
{
    char hob = session->block_n >> 8;
    char lob = session->block_n & 0xff;
    log("preparing data packet for block# %u (high:%d low:%d)\n",
        session->block_n, hob, lob);
    memcpy(session->sendbuf, (char [4]){
        0, DATA, // opcode
        hob, lob // block number
    }, 4*sizeof(char));
    // Read data from disk
    int bytes_read = fread(&session->sendbuf[4],
        sizeof(char), 512, session->file);
    log("read %d byte(s)\n", bytes_read);
    session->sendbytes = 2 + 2 + bytes_read;

    return;
}
//=============================================================================
void
reset_session(session_t * session)
{
    log("resetting transfer\n");

    session->status = IDLE;
    session->block_n = 0;
    session->fn[0] = '\0';
    if (session->file != NULL) {
        fclose(session->file);
        session->file = NULL;
    }

    return;
}
//=============================================================================
int
get_bound_sockfd(const int port)
{
    int sockfd = get_udp_sockfd();
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;

    set_socket_options(sockfd);
    log("setting port to %d\n", port);

    struct sockaddr_in sin = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
        .sin_zero = {0,0,0,0,0,0,0,0} // 8 zeros
    };
    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    log("bind successful\n");
    return sockfd;
}
//-----------------------------------------------------------------------------
int
get_udp_sockfd(void)
{
    int sockfd;
    // use IP, UDP, and automatically get the UDP protocol number
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    log("successfully got udp sockfd %d\n", sockfd);
    return sockfd;
}
//-----------------------------------------------------------------------------
void
set_socket_options(int sockfd)
{
    static const int yes = 1;
    static const struct timeval timeout = {
        .tv_sec = TIMEOUT_SEC, .tv_usec = 0
    };

    log("timeout length is %d seconds\n", TIMEOUT_SEC);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
        == -1) {
        perror("setsockopt");
        log("continuing without port reuse\n");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
        (void*)&timeout, sizeof(struct timeval)) == -1) {
        perror("setsockopt");
        log("continuing without receive timeout\n");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
        (void*)&timeout, sizeof(struct timeval)) == -1) {
        perror("setsockopt");
        log("continuing without send timeout\n");
    }

    return;
}
//-----------------------------------------------------------------------------
void
packet_listener(int sockfd, session_t * session, struct sockaddr_in * fromaddr)
{
    // log("listening for new packet on sockfd %d\n", sockfd);
    // XXX: We discard and do not check this value
    socklen_t fromaddr_len = sizeof(struct sockaddr);
    session->recvbytes = recvfrom(sockfd, session->recvbuf,
        MAX_BUFFER_LEN, 0, (struct sockaddr *)fromaddr, &fromaddr_len);

    if (VERBOSE) {
        log("received %zd bytes\n", session->recvbytes);
        if (session->recvbytes == -1) {
            if (errno != EAGAIN)
                perror("recvfrom");
        } else
            log("packet originated from %s:%d\n",
                inet_ntoa(fromaddr->sin_addr), ntohs(fromaddr->sin_port));
    }

    return;
}
//-----------------------------------------------------------------------------
void
send_packet(int sockfd, struct sockaddr_in * fromaddr, session_t * session)
{
    ssize_t sent_bytes = sendto(sockfd, session->sendbuf, session->sendbytes,
        0, (struct sockaddr *)fromaddr, sizeof(struct sockaddr));

    if (VERBOSE) {
        log("=========================\nsend_packet: ");
        int i;
        for (i = 0; i < session->sendbytes; i++) {
            printf("%d|", session->sendbuf[i]);
        }
        printf("\n");

        log("wanted to send %zd bytes; actually sent %zd bytes\n",
            session->sendbytes, sent_bytes);
        log("sent to %s:%d\n",
            inet_ntoa(fromaddr->sin_addr), ntohs(fromaddr->sin_port));
        log("=========================\n");
    }

    return;
}
