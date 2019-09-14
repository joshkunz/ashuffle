#include <stdlib.h>

#include <mpd/client.h>

#include "args.h"
#include "ashuffle.h"
#include "getpass.h"
#include "list.h"
#include "rule.h"
#include "shuffle.h"

int main(int argc, const char *argv[]) {
    /* attempt to parse out options given on the command line */
    struct ashuffle_options options;
    options_init(&options);
    struct options_parse_result parse_r = options_parse(&options, argc, argv);
    if (parse_r.status != PARSE_OK) {
        if (parse_r.msg != NULL) {
            fprintf(stderr, "error: %s\n", parse_r.msg);
        }
        options_help(stderr);
        exit(1);
    }
    options_parse_result_free(&parse_r);

    /* attempt to connect to MPD */
    struct mpd_connection *mpd = ashuffle_connect(&options, NULL);

    struct shuffle_chain songs;
    shuffle_init(&songs, WINDOW_SIZE);

    /* build the list of songs to shuffle through */
    if (options.file_in != NULL) {
        build_songs_file(mpd, &options.ruleset, options.file_in, &songs,
                         options.check_uris);
        fclose(options.file_in);
    } else {
        build_songs_mpd(mpd, &options.ruleset, &songs);
    }

    // For integration testing, we sometimes just want to have ashuffle
    // dump the list of songs in its shuffle chain.
    if (options.test.print_all_songs_and_exit) {
        struct list all_songs;
        list_init(&all_songs);
        shuffle_items(&songs, &all_songs);
        for (unsigned i = 0; i < all_songs.length; i++) {
            puts(list_at_str(&all_songs, i));
        }
        return 0;
    }

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
            shuffle_single(mpd, &songs);
        }
        printf("Added %u songs.\n", options.queue_only);
    } else {
        shuffle_loop(mpd, &songs, &options, NULL);
    }

    /* dispose of the rules used to build the song-list */
    for (unsigned i = 0; i < options.ruleset.length; i++) {
        rule_free(list_at(&options.ruleset, i)->data);
    }
    list_free(&options.ruleset);

    /* free-up our songs */
    shuffle_free(&songs);
    mpd_connection_free(mpd);
    return 0;
}
