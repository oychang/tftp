#include "server.h"

int
parse_request_packet(struct session_t * session, int is_read)
{
    // Check that the packet is a good size
    // Opcode + 1 Char filename + \0 + 1 Char mode + \0
    static const int min_req_length = 2 + 1 + 1 + 1 + 1;
    if (session->status != IDLE) {
        log("transfer in progress, ignoring request\n");
        return -1;
    } else if (session->recvbytes < min_req_length) {
        log("aborting: request packet too short (min length is %d)\n",
            min_req_length);
        prepare_error_packet(session, 0, "request too short");
        return ERROR;
    }

    // Set staus
    session->status = (is_read ? RRQ : WRQ);
    log("parsing %s packet\n", is_read ? "RRQ" : "WRQ");

    // Set filename field
    strncpy(session->fn, &session->recvbuf[2], MAX_STRING_LEN);
    session->fn[MAX_STRING_LEN-1] = '\0'; // ensure null-terminated
    log("got filename as %s\n", session->fn);

    // Check filename is accessible
    // (1): only current directory allowed (no ../foo or /foo)
    // (2): only do if accessible to everyone
    struct stat statbuf;
    if (strstr(session->fn, "..") != NULL) {
        log("terminating: found up traversal\n");
        prepare_error_packet(session, 2, "no up traversal");
        return ERROR;
    } else if (is_read) {
        if (!stat(session->fn, &statbuf)
            && (statbuf.st_mode & (UGOR)) != (UGOR)) {
            log("terminating: file not readable by all\n");
            prepare_error_packet(session, 2, "bad read permissions");
            return ERROR;
        }
    }

    // So we can access the file!
    // Now, open a file descriptor to it
    log("fopening file '%s' mode '%s'\n", session->fn, is_read ? "r" : "w");
    session->file = fopen(session->fn, is_read ? "r" : "w");

    // Check mode equal to octet
    const size_t filename_len = strlen(session->fn); // get for offset
    static const char * octet = "octet";
    const size_t octet_len = strlen(octet);
    if (strncasecmp(&session->recvbuf[2+filename_len+1], octet, octet_len)) {
        log("aborting: only octet mode supported\n");
        prepare_error_packet(session, 0, "only octet supported");
        return ERROR;
    }

    // Set initial block number
    if (is_read) {
        session->block_n = 1;
        prepare_data_packet(session);
        return DATA;
    }
    session->block_n = 0;
    prepare_ack_packet(session);
    return ACK;
}

int
parse_data_packet(struct session_t * session)
{
    // Check that packet is a good size
    // Opcode (2B) + Block Number (2B) + n bytes (max 512)
    static const int min_data_len = 2 + 2 + 0; // can get 0 bytes
    static const int max_data_len = 2 + 2 + 512;
    if (session->status != RECV) {
        log("no request or we are sender; can't write data packet");
        return -1;
    } else if (session->recvbytes > max_data_len
        || session->recvbytes < min_data_len) {
        log("aborting: data packet size is wrong\n");
        prepare_error_packet(session, 0, "bad data packet size");
        return ERROR;
    }

    // Set block number
    log("parsing data packet\n");
    unsigned int block_number = (session->recvbuf[2]<<8) + session->recvbuf[3];
    log("got block number as %u\n", block_number);
    // Check for Sorcerer's Apprentice Bug
    // That is, check if we've already sent an ACK for the block number
    // we just received and if so, ignore this packet.
    if (block_number <= session->block_n) {
        log("SAS detected\n");
        log("Got block# %u while expecting block# %u\n",
            block_number, session->block_n);
        return -1;
    } else
        session->block_n++;

    // Write data out to disk
    log("writing out %ld bytes to %s\n", session->recvbytes - 4, session->fn);
    fwrite(&session->recvbuf[4], sizeof(char),
        session->recvbytes - 4, session->file);

    prepare_ack_packet(session);
    return ACK;
}


int
parse_ack_packet(struct session_t * session)
{
    log("parsing ack packet\n");

    // Get block number
    size_t block_number = (session->recvbuf[2] << 8) + session->recvbuf[3];
    log("got block number %zu\n", block_number);

    if (block_number != session->block_n)
        return -1;
    else
        session->block_n++;

    prepare_data_packet(session);
    return DATA;
}

void
reset_session(struct session_t * session)
{
    log("resetting transfer\n");
    session->status = IDLE;
    session->fn[0] = '\0';
    session->block_n = 0;
    if (session->file)
        fclose(session->file);

    return;
}

int
parse_error_packet(struct session_t * session)
{
    log("parsing error packet\n");

    // Get error code & message
    int error_code = (session->recvbuf[2] << 8) + session->recvbuf[3];
    fprintf(stderr, "Could not transfer %s; got code %d, message: '%s'\n",
        session->fn, error_code, &session->recvbuf[4]);

    return -1;
}

void
send_packet(int sockfd, struct sockaddr_in * fromaddr,
    struct session_t * session)
{
    ssize_t sent_bytes = sendto(sockfd, session->sendbuf,
        session->sendbytes, 0,
        (struct sockaddr *)&fromaddr, sizeof(struct sockaddr));
    log("send %ld bytes\n", sent_bytes);

