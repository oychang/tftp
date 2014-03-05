#include "server.h"


// /* Parse out the correct opcode present in the first to fields
//  * of the packet
//  */
// enum tftp_opcode
// get_opcode(char a, char b)
// {
//     if (a != 0 || b < 1 || b > 5)
//         return INVALID;

//     switch (b) {
//     case 1: return RRQ;
//     case 2: return WRQ;
//     case 3: return DATA;
//     case 4: return ACK;
//     default:
//     case 5: return ERROR;
//     }
// }

// /*
//     2B        2B        str      1B
// | Opcode | ErrorCode | ErrMsg | '\0' |
// */
// void
// send_error_packet(enum error_codes code, string message)
// {
//     tftp_packet response = {0, 5};
//     return;
// }

// /*
//   2 bytes   string    1byte string 1byte
// | Opcode | Filename | '\0' | Mode | '\0' |
// */
// int
// parse_request_packet(tftp_packet packet, struct tftp_session * session)
// {
//     // Check that we aren't already sending a file
//     if (session->status != IDLE)
//         send_error_packet(NOTDEFINED, "transfer already in progress");
//     // Assign ports and set the session struct to the correct status and fn
//     string fn = "";
//     strncpy(fn, &packet[2], MAX_STRING_LEN);
//     printf("string: %s\n", fn);
//     // get mode, check that it is acceptable
//     static const char netascii[] = "netascii";
//     string mode = "";
//     if (strncasecmp(mode, netascii, strlen(netascii)))
//         send_error_packet(NOTDEFINED, "only netascii supported");
//     // TODO: Check that file is valid and allowed access



//     return -1;
// }

// int
// parse_packet(tftp_packet packet, struct tftp_session * session)
// {
//     enum tftp_opcode opcode = get_opcode(packet[0], packet[1]);

//     switch (opcode) {
//     default:
//     case RRQ: case WRQ: return parse_request_packet(packet, session);
//     // case DATA:          return parse_data_packet(packet);
//     // case ACK:           return parse_ack_packet(packete);
//     // case ERROR:         return parse_error_packet(packet);
//     }

//     return -1;
// }


ssize_t
packet_listener(int sockfd, tftp_packet buf, struct sockaddr_in * fromaddr) {
    // TODO: We discard and do not check this value
    socklen_t fromaddr_len = sizeof(struct sockaddr);
    ssize_t recvbytes = recvfrom(sockfd, buf, MAX_BUFFER_LEN - 1, 0,
        (struct sockaddr *)fromaddr, &fromaddr_len);

    if (recvbytes == -1)
        perror("recvfrom");

    log("received %ld bytes\n", recvbytes);
    return recvbytes;
}

int
tftp_server(int port, int is_verbose)
{
    // must reset or else value is lost
    VERBOSE = is_verbose;

    // get a socket file descriptor
    const int sockfd = get_udp_sockfd();
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;
    set_reusable_ports(sockfd);

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
    ssize_t recvbytes;
    tftp_packet recvbuf;
    // tftp_packet send_buf;
    while (true) {
        // listen for udp packet
        log("waiting for udp packet\n");
        if ((recvbytes = packet_listener(sockfd, recvbuf, &fromaddr)) == -1)
            return EXIT_FAILURE;

        // parse packet


        // respond to packet

    }

    // This code will never be called since the loop above is infinite.
    // The kernel will handle closing the sockfd itself.
    log("closing down\n");
    close(sockfd);
    return EXIT_SUCCESS;
}


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
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, udp_proto->p_proto))
        == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }
    log("successfully got sockfd %d\n", sockfd);

    return sockfd;
}


void
set_reusable_ports(int sockfd)
{
    log("attempting to set port reuse\n");

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
        == -1) {
        perror("setsockopt");
        log("continuing without port reuse\n");
    } else
        log("successfully set\n");

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

// void
// listen_respond_loop(int sockfd, struct sockaddr_in * my_addr)
// {
    // /* packet handling */
    // // The length of the from address
    // socklen_t           fromaddr_len;
    // // A char buffer that represents the bytes of the packet
    // tftp_packet         buf;
    // // The number of received bytes
    // ssize_t             recvbytes;
    // struct tftp_session session;// = {IDLE, 0};
    // session.status = IDLE;
    // struct sockaddr_in from;

    // while (1) {
    //     // always reset to address size
    //     // after recvfrom(), set to the actual size of the source address
    //     fromaddr_len = sizeof(struct sockaddr);
    //     // receive packet, put it in buffer
    //     // blocks until a packet is available
    //     if ((recvbytes = recvfrom(sockfd, buf, MAX_BUFFER_LEN - 1, 0,
    //         (struct sockaddr *)&from, &fromaddr_len))
    //         == -1) {
    //         perror("recvfrom");
    //         return EXIT_FAILURE;
    //     }

    //     // start to pick apart packet
    //     parse_packet(buf, &session);

    //     // respond
    //     // ssize_t sent_bytes = sendto(sockfd, buf, strlen(buf), 0,
    //     //     (struct sockaddr *)&from, sizeof(struct sockaddr));
    //     // if (sent_bytes == -1) {
    //     //     perror("sendto");
    //     //     return EXIT_FAILURE;
    //     // }
    // }
// }
