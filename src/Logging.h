#ifndef LOGGING_H
#define LOGGING_H

// Macro that depends on a global variable to be set to true/falsey
// and prints to stdout with normal printf formatting information.
#include <stdio.h>
static int VERBOSE = 0;
// NB: this expands into two statements, thus will
// lead to undesired logging with optional bracket things, e.g. if-else
#define log(format_string, ...) \
        if (VERBOSE) printf("%s: ", __FUNCTION__); \
        if (VERBOSE) printf(format_string, ##__VA_ARGS__)

#endif
