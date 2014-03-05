#ifndef LOGGING_H
#define LOGGING_H

// Macro that depends on a global variable to be set to true/falsey
// and prints to stdout with normal printf formatting information.
#include <stdio.h>
static int VERBOSE = 0;
#define log(format_string, ...) \
        if (VERBOSE) printf("%s: ", __FUNCTION__); \
        if (VERBOSE) printf(format_string, ##__VA_ARGS__)

#endif

