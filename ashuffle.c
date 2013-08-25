#include <mpd/client.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#include "array.h"
#include "rule.h"

/* 25 seconds is the default timeout */
#define TIMEOUT 25000

struct ashuffle_options {
    struct auto_array ruleset;
    unsigned queue_only;
};

/* case-insensitive string matching */
bool istrmatch(const char * str1, const char * str2) {
    return strcasecmp(str1, str2) == 0;
}

/* Append a random song to the queue */
void queue_random_song(struct mpd_connection * mpd, 
                       struct auto_array * songs) {
    mpd_run_add(mpd, songs->array[rand() % songs->length]);
}

/* Keep adding songs when the queue runs out */
int shuffle_idle(struct mpd_connection * mpd, struct auto_array * songs) {
    struct mpd_status * status;
    while (true) {
        mpd_send_status(mpd);
        status = mpd_recv_status(mpd);

        /* If the player is on its last song *or* not
         * not reporting a song as playing */
        if (mpd_status_get_song_pos(status) == 
            ((int) mpd_status_get_queue_length(status)) - 1) {
            /* If the player is stopped and doesn't have any songs
             * in its queue, then add a song and start the player,
             * otherwise, leave the queue as it is. */
            if (mpd_status_get_state(status) == MPD_STATE_STOP &&
                mpd_status_get_queue_length(status) == 0) {
                queue_random_song(mpd, songs);
                mpd_run_play(mpd);
            } else {
                queue_random_song(mpd, songs);
            }
        }

        /* wait till the player state changes */
        mpd_run_idle_mask(mpd, MPD_IDLE_PLAYER);
    }
    return 0;
}

/* build our list of songs to shuffle from */
int build_song_list(struct mpd_connection * mpd, 
                    struct auto_array * ruleset, 
                    struct auto_array * songs) {
    /* ask for a list of songs */
    mpd_send_list_all_meta(mpd, NULL);

    /* parse out the pairs */
    struct mpd_song * song = mpd_recv_song(mpd);
    struct song_rule * rule = NULL;
    while (song) {
        bool song_ok = true;
        for (unsigned i = 0; i < ruleset->length; i++) {
            rule = ruleset->array[i];
            if (! rule_match(rule, song)) {
                song_ok = false;
                break;
            }
        }
        if (song_ok) {
            array_append_s(songs, mpd_song_get_uri(song), strlen(mpd_song_get_uri(song)) + 1);
        }
        song = mpd_recv_song(mpd);
    } 

    /* trim any excess storage */
    array_trim(songs);
    return 0;
}

int rule_type_from_flag(char * option) {
    if (istrmatch("--include", option) || istrmatch("-i", option)) {
        return RULE_INCLUDE;
    } else if (istrmatch("--exclude", option) || istrmatch("-e", option)) {
        return RULE_EXCLUDE;
    } else {
        return -1;
    }
}


int parse_options(struct ashuffle_options * opts, int argc, char * argv[]) {
    /* State for the state machine */
    enum parse_state {
        START, RULE, RULE_VALUE, QUEUE
    } state = START;

    char * match_field;
    struct song_rule rule;

    int type_flag = -1;
    
    for (int i = 1; i < argc; i++) {
        if (state == START || state == RULE) {
            type_flag = rule_type_from_flag(argv[i]);
        }
        
        if ((state == START || state == RULE) && type_flag != -1) {
            if (state == RULE_VALUE) {
                fprintf(stderr, "Rule match for '%s' before argument '%s' is not complete "
                                "(it needs a value to match).\n", match_field, argv[i]);
                return -1;
            }

            if (state != START) {
                /* add the rule to the array */
                array_append(&opts->ruleset, &rule, sizeof(struct song_rule));
            }
            rule_init(&rule, type_flag);
            state = RULE;
        /* if we're in a valid location *and* we don't already have an option,
         * then try and get a queue number */
        } else if ((state == START || state == RULE) && opts->queue_only == 0
                && (istrmatch("--only", argv[i]) || istrmatch("-o", argv[i]))) {
            /* close of the current rule */
            if (state == RULE) {
                array_append(&opts->ruleset, &rule, sizeof(struct song_rule));
            }
            state = QUEUE;
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
            state = START;
        } else {
            fprintf(stderr, "Invalid option: %s.\n", argv[i]);
            return -1;
        }
    }

    if (state == RULE_VALUE) {
        fprintf(stderr, "No value supplied for match '%s'.\n", match_field);
        return -1;
    /* if we're provisioning a rule right now, flush it */
    } else if (state == RULE) {
        array_append(&opts->ruleset, &rule, sizeof(struct song_rule));
    }
    return 0;
}

void fhelp(FILE * output) {
    fputs(
    "usage: ashuffle [-i PATTERN ...] ...\n"
    "                [-e PATTERN ...] ...\n"
    "\n"
    "Optional Arguments:\n"
    "   -i,--include  Specify things include in shuffle (think whitelist).\n"
    "   -e,--exclude  Specify things to remove from shuffle (think blacklist).\n"
    "\n"
    "PATTERN\n"
    "A pattern is made up of two components, a field to check,\n"
    "followed by a value to match. The field is a tag indexed\n"
    "by MPD (for example artist, title, album), and value is a\n"
    "case-insensitve string that will act as a wildcard matcher.\n"
    "For example, if you wanted to exclude all songs from\n"
    "Girl Talk's 'Secret Diary' album you could supply this pattern\n"
    "as an argument:\n"
    "   $ ashuffle --exclude artist girl album \"secret diary\"\n",
    output);
}

int main (int argc, char * argv[]) {
    struct mpd_connection *mpd;
    struct ashuffle_options options;
    array_init(&options.ruleset);
    int status = parse_options(&options, argc, argv);
    if (status != 0) { fhelp(stderr); return status; }

    /* Attempt to use MPD_HOST variable if available.
     * Otherwise use 'localhost'. */
    char * mpd_host = getenv("MPD_HOST") ? 
                        getenv("MPD_HOST") : "localhost";
    /* Same thing for the port, use the environment defined port
     * or the default port */
    unsigned mpd_port = (unsigned) (getenv("MPD_PORT") ? 
                            atoi(getenv("MPD_PORT")) : 6600);

    /* Create a new connection to mpd */
    mpd = mpd_connection_new(mpd_host, mpd_port, TIMEOUT);
    
    if (mpd == NULL) {
        fputs("Could not connect due to lack of memory.", stderr);
        return 1;
    } else if (mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "Could not connect to %s:%u.\n", mpd_host, mpd_port);
        return 1;
    }
     
    /* Auto-expanding array to hold songs */
    struct auto_array songs;
    array_init(&songs);

    build_song_list(mpd, &options.ruleset, &songs);

    /* Seed the random number generator */
    srand(time(NULL));
    if (options.queue_only) {
        for (unsigned i = 0; i < options.queue_only; i++) {
            queue_random_song(mpd, &songs);
        }
        printf("Added %u songs.\n", options.queue_only);
    } else {
        shuffle_idle(mpd, &songs);
    }

    array_free(&songs);
    mpd_connection_free(mpd);
    return 0;
}
