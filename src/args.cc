#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "list.h"
#include "rule.h"
#include "util.h"

// Maximum string length for a port option.
static const unsigned PORTLEN = 6;

/* Enum representing the various state of the parser */
enum parse_state {
    NO_STATE,      // Ready for anything!
    HOST,          // expecting a hostname or ip address
    IFILE,         // expecting song list input file
    PORT,          // expecting a port number
    QUEUE,         // expecting "queue_only" int value
    QUEUE_BUFFER,  // expecting queue buffer value
    RULE,          // expecting a rule matcher (like "artist")
    RULE_FIRST,    // expecting first rule matcher (see RULE). This is needed
                   // so that a bare `-e` or `--exclude` is not matched.
    RULE_VALUE,    // expecting rule value (like "modest mouse")
    TEST,          // expecting a test option
};

/* check and see if 'to_check' matches any of 'count' given
 * arguments */
static bool check_flags(const char *to_check, unsigned count, ...) {
    va_list args;
    const char *current;
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

/* check and see if we can transition to a new top-level
 * state from our current state */
static bool state_can_trans(enum parse_state state) {
    return state == NO_STATE || state == RULE;
}

/* if we're in a correct state, then add the rule to the
 * ruleset in the list of options */
static void flush_rule(enum parse_state state, struct ashuffle_options *opts,
                       const Rule &rule) {
    if (state != RULE || rule.Empty()) {
        return;
    }
    opts->ruleset.push_back(rule);
}

void options_init(struct ashuffle_options *opts) {
    opts->queue_only = 0;
    opts->file_in = nullptr;
    opts->check_uris = true;
    opts->queue_buffer = ARGS_QUEUE_BUFFER_NONE;  // 0
    opts->host = nullptr;
    opts->port = 0;
    memset(&opts->test, 0, sizeof(opts->test));
}

/* "safe" string to unsigned conversion
 * unlike strtoul, considers partial matches e.g. "42foo" or "" as errors.
 * returns UINT_MAX and sets errno on error.
 */
static unsigned strtou(const char *str) {
    char *endptr;
    unsigned long value;

    value = strtoul(str, &endptr, 10);

    if (endptr == str || endptr[0] != '\0') {
        errno = EINVAL;
        return UINT_MAX;
    }

    if (value > UINT_MAX) {
        errno = ERANGE;
        return UINT_MAX;
    }

    return (unsigned)value;
}

struct options_parse_result options_parse(struct ashuffle_options *opts,
                                          int argc, const char *argv[]) {
    /* State for the state machine */
    enum parse_state state = NO_STATE;
    bool transable = false;

    const char *match_field = NULL;
    Rule rule;

#define PARSE_FAIL(fmt, ...)                                        \
    return (struct options_parse_result) {                          \
        .status = PARSE_FAILURE, .msg = xsprintf(fmt, __VA_ARGS__), \
    }

    for (int i = 0; i < argc; i++) {
        transable = state_can_trans(state);

        /* check we should print the help text */
        if (check_flags(argv[i], 3, "--help", "-h", "-?")) {
            return (struct options_parse_result){
                .status = PARSE_HELP,
                .msg = NULL,
            };
        } else if (transable && check_flags(argv[i], 2, "--exclude", "-e")) {
            flush_rule(state, opts, rule);
            rule = Rule();
            state = RULE_FIRST;
        } else if (transable && check_flags(argv[i], 2, "--no-check", "-n")) {
            flush_rule(state, opts, rule);
            opts->check_uris = false;
            state = NO_STATE;
        } else if (transable &&
                   check_flags(argv[i], 2, "--queue-buffer", "-q")) {
            flush_rule(state, opts, rule);
            state = QUEUE_BUFFER;
        } else if (transable && opts->queue_only == 0 &&
                   check_flags(argv[i], 2, "--only", "-o")) {
            flush_rule(state, opts, rule);
            state = QUEUE;
        } else if (transable && opts->file_in == NULL &&
                   check_flags(argv[i], 2, "--file", "-f")) {
            flush_rule(state, opts, rule);
            state = IFILE;
        } else if (transable && check_flags(argv[i], 1, "--host")) {
            flush_rule(state, opts, rule);
            state = HOST;
        } else if (transable && check_flags(argv[i], 2, "--port", "-p")) {
            flush_rule(state, opts, rule);
            state = PORT;
        } else if (transable &&
                   check_flags(argv[i], 1, "--test_enable_option_do_not_use")) {
            flush_rule(state, opts, rule);
            state = TEST;
        } else if (state == RULE || state == RULE_FIRST) {
            match_field = argv[i];
            state = RULE_VALUE;
        } else if (state == RULE_VALUE) {
            rule.AddPattern(match_field, argv[i]);
            match_field = NULL;
            state = RULE;
        } else if (state == QUEUE) {
            opts->queue_only = strtou(argv[i]);
            if (opts->queue_only == UINT_MAX) {
                PARSE_FAIL("couldn't convert queue length '%s' to integer.",
                           argv[i]);
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
            opts->queue_buffer = strtou(argv[i]);
            if (opts->queue_buffer == UINT_MAX) {
                PARSE_FAIL(
                    "couldn't convert queue buffer length '%s' to integer.",
                    argv[i]);
            }
            state = NO_STATE;
        } else if (state == HOST) {
            opts->host = xstrdup(argv[i]);
            state = NO_STATE;
        } else if (state == PORT) {
            if (strlen(argv[i]) >= PORTLEN) {
                PARSE_FAIL("port value '%s' too large.", argv[i]);
            }
            opts->port = strtou(argv[i]);
            if (opts->port == UINT_MAX) {
                PARSE_FAIL("couldn't convert port '%s' to integer.", argv[i]);
            }
            state = NO_STATE;
        } else if (state == TEST) {
            if (check_flags(argv[i], 1, "print_all_songs_and_exit")) {
                opts->test.print_all_songs_and_exit = true;
            } else {
                PARSE_FAIL("bad test option '%s'", argv[i]);
            }
            state = NO_STATE;
        } else {
            PARSE_FAIL("bad option '%s'", argv[i]);
        }
    }

    if (state == RULE_VALUE) {
        PARSE_FAIL("no value supplied for match '%s'", match_field);
    } else if (!state_can_trans(state)) {
        PARSE_FAIL("no argument supplied for '%s'", argv[argc - 1]);
    }
    /* if we're provisioning a rule right now, flush it */
    flush_rule(state, opts, rule);
    return (struct options_parse_result){.status = PARSE_OK, .msg = NULL};
}

void options_parse_result_free(struct options_parse_result *r) {
    if (r->msg != NULL) {
        free(r->msg);
    }
}

void options_free(struct ashuffle_options *opts) {
    // Also need to free the host string, if it's been allocated.
    free(opts->host);
}

static const char *HELP_MESSAGE =
    "usage: ashuffle [-h] [-n] [-e PATTERN ...] [-o NUMBER] [-f FILENAME] "
    "[-q NUMBER]\n"
    "\n"
    "Optional Arguments:\n"
    "   -h,-?,--help      Display this help message.\n"
    "   -e,--exclude      Specify things to remove from shuffle (think\n"
    "                     blacklist).\n"
    "   -f,--file         Use MPD URI's found in 'file' instead of using the\n"
    "                     entire MPD library. You can supply `-` instead of a\n"
    "                     filename to retrive URI's from standard in. This\n"
    "                     can be used to pipe song URI's from another program\n"
    "                     into ashuffle.\n"
    "   --host            Specify a hostname or IP address to connect to.\n"
    "                     Defaults to `localhost`.\n"
    "   -n,--no-check     When reading URIs from a file, don't check to\n"
    "                     ensure that the URIs match the given exclude rules.\n"
    "                     This option is most helpful when shuffling songs\n"
    "                     with -f, that aren't in the MPD library.\n"
    "   -o,--only         Instead of continuously adding songs, just add\n"
    "                     'NUMBER' songs and then exit.\n"
    "   -p,--port         Specify a port number to connect to. Defaults to\n"
    "                     `6600`.\n"
    "   -q,--queue-buffer Specify to keep a buffer of `n` songs queued after\n"
    "                     the currently playing song. This is to support MPD\n"
    "                     features like crossfade that don't work if there\n"
    "                     are no more songs in the queue.\n"
    "See included `readme.md` file for PATTERN syntax.\n";

void options_help(FILE *output) { fputs(HELP_MESSAGE, output); }
