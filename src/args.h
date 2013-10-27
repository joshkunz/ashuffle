#include <stdio.h>
#include <stdbool.h>
#include "array.h"

#ifndef ASHUFFLE_ARGS_H
#define ASHUFFLE_ARGS_H

struct ashuffle_options {
    struct auto_array ruleset;
    unsigned queue_only;
    FILE * file_in;
    bool check_uris;
};

int ashuffle_init(struct ashuffle_options *);

/* parse the options in to the 'ashuffle options' structure. 
 * Returns 0 on success, -1 for failure. */
int ashuffle_options(struct ashuffle_options *, 
                     int argc, char * argv[]);

void ashuffle_help(FILE * output_stream);

#endif
