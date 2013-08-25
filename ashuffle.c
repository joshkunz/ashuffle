#include <mpd/client.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "array.h"
#include "rule.h"
#include "args.h"

/* 25 seconds is the default timeout */
#define TIMEOUT 25000

/* Append a random song fromt the given array of 
 * songs to the queue */
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
        /* attempt to find a rule that doesn't allow
         * the current song */
        for (unsigned i = 0; i < ruleset->length; i++) {
            rule = ruleset->array[i];
            if (! rule_match(rule, song)) {
                song_ok = false;
                break;
            }
        }
        /* if this song is allowed, add it to the list */
        if (song_ok) {
            array_append_s(songs, 
                           mpd_song_get_uri(song), 
                           strlen(mpd_song_get_uri(song)) + 1);
        }
        song = mpd_recv_song(mpd);
    } 

    /* trim any excess storage */
    array_trim(songs);
    return 0;
}

int main (int argc, char * argv[]) {

    /* attempt to parse out options given on the command line */
    struct ashuffle_options options;
    array_init(&options.ruleset);
    int status = ashuffle_options(&options, argc, argv);
    if (status != 0) { ashuffle_help(stderr); return status; }

    /* attempt to connect to MPD */
    struct mpd_connection *mpd;

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

    /* dispose of the rules used to build the song-list */
    for (unsigned i = 0; i < options.ruleset.length; i++) {
        rule_free(options.ruleset.array[i]);
    }
    array_free(&options.ruleset);

    if (songs.length == 0) {
        puts("Song pool is empty.");
        return -1;
    } 
    printf("Picking random songs out of a pool of %u.\n", songs.length);


    /* Seed the random number generator */
    srand(time(NULL));

    /* do the main action */
    if (options.queue_only) {
        for (unsigned i = 0; i < options.queue_only; i++) {
            queue_random_song(mpd, &songs);
        }
        printf("Added %u songs.\n", options.queue_only);
    } else {
        shuffle_idle(mpd, &songs);
    }

    /* free-up our songs */
    array_free(&songs);
    mpd_connection_free(mpd);
    return 0;
}
