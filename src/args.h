#include <stdio.h>
#include <stdbool.h>
#include "list.h"

#ifndef ASHUFFLE_ARGS_H
#define ASHUFFLE_ARGS_H

const unsigned ARGS_QUEUE_BUFFER_NONE; // 0

struct ashuffle_options {
    struct list ruleset;
    unsigned queue_only;
    FILE * file_in;
    bool check_uris;
    unsigned queue_buffer;
};

int ashuffle_init(struct ashuffle_options *);

/* parse the options in to the 'ashuffle options' structure. 
 * Returns 0 on success, -1 for failure. */
int ashuffle_options(struct ashuffle_options *, 
                     int argc, char * argv[]);

void ashuffle_help(FILE * output_stream);

#endif
