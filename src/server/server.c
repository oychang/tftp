#include "server.h"

enum opcode
parse_request_packet(buffer buf, struct session_t * session, int is_read)
{
    // Check that the packet is a good size
    // Opcode + 1 Char filename + \0 + 1 Char mode + \0
    static const int min_req_length = 2 + 1 + 1 + 1 + 1;
    if (session->recvbytes < min_req_length) {
        log("aborting: request packet too short (min length is %d)\n",
            min_req_length);
        prepare_error_packet(&session->sendpkt, 0, "request too short");
        return ERROR;
    } else if (session->status != IDLE) {
        log("aborting: transfer still in session\n");
        prepare_error_packet(&session->sendpkt, 3, "transfer in progress");
        return ERROR;
    }

    // Set staus
    session->status = (is_read ? RRQ : WRQ);
    log("parsing %s packet\n", is_read ? "RRQ" : "WRQ");

    // Set filename field
    strncpy(session->fn, &buf[2], MAX_STRING_LEN);
    session->fn[MAX_STRING_LEN-1] = '\0'; // ensure null-terminated
    log("got filename as %s\n", session->fn);

    // Check filename is accessible
    // (1): only current directory allowed (no ../foo or /foo)
    // (2): only do if accessible to everyone
    // (3): only do if exists already
    struct stat statbuf;
    if (strstr(session->fn, "..") != NULL) {
        log("terminating: found up traversal\n");
        prepare_error_packet(&session->sendpkt, 2, "no up traversal");
        return ERROR;
    } else if (is_read) {
        if (!stat(session->fn, &statbuf)
            && (statbuf.st_mode & (UGOR)) != (UGOR)) {
            log("terminating: file not readable by all\n");
            prepare_error_packet(&session->sendpkt, 2, "bad read permissions");
            return ERROR;
        }
    }

    // So we can access the file!
    // Now, open a file descriptor to it
    session->file = fopen(session->fn, is_read ? "r" : "w");

    // Check mode equal to octet
    const size_t filename_len = strlen(session->fn); // get for offset
    static const char * octet = "octet";
    const size_t octet_len = strlen(octet);
    if (strncasecmp(&buf[2+filename_len+1], octet, octet_len)) {
        log("aborting: only octet mode supported\n");
        prepare_error_packet(&session->sendpkt, 0, "only octet supported");
        return ERROR;
    }

    // Set initial block number
    if (is_read) {
        session->block_n = 1;
        prepare_data_packet(session);
        return DATA;
    }
    session->block_n = 0;
    prepare_ack_packet(&session->sendpkt, session->block_n);
    return ACK;
}

int
parse_data_packet(buffer buf, struct session_t * session)
{
    // Check that packet is a good size
    // Opcode (2B) + Block Number (2B) + n bytes (max 512)
    static const int min_data_len = 2 + 2 + 0; // can get 0 bytes
    static const int max_data_len = 2 + 2 + 512;
    if (session->recvbytes > max_data_len
        || session->recvbytes < min_data_len) {
        log("aborting: data packet size is wrong\n");
        prepare_error_packet(&session->sendpkt, 0, "bad data packet size");
        return ERROR;
    } else if (session->status != RECV) {
        log("aborting: connection hasn't been opened or we are sender");
        prepare_error_packet(&session->sendpkt, 4, "not receiving data");
        return ERROR;
    }

    // Set block number
    log("parsing data packet\n");
    size_t block_number = (buf[2] << 8) + buf[3];
    log("got block number as %zu\n", block_number);
    // Check for Sorcerer's Apprentice Bug
    // That is, check if we've already sent an ACK for the block number
    // we just received and if so, ignore this packet.
    if (block_number <= session->block_n) {
        log("SAS detected\n");
        log("Got block# %zu while expecting block# %zu\n",
            block_number, session->block_n);
        return -1;
    } else
        session->block_n++;

    // Write data out to disk
    log("writing out %zu bytes to %s\n",
        session->recvbytes - 4, session->fn);
    fwrite(&buf[4], sizeof(char), session->recvbytes - 4, session->file);

    prepare_ack_packet(&session->sendpkt, session->block_n);
    return ACK;
}


int
parse_ack_packet(buffer buf, struct session_t * session)
{
    log("parsing ack packet\n");

    // Get block number
    size_t block_number = (buf[2] << 8) + buf[3];
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
    session->tid[0] = -1;
    session->tid[1] = -1;
    if (session->file)
        fclose(session->file);

    return;
}

