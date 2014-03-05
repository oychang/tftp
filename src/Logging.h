#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
static int VERBOSE = 0;
#define log(format_string, ...) \
        if (VERBOSE) printf(format_string, ##__VA_ARGS__)

#endif

