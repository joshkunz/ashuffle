#include <stdbool.h>
#include <stdio.h>
#include "list.h"

#ifndef ASHUFFLE_ARGS_H
#define ASHUFFLE_ARGS_H

const unsigned ARGS_QUEUE_BUFFER_NONE;  // 0

struct ashuffle_test_options {
    bool print_all_songs_and_exit;
};

struct ashuffle_options {
    struct list ruleset;
    unsigned queue_only;
    FILE *file_in;
    bool check_uris;
    unsigned queue_buffer;
    char *host;
    unsigned port;
    struct ashuffle_test_options test;
};

typedef enum {
    PARSE_FAILURE,  // We failed to parse the input args.
    PARSE_HELP,     // The user requested help text to be printed.
    PARSE_OK,       // Options fully parsed.
} parse_status_t;

struct options_parse_result {
    parse_status_t status;
    char *msg;
};

void options_parse_result_free(struct options_parse_result *);

void options_init(struct ashuffle_options *);

/* parse the options in to the 'ashuffle options' structure. The returned
 * parse result describes the results of the parse.
 * options_parse_result_free *must* be called reguardless of the status
 * of the parse. */
struct options_parse_result options_parse(struct ashuffle_options *, int argc,
                                          const char *argv[]);

void options_help(FILE *output_stream);

#endif