int
parse_error_packet(buffer buf, struct session_t * session)
{
    log("parsing error packet\n");

    // Get error code & message
    int error_code = (buf[2] << 8) + buf[3];
    fprintf(stderr, "Could not transfer %s; got code %d, message: '%s'\n",
        session->fn, error_code, &buf[4]);

    return -1;
}

void
send_packet(int sockfd, buffer sendbuf, fromaddr, &session)
{
    ssize_t sent_bytes = sendto(sockfd, sendbuf,
        session->sendbytes, 0, fromaddr)

    return;
}
//=============================================================================
int
tftp_server(int port, int is_verbose)
{
    // must reset or else value is lost
    VERBOSE = is_verbose;

    // get a socket file descriptor
    const int sockfd = get_udp_sockfd();
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;
    set_socket_options(sockfd);

    // get network information about myself and then bind
    struct sockaddr_in myaddr;
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
    struct sockaddr_in fromaddr;
    buffer recvbuf;
    // buffer sendbuf;
    struct session_t session;
    session.status = IDLE;
    while (true) {
        // Listen for udp packet
        log("waiting for udp packet\n");
        session.recvbytes = packet_listener(sockfd, recvbuf, &fromaddr);

        // Parse packet and prepare response
        switch (parse_packet(recvbuf, &session)) {
        case RRQ: case WRQ: case DATA: case ACK:
            send_packet(sockfd, &fromaddr, &session);
            break;
        case ERROR: case 0:
            send_packet();
            reset_session();
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
    log("getting udp socket file descriptor\n");

    struct protoent * udp_proto = getprotobyname("udp");
    if (udp_proto == NULL) {
        perror("getprotobyname");
        return EXIT_FAILURE;
    }
    log("udp protocol number is %d\n", udp_proto->p_proto);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, udp_proto->p_proto)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    log("successfully got sockfd %d\n", sockfd);

    return sockfd;
}


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


ssize_t
packet_listener(int sockfd, buffer buf, struct sockaddr_in * fromaddr)
{
    log("listening for new packet\n");
    // TODO: We discard and do not check this value
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


int
parse_packet(buffer buf, struct session_t * session)
{
    // Do a basic check for opcode validity
    if (session->recvbytes == -1) {
        log("got no packet. probably timeout. not parsing\n");
        return -1;
    } else if (buf[0] != 0 || buf[1] < 1 || buf[1] > 5) {
        log("got invalid opcode...not parsing\n");
        prepare_error_packet(&session->sendpkt, 4, "invalid opcode");
        return ERROR;
    } else {
        log("found opcode as 0%d\n", buf[1]);
    }

    switch (buf[1]) {
    case RRQ:
        return parse_request_packet(buf, session, true);
    case WRQ:
        return parse_request_packet(buf, session, false);
    case DATA:
        return parse_data_packet(buf, session);
    case ACK:
        return parse_ack_packet(buf, session);
    default: case ERROR:
        return parse_error_packet(buf, session);
    }
}


void
prepare_error_packet(union packet * pkt, char errcode, char * errmsg)
{
    log("preparing error packet with code 0%d and message %s\n",
        errcode, errmsg);
    memcpy(pkt->error.opcode, (char [2]){0, ERROR}, 2*sizeof(char));
    memcpy(pkt->error.errorcode, (char [2]){0, errcode}, 2*sizeof(char));
    strcpy(pkt->error.errmsg, errmsg);
    return;
}


void
prepare_ack_packet(union packet * pkt, size_t block_n)
{
    char byte_a = block_n >> 8;
    char byte_b = block_n & 0xff;
    log("preparing ack packet for block# %zu (high:%d low:%d)\n",
        block_n, byte_a, byte_b);
    memcpy(pkt->ack.opcode, (char [2]){0, ACK}, 2*sizeof(char));
    memcpy(pkt->ack.block_number, (char [2]){byte_a, byte_b}, 2*sizeof(char));

    return;
}

void
prepare_data_packet(struct session_t * session)
{
    char byte_a = session->block_n >> 8;
    char byte_b = session->block_n & 0xff;
    log("preparing data packet for block# %zu (high:%d low:%d)\n",
        session->block_n, byte_a, byte_b);
    memcpy(session->sendpkt.data.opcode, (char [2]){0, DATA}, 2*sizeof(char));
    memcpy(session->sendpkt.data.block_number,
        (char [2]){byte_a, byte_b}, 2*sizeof(char));
    // Read data from disk
    int bytes_read = fread(&session->sendpkt.data.data,
        sizeof(char), 512, session->file);
    session->sendbytes = bytes_read;

    return;
}
