#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "args.h"
#include "list.h"
#include "rule.h"

const unsigned ARGS_QUEUE_BUFFER_NONE = 0;

/* Enum representing the various state of the parser */
enum parse_state {
    NO_STATE,     // Ready for anything!
    RULE,         // expecting a rule matcher (like "artist")
    RULE_VALUE,   // expecting rule value (like "modest mouse")
    QUEUE,        // expecting "queue_only" int value
    IFILE,        // expecting song list input file
    QUEUE_BUFFER  // expecting queue buffer value
};

/* check and see if 'to_check' matches any of 'count' given
 * arguments */
bool check_flags(const char * to_check, unsigned count, ...) {
    va_list args;
    const char * current;
    va_start(args, count);
    bool out = false;
    for (; count > 0; count--) {
        current = va_arg(args, char *);
        if (strcasecmp(to_check, current) == 0) {
            out = true; /* don't break, we need to process the arguments */
        }
    }
    va_end(args);
    return out;
}

/* get the enum rule_type type from the option if possible.
 * Otherwise, return -1 */
int rule_type_from_flag(char * option) {
   if (check_flags(option, 2, "--exclude", "-e")) {
        return RULE_EXCLUDE;
    } else {
        return -1;
    }
}

/* check and see if we can transition to a new top-level
 * state from our current state */
bool state_can_trans(enum parse_state state) {
    if (state == NO_STATE || state == RULE) { return true; }
    return false;
}

/* if we're in a correct state, then add the rule to the
 * ruleset in the list of options */
int flush_rule(enum parse_state state, 
               struct ashuffle_options * opts, 
               struct song_rule * rule) {
    if (state == RULE && rule->matchers.length > 0) {
        /* add the rule to the ruleset */
        list_push(&opts->ruleset, node_from(rule, sizeof(struct song_rule)));
    }
    return 0;
}

int ashuffle_init(struct ashuffle_options * opts) {
    opts->queue_only = 0;
    opts->file_in = NULL;
    opts->check_uris = true;
    list_init(&opts->ruleset);
    opts->queue_buffer = ARGS_QUEUE_BUFFER_NONE; // 0
    return 0;
}

int ashuffle_options(struct ashuffle_options * opts, 
                     int argc, char * argv[]) {
    /* State for the state machine */
    enum parse_state state = NO_STATE;
    bool transable = false;

    char * match_field = NULL;
    struct song_rule rule;

    int type_flag = -1;
    
    for (int i = 1; i < argc; i++) {
        transable = state_can_trans(state);
        if (transable) {
            type_flag = rule_type_from_flag(argv[i]);
        }

        /* check we should print the help text */
        if (check_flags(argv[i], 3, "--help", "-h", "-?")) {
            return -1;
        } else if (type_flag != -1) {
            flush_rule(state, opts, &rule);
            rule_init(&rule, type_flag);
            type_flag = -1;
            state = RULE;
        } else if (transable && check_flags(argv[i], 2, "--nocheck", "-n")) {
            flush_rule(state, opts, &rule);
            opts->check_uris = false;
            state = NO_STATE;
        } else if (transable && check_flags(argv[i], 1, "--queue_buffer")) {
            flush_rule(state, opts, &rule);
            state = QUEUE_BUFFER;
        } else if (transable && opts->queue_only == 0 && check_flags(argv[i], 2, "--only", "-o")) {
            flush_rule(state, opts, &rule);
            state = QUEUE;
        } else if (transable && opts->file_in == NULL && check_flags(argv[i], 2, "--file", "-f")) {
            flush_rule(state, opts, &rule);
            state = IFILE;
        } else if (state == RULE) {
            match_field = argv[i];
            state = RULE_VALUE;
        } else if (state == RULE_VALUE) {
            rule_add_criteria(&rule, match_field, argv[i]);
            match_field = NULL;
            state = RULE;
        } else if (state == QUEUE) {
            opts->queue_only = (unsigned) strtoul(argv[i], NULL, 10);
            /* Make sure we got a valid queue number */
            if (errno == EINVAL || errno == ERANGE) {
                fputs("Error converting queue length to integer.\n", stderr);
                return -1;
            }
            state = NO_STATE;
        } else if (state == IFILE) {
            if (check_flags(argv[i], 1, "-")) {
                opts->file_in = stdin;
            } else {
                opts->file_in = fopen(argv[i], "r");
            }
            state = NO_STATE;
        } else if (state == QUEUE_BUFFER) {
            opts->queue_buffer = (unsigned) strtoul(argv[i], NULL, 10);
            if (errno == EINVAL || errno == ERANGE) {
                fputs("Error converting queue buffer length to integer.\n", stderr);
                return -1;
            }
            state = NO_STATE;
        } else {
            fprintf(stderr, "Invalid option: %s.\n", argv[i]);
            return -1;
        }
    }

    if (state == RULE_VALUE) {
        fprintf(stderr, "No value supplied for match '%s'.\n", match_field);
        return -1;
    }
    /* if we're provisioning a rule right now, flush it */
    flush_rule(state, opts, &rule);
    return 0;
}

void ashuffle_help(FILE * output) {
    fputs(
    "usage: ashuffle -h -n { ..opts.. } [-e PATTERN ...] [-o NUMBER] [-f FILENAME]\n"
    "\n"
    "Optional Arguments:\n"
    "   -e,--exclude   Specify things to remove from shuffle (think blacklist).\n"
    "   -o,--only      Instead of continuously adding songs, just add 'NUMBER'\n"
    "                  songs and then exit.\n"
    "   -h,-?,--help   Display this help message.\n"
    "   -f,--file      Use MPD URI's found in 'file' instead of using the entire MPD\n"
    "                  library. You can supply `-` instead of a filename to retrive\n"
    "                  URI's from standard in. This can be used to pipe song URI's\n"
    "                  from another program into ashuffle.\n"
    "   -n,--nocheck   When reading URIs from a file, don't check to ensure that\n"
    "                  the URIs match the given exclude rules. This option is most\n"
    "                  helpful when shuffling songs with -f, that aren't in the\n"
    "                  MPD library.\n"
    "   --queue_buffer Specify to keep a buffer of `n` songs queued after the\n"
    "                  currently playing song. This is to support MPD features\n"
    "                  like crossfade that don't work if there are no more\n"
    "                  songs in the queue.\n"
    "See included `readme.md` file for PATTERN syntax.\n", output);
}

