#include "tftp.h"

// Process command line arguments with getopt and then delegate
// to the correct half of the project.
// Valuable: http://www.ibm.com/developerworks/aix/library/au-unix-getopt.html
int
main(int argc, char *argv[])
{
    static const char * optstr = "lp:vrwh";
    int is_server = false;
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
            VERBOSE = true;
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

    // Check and call server
    if (is_server == true) {
        port = (port == -1) ? DEFAULT_SERVER_PORT : port;
        log("args are port=%d, is_verbose=%d\n", port, VERBOSE);
        return tftp_server(port);
    }

    // Get keyword arguments, check and call client
    if (argv[optind] != NULL)
        file = argv[optind];
    if (argv[optind+1] != NULL)
        host = argv[optind + 1];
    if (file == NULL || host == NULL) {
        fprintf(stderr, "must specifiy a file and host\n");
        print_usage();
        return EXIT_FAILURE;
    }

    port = (port == -1) ? DEFAULT_CLIENT_PORT : port;
    log("args are port=%d, is_verbose=%d, is_read=%d, file=%s, host=%s\n",
        port, VERBOSE, is_read, file, host);
    return tftp_client(port, is_read, file, host);
}

void
print_usage(void)
{
    fprintf(stderr, "\nusage (server): mytftp -l [-p port] [-v]\n");
    fprintf(stderr, "usage (client): mytftp [-p port] [-v] -[r|w] file host\n");
    fprintf(stderr, "see README.md for more information.\n");
    return;
}