    return;
}
//=============================================================================
int
tftp_server(const int port, const int is_verbose)
{
    // must reset or else value is lost
    // XXX: there's got to be a better way of handling globals than this
    VERBOSE = is_verbose;

    // Setup listening on our pseudo well-known port.
    // In addition, just reuse this socket for transfers instead of juggling
    // two different ones.
    // XXX: for multiple clients, I imagine select() + multiple sockfds
    const int sockfd = get_udp_sockfd();
    struct sockaddr_in myaddr;
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;
    else
        set_socket_options(sockfd);
    // get network information about myself and then bind
    setup_my_sin(&myaddr, port);
    if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(struct sockaddr))
        == -1) {
        perror("bind");
        log("bind failed\n");
        return EXIT_FAILURE;
    } else {
        log("bind successful\n");
    }

    log("entering main program loop\n");
    int client_tid = -1;
    struct sockaddr_in fromaddr;
    struct session_t session;
    session.status = IDLE;

    // In this loop, we have to make sure that TIDs get sorted out.
    // Cases:
    // (1) if session is idle, listen on our "well-known port" (default
    // & occurs on reset of session)
    // (2) if session is active, listen on the ephemeral session port for
    // a singular TID (i.e. only respond to things on one port, ostensibly
    // from the same client)
    while (true) {
        log("waiting for udp packet\n");
        session.recvbytes = packet_listener(sockfd, session.recvbuf, &fromaddr);

        // Parse packet and prepare response
        switch (parse_packet(&session)) {
        case RRQ: case WRQ: case DATA: case ACK:
            if (client_tid == -1) {
                log("obtaining ephemeral port\n");
                setup_my_sin(&myaddr, 0);
                // XXX: assume success
                bind(sockfd, (struct sockaddr *)&myaddr, sizeof(struct sockaddr));
                client_tid = ntohs(myaddr.sin_port);
                log("listening on %d for packets from %d\n",
                    client_tid, ntohs(myaddr.sin_port));
            }

            log("sending packet\n");
            // send_packet(sockfd, &fromaddr, &session);
            break;
        case ERROR: case 0:
            log("sending packet and then resetting connection\n");
            // send_packet();
            client_tid = -1;
            setup_my_sin(&myaddr, port);
            // XXX: assume success
            bind(sockfd, (struct sockaddr *)&myaddr, sizeof(struct sockaddr));
            reset_session(&session);
            break;
        default: case -1:
            log("ignoring this packet\n");
            break;
        }
    }

    // This code will never be called since the loop above is infinite.
    // The kernel will handle closing the sockfd itself.
    log("closing down\n");
    close(sockfd);
    return EXIT_SUCCESS;
}
//=============================================================================
//=============================================================================
int
get_udp_sockfd(void)
{
    struct protoent * udp_proto = getprotobyname("udp");
    if (udp_proto == NULL) {
        perror("getprotobyname");
        return EXIT_FAILURE;
    } else {
        log("udp protocol number is %d\n", udp_proto->p_proto);
    }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, udp_proto->p_proto)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    } else {
        log("successfully got udp sockfd %d\n", sockfd);
    }

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

    log("attempting to set port reuse\n");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
        == -1) {
        perror("setsockopt");
        log("continuing without port reuse\n");
    } else {
        log("successfully set port reuse\n");
    }

    log("attempting to set timeout to %d seconds\n", TIMEOUT_SEC);
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
        (char*)&timeout, sizeof(struct timeval)) == -1) {
        perror("setsockopt");
        log("continuing without receive timeout\n");
    } else {
        log("successfully set receive timeout\n");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
        (char*)&timeout, sizeof(struct timeval)) == -1) {
        perror("setsockopt");
        log("continuing without send timeout\n");
    } else {
        log("successfully set send timeout\n");
    }

    return;
}
//-----------------------------------------------------------------------------
void
setup_my_sin(struct sockaddr_in * sin, int port)
{
    sin->sin_family = AF_INET;
    log("setting listen port to %d\n", port);
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = INADDR_ANY;
    memset(&(sin->sin_zero), '\0', 8);

    return;
}
//-----------------------------------------------------------------------------
ssize_t
packet_listener(int sockfd, buffer buf, struct sockaddr_in * fromaddr)
{
    log("listening for new packet\n");
    // XXX: We discard and do not check this value
    socklen_t fromaddr_len = sizeof(struct sockaddr);
    ssize_t recvbytes = recvfrom(sockfd, buf, MAX_BUFFER_LEN - 1, 0,
        (struct sockaddr *)fromaddr, &fromaddr_len);

    log("received %ld bytes\n", recvbytes);
    if (recvbytes == -1)
        perror("recvfrom");
    else {
        log("packet originated from %s:%d\n", inet_ntoa(fromaddr->sin_addr),
            ntohs(fromaddr->sin_port));
    }

    return recvbytes;
}
//=============================================================================
int
parse_packet(struct session_t * session)
{
    // Do a basic check for opcode validity
    if (session->recvbytes == -1) {
        log("got no packet. probably timeout. not parsing\n");
        return -1;
    } else if (session->recvbuf[0] != 0
        || session->recvbuf[1] < 1
        || session->recvbuf[1] > 5) {
        log("got invalid opcode...not parsing\n");
        return -1;
    } else {
        log("found opcode as 0%d\n", session->recvbuf[1]);
    }

    // These enum values are set to their opcodes, so we can switch
    // on either one--this is easier to read.
    switch (session->recvbuf[1]) {
    case RRQ:
        return parse_request_packet(session, true);
    case WRQ:
        return parse_request_packet(session, false);
    case DATA:
        return parse_data_packet(session);
    case ACK:
        return parse_ack_packet(session);
    case ERROR:
        return parse_error_packet(session);
    default:
        return -1;
    }
}
//-----------------------------------------------------------------------------
void
prepare_error_packet(struct session_t * session, char errcode, char * errmsg)
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
prepare_ack_packet(struct session_t * session)
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
prepare_data_packet(struct session_t * session)
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
    session->sendbytes = 2 + 2 + bytes_read;

    return;
}
