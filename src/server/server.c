#include "server.h"

int
get_udp_socket()
{
    // get the correct ip header protocol number for udp
    struct protoent * udp_proto = getprotobyname("udp");
    if (udp_proto == NULL) {
        perror("could not find udp proto number");
        return EXIT_FAILURE;
    }

    // create the socket
    int sockfd = socket(
        AF_INET, // internet
        SOCK_DGRAM, // datagram
        udp_proto->p_proto
    );
    if (sockfd == -1) {
        perror("could not create socket");
        return EXIT_FAILURE;
    }

    return sockfd;
}

void
set_reusable_ports(int sockfd)
{
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        fprintf(stderr, "continuing without port reuse\n");
    }

    return;
}

void
get_my_addr(struct sockaddr_in * sin)
{
    sin->sin_family = AF_INET;
    sin->sin_port = htons(LISTEN_PORT);
    sin->sin_addr.s_addr = INADDR_ANY;
    memset(&(sin->sin_zero), '\0', 8);
    return;
}

int
main(void)
{
    /* housekeeping */
    // get a socket file descriptor
    const int sockfd = get_udp_socket();
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;
    set_reusable_ports(sockfd);

    // get network information about myself and then bind
    struct sockaddr_in my_addr;
    get_my_addr(&my_addr);
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    /* packet handling */
    struct sockaddr_in from;
    socklen_t fromlen;
    // use char type so each index is a byte
    char buf[MAX_BUFFER_LEN];
    ssize_t n_bytes;
    // int block_n = 0;

    while (1) {
        // receive packet, put it in buffer
        fromlen = sizeof(struct sockaddr);
        n_bytes = recvfrom(sockfd, buf, MAX_BUFFER_LEN - 1, 0,
            (struct sockaddr *)&from, &fromlen
        );
        if (n_bytes == -1) {
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        // start to pick apart packet
        // get opcode
        printf("%d%d\n", buf[0], buf[1]);

        // respond
        // ssize_t sent_bytes = sendto(sockfd, buf, strlen(buf), 0,
        //     (struct sockaddr *)&from, sizeof(struct sockaddr));
        // if (sent_bytes == -1) {
        //     perror("sendto");
        //     return EXIT_FAILURE;
        // }
    }

    /* fin */
    // close the file descriptor for the socket
    close(sockfd);
    return EXIT_SUCCESS;
}


