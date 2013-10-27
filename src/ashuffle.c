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

    /* whether or not we should queue a song */
    bool queue_enabled = false;
    while (true) {
        mpd_send_status(mpd);
        status = mpd_recv_status(mpd);

        /* Check for error while fetching the status */
        if (status == NULL) { 
            /* print the error message from the server */
            puts(mpd_connection_get_error_message(mpd));
            /* kill the loop */
            break;
        }

        /* If the currently playing song is the last song in the list,
         * then when MPD stops playing, add another song to the list and
         * restart the player */
        if (mpd_status_get_song_pos(status) ==
            (int) (mpd_status_get_queue_length(status) - 1)) {
            queue_enabled = true;
        } 

        /* if MPD has stoppped playing and the last playing song was the last song
         * in the list, then add another song and keep playing */
        if (queue_enabled && mpd_status_get_state(status) == MPD_STATE_STOP) {
            queue_random_song(mpd, songs);
            /* Since the 'status' was before we added our song, and the queue
             * is zero-indexed, the length will be the position of the song we
             * just added. Play that song */
            mpd_run_play_pos(mpd, mpd_status_get_queue_length(status));
        } 

        /* free the status we retrieved */
        mpd_status_free(status);

        /* wait till the player state changes */
        mpd_run_idle_mask(mpd, MPD_IDLE_PLAYER);
    }
    return 0;
}

/* check wheter a song is allowed by the given ruleset */
bool ruleset_accepts_song(struct auto_array * ruleset, struct mpd_song * song) {
    struct song_rule * rule = NULL;
    for (unsigned i = 0; i < ruleset->length; i++) {
        rule = ruleset->array[i];
        if (! rule_match(rule, song)) {
            return false;
        }
    }
    return true;
}

bool ruleset_accepts_uri(struct mpd_connection * mpd, 
                         struct auto_array * ruleset, char * uri) {

    bool accepted = false;
    /* search for the song URI in MPD */
    mpd_search_db_songs(mpd, true);
    mpd_search_add_uri_constraint(mpd, MPD_OPERATOR_DEFAULT, uri);
    mpd_search_commit(mpd);

    struct mpd_song * song = mpd_recv_song(mpd);
    if (song != NULL) {
        if (ruleset_accepts_song(ruleset, song)) {
            accepted = true;
        }

        /* free the song we got from MPD */
        mpd_song_free(song);

        /* even though we're searching for a single song, libmpdclient
         * still acts like we're reading a song list. We read an aditional
         * element to convince MPD this is the end of the song list. */
        song = mpd_recv_song(mpd);
    } else {
        fprintf(stderr, "Song uri '%s' not found.\n", uri);
    }

    return accepted;
}


/* build the list of songs to shuffle from using
 * the supplied file. */
int build_songs_file(struct mpd_connection * mpd, struct auto_array * ruleset,
                     FILE * input, struct shuffle_chain * songs, bool check) {
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

        if ((check && ruleset_accepts_uri(mpd, ruleset, uri)) || (! check)) {
            shuffle_add(songs, uri, length + 1);
        }

        /* free the temporary memory */
        free(uri); uri = NULL;

        /* get the next uri */
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
    while (song) {
        /* if this song is allowed, add it to the list */
        if (ruleset_accepts_song(ruleset, song)) {
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
        build_songs_file(mpd, &options.ruleset, options.file_in, 
                         &songs, options.check_uris);
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
