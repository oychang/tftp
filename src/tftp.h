#ifndef TFTP_H
#define TFTP_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // getopt
#include "Boolean.h"
#include "Logging.h"
#include "server/server.h"

#define DEFAULT_SERVER_PORT 3335
#define DEFAULT_CLIENT_PORT 3335

// print the usage strings to stderr
void print_usage(void);

#endif
