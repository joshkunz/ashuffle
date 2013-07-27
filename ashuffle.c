#include <mpd/client.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* 25 seconds is the default timeout */
#define TIMEOUT 25000

struct auto_array {
    unsigned length;
    unsigned alloc_length;
    char ** array;
};

/* initialize the auto array */
int array_init(struct auto_array * ar) {
    ar->length = 0;
    ar->alloc_length = 0;
    ar->array = NULL;
    return 0;
}

/* Append the string to the array */
int array_append(struct auto_array * ar, const char * astr, unsigned astrlen) {
    /* if this array is full, reallocate it larger */
    if (ar->length == ar->alloc_length) {
        ar->alloc_length = ar->length + 1 + (ar->length / 3);
        ar->array = realloc(ar->array, sizeof(char *) * ar->alloc_length);
    }
    ar->array[ar->length] = malloc(sizeof(char) * astrlen);
    /* Copy the string into the buffer */
    memcpy(ar->array[ar->length], astr, astrlen);
    /* Increment the length of the array */
    ar->length++;
    return 0;
}

/* Free the entire array */
int array_free(struct auto_array * ar) {
    for (unsigned i = 0; i < ar->length; i++) {
        free(ar->array[i]);
    }
    free(ar->array);
    array_init(ar);
    return 0;
}

/* Trim the auto_allocated array to the minumim size */
int array_trim(struct auto_array * ar) {
    if (ar->length == ar->alloc_length) { return 0; }
    /* save a reference to the array */
    char ** tmp = ar->array;
    /* Allocate a minimum sized array */
    ar->array = malloc(ar->length * sizeof(char *));
    /* Copy the old contents */
    memcpy(ar->array, tmp, ar->length * sizeof(char *));
    /* re-set the allocated length */
    ar->alloc_length = ar->length;
    /* free the old array */
    free(tmp);
    return 0;
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
        if (mpd_status_get_next_song_id(status) -1) {
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

int main (int argc, char * argv[]) {
    struct mpd_connection *mpd;

    unsigned queue_only = 0;
    if (argc > 1) {
        queue_only = atoi(argv[1]);
    }

    /* Attempt to use MPD_HOST variable if available.
     * Otherwise use 'localhost'. */
    char * mpd_host = getenv("MPD_HOST") ? 
                        getenv("MPD_HOST") : "localhost";
    /* Same thing for the port, use the environment defined port
     * or the default port */
    unsigned mpd_port = (unsigned) getenv("MPD_PORT") ? 
                            atoi(getenv("MPD_PORT")) : 6600;

    /* Create a new connection to mpd */
    mpd = mpd_connection_new(mpd_host, mpd_port, TIMEOUT);
    if (mpd == NULL) {
        fprintf(stderr, "Could not connect to %s:%u.\n", mpd_host, mpd_port);
        return 1;
    }

    /* Auto-expanding array to hold songs */
    struct auto_array songs;
    array_init(&songs);

    /* Build the song list */
    mpd_send_list_all(mpd, NULL);
    struct mpd_pair *pair = mpd_recv_pair(mpd);
    while (pair) {
        if (strcmp("file", pair->name) == 0) {
            array_append(&songs, pair->value, strlen(pair->value) + 1);
        }
        mpd_return_pair(mpd, pair); 
        pair = mpd_recv_pair(mpd);
    } 
    array_trim(&songs);

    /* Seed the random number generator */
    srand(time(NULL));
    if (queue_only) {
        for (unsigned i = 0; i < queue_only; i++) {
            queue_random_song(mpd, &songs);
        }
        printf("Added %u songs.\n", queue_only);
    } else {
        shuffle_idle(mpd, &songs);
    }

    array_free(&songs);
    mpd_connection_free(mpd);
    return 0;
}
