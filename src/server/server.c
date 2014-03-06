#include "server.h"

// XXX: assume strings are null-terminated or are > max_string_len
int
parse_request_packet(buffer buf, union packet * pkt, int is_read)
{
    // Set opcode
    memcpy(pkt->rq.opcode, (char [2]){0, is_read ? RRQ : WRQ}, 2*sizeof(char));
    log("parsing %s packet\n", is_read ? "RRQ" : "WRQ");

    // Get filename field
    strncpy(pkt->rq.filename, &buf[2], MAX_STRING_LEN);
    pkt->rq.filename[MAX_STRING_LEN-1] = '\0'; // ensure null-terminated
    const size_t filename_len = strlen(pkt->rq.filename); // get for offset
    log("got filename as %s\n", pkt->rq.filename);

    // Get mode
    strncpy(pkt->rq.mode, &buf[2+filename_len+1], MAX_STRING_LEN);
    pkt->rq.mode[MAX_STRING_LEN-1] = '\0';
    log("got mode as %s\n", pkt->rq.mode);
    // Verify equal to octet
    static const char * octet = "octet";
    const size_t octet_len = strlen(octet);
    if (strncasecmp(pkt->rq.mode, octet, octet_len)) {
        log("%s != %s\n", pkt->rq.mode, octet);
        return 1;
    }

    log("successfully parsed packet\n");
    return 0;
}

int
parse_data_packet(buffer buf, union packet * pkt)
{
    // Set opcode
    memcpy(pkt->data.opcode, (char [2]){0, DATA}, 2*sizeof(char));
    log("parsing data packet\n");

    // Set block number
    pkt->data.block_number[0] = buf[2];
    pkt->data.block_number[1] = buf[3];
    log("got block number as %d%d\n",
        pkt->data.block_number[0], pkt->data.block_number[1]);

    // Copy over anything that might be data
    memcpy(pkt->data.data, &buf[4], 512);
    log("copied over data with memcpy()\n");

    return 0;
}


int
parse_ack_packet(buffer buf, union packet * pkt)
{
    // Set opcode
    memcpy(pkt->ack.opcode, (char [2]){0, ACK}, 2*sizeof(char));
    log("parsing ack packet\n");

    // Set block number
    pkt->ack.block_number[0] = buf[2];
    pkt->ack.block_number[1] = buf[3];
    log("got block number %d%d\n",
        pkt->ack.block_number[0], pkt->ack.block_number[1]);
    // Verify block number >= 0
    if (pkt->ack.block_number[0] < 0 || pkt->ack.block_number[1] < 0) {
        log("error: negative block number\n");
        return 1;
    }

    return 0;
}

// Returns -1 for send packet error set

int
parse_error_packet(buffer buf, union packet * pkt)
{
    // Set opcode
    memcpy(pkt->error.opcode, (char [2]){0, ERROR}, 2*sizeof(char));
    log("parsing error packet\n");

    // Set error code
    pkt->error.errorcode[0] = buf[2];
    pkt->error.errorcode[1] = buf[3];
    log("got error code as %d%d\n",
        pkt->error.errorcode[0], pkt->error.errorcode[1]);

    // Set error message
    strncpy(pkt->error.errmsg, &buf[4], MAX_STRING_LEN);
    pkt->error.errmsg[MAX_STRING_LEN-1] = '\0';
    log("got error message as %s\n", pkt->error.errmsg);

    return 0;
}

void
prepare_error_packet(union packet * pkt, char errcode, char * errmsg)
{
    log("preparing error packet with code 0%d and message %s\n",
        errcode, errmsg);
    memcpy(pkt->error.opcode, (char [2]){0, 5}, 2*sizeof(char));
    memcpy(pkt->error.errorcode, (char [2]){0, errcode}, 2*sizeof(char));
    strcpy(pkt->error.errmsg, errmsg);
    return;
}

int
prepare_ack_packet(int retcode, struct tftp_session * session)
{
    return ACK;
}

/*
Return values:
    -1: premature termination, please send the error packet I've prepared
    0: good packet, continue with checks (i.e. allowed files, same client) and then send out
    1: normal termination, continue with checks (i.e. same client) & ACK & close down
*/
// return -1 for failure, else the enum value for the value of sndpkt
int
parse_packet(ssize_t recvbytes, buffer buf, struct tftp_session * session)
{
    // Do a basic check for tftp opcode validity
    if (recvbytes == -1) {
        log("got bad packet...not parsing\n");
        prepare_error_packet(&session->sendpkt, 0, "timeout probably");
        return -1;
    } else if (buf[0] != 0 || buf[1] < 1 || buf[1] > 5) {
        log("got invalid opcode...not parsing\n");
        prepare_error_packet(&session->sendpkt, 4, "invalid opcode");
        return -1;
    }
    log("found opcode as 0%d\n", buf[1]);

    switch (buf[1]) {
    default:
    case RRQ:
        return prepare_ack_packet(
            parse_request_packet(buf, &session->recvpkt, true),
            session
        );
    // case WRQ:
    //     return prepare_ack_packet(
    //         parse_request_packet(buf, &session->recvpkt, false),
    //         session
    //     );
    // case DATA:
    //     return prepare_ack_packet(
    //         parse_data_packet(buf, &session->recvpkt),
    //         session
    //     );
    // case ACK:
    //     return prepare_data_packet(
    //         parse_ack_packet(buf, &session->recvpkt),
    //         session
    //     );
    // default: case ERROR:
    //     return prepare_error_packet(
    //         parse_error_packet(buf, &session->recvpkt),
    //         session
    //     );
    }
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
    ssize_t recvbytes;
    struct tftp_session session;
    int have_response = false;
    while (true) {
        // Listen for udp packet
        log("waiting for udp packet\n");
        recvbytes = packet_listener(sockfd, recvbuf, &fromaddr);

        // Parse packet and prepare response
        if ((have_response = parse_packet(recvbytes, recvbuf, &session))
            == -1) {
            log("got -1 from parser, sending no response\n");
        } else {
            log("have a response, about to send\n");
            // send_response(have_response);
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
