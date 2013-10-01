#define _GNU_SOURCE

#include <mpd/client.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "shuffle.h"
#include "array.h"
#include "rule.h"
#include "args.h"

/* 25 seconds is the default timeout */
#define TIMEOUT 25000

/* The size of the rolling shuffle window */
#define WINDOW_SIZE 7

/* Append a random song fromt the given array of 
 * songs to the queue */
void queue_random_song(struct mpd_connection * mpd, 
                       struct shuffle_chain * songs) {
    mpd_run_add(mpd, shuffle_pick(songs));
}

/* Keep adding songs when the queue runs out */
int shuffle_idle(struct mpd_connection * mpd, 
                 struct shuffle_chain * songs) {
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

/* build the list of songs to shuffle from using
 * the supplied file. */
int build_songs_file(FILE * input, struct shuffle_chain * songs) {
    char * uri = NULL;
    ssize_t length = 0;
    size_t ignored = 0;
    length = getline(&uri, &ignored, input);
    while (! feof(input) && ! ferror(input)) {
        /* if this line has terminating newline attached, set it
         * to null and decrement the length (effectively removing
         * the newline). */
        if (uri[length - 1] == '\n') {
            uri[length - 1] = '\0';
            length -= 1;
        }

        /* add the song to the shuffle list */
        shuffle_add(songs, uri, length + 1);

        /* free the temporary memory */
        free(uri); uri = NULL;
        length = getline(&uri, &ignored, input);
    }
    fclose(input);
    return 0;
}


/* build the list of songs to shuffle from using MPD */
int build_songs_mpd(struct mpd_connection * mpd, 
                    struct auto_array * ruleset, 
                    struct shuffle_chain * songs) {
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
            shuffle_add(songs, mpd_song_get_uri(song),
                        strlen(mpd_song_get_uri(song)) + 1);
        }
        /* free the current song */
        mpd_song_free(song);

        /* get the next song from the list */
        song = mpd_recv_song(mpd);
    } 
    return 0;
}

int main (int argc, char * argv[]) {

    /* attempt to parse out options given on the command line */
    struct ashuffle_options options;
    ashuffle_init(&options);
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
    struct shuffle_chain songs;
    shuffle_init(&songs, WINDOW_SIZE);

    /* build the list of songs to shuffle through */
    if (options.file_in != NULL) {
        build_songs_file(options.file_in, &songs);
    } else {
        build_songs_mpd(mpd, &options.ruleset, &songs);
    }

    /* dispose of the rules used to build the song-list */
    for (unsigned i = 0; i < options.ruleset.length; i++) {
        rule_free(options.ruleset.array[i]);
    }
    array_free(&options.ruleset);

    if (shuffle_length(&songs) == 0) {
        puts("Song pool is empty.");
        return -1;
    } 
    printf("Picking random songs out of a pool of %u.\n", 
           shuffle_length(&songs));

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
    shuffle_free(&songs);
    mpd_connection_free(mpd);
    return 0;
}
