#include "tftp.h"

int
main(int argc, char *argv[])
{
    // http://www.ibm.com/developerworks/aix/library/au-unix-getopt.html
    static const char * optstr = "lp:vrwh";
    int is_server = false;
    int is_verbose = false;
    int is_read = false;
    char * file = NULL;
    char * host = NULL;
    int port = -1;

    int opt;
    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
        case 'l':
            is_server = true;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'v':
            VERBOSE = is_verbose = true;
            break;
        case 'r':
            is_read = true;
            break;
        case 'w':
            is_read = false;
            break;
        case 'h': default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (argv[optind] != NULL)
        file = argv[optind];
    if (argv[optind+1] != NULL)
        host = argv[optind + 1];

    if (is_server == true) {
        port = (port == -1) ? DEFAULT_SERVER_PORT : port;
        log("port=%d; is_verbose=%d\n", port, is_verbose);
        return tftp_server(port, is_verbose);
    }

    port = (port == -1) ? DEFAULT_CLIENT_PORT : port;
    if (file == NULL || host == NULL) {
        fprintf(stderr, "must specifiy a file and host\n");
        print_usage();
        return EXIT_FAILURE;
    } else {
        log("port=%d; is_verbose=%d; is_read=%d; file=%s; host=%s\n",
            port, is_verbose, is_read, file, host);
    }

    return 0; //tftp_client(port, is_verbose, is_read, file, host);
}

void
print_usage(void)
{
    fprintf(stderr, "\nusage (server): mytftp -l [-p port] [-v]\n");
    fprintf(stderr, "usage (client): mytftp [-p port] [-v] -[r|w] file host\n");
    fprintf(stderr, "see README.md for more information.\n");
    return;
}
