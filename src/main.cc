#include <stdlib.h>
#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <mpd/client.h>

#include "args.h"
#include "ashuffle.h"
#include "getpass.h"
#include "rule.h"
#include "shuffle.h"

int main(int argc, const char *argv[]) {
    std::variant<Options, ParseError> parse = Options::ParseFromC(argv, argc);
    if (ParseError *err = std::get_if<ParseError>(&parse); err != nullptr) {
        switch (err->type) {
            case ParseError::Type::kUnknown:
                fprintf(stderr,
                        "unknown option parsing error. Please file a bug "
                        "at https://github.com/joshkunz/ashuffle");
                break;
            case ParseError::Type::kHelp:
                // We always print the help, so just break here.
                break;
            case ParseError::Type::kGeneric:
                fprintf(stderr, "error: %s\n", err->msg.data());
                break;
            default:
                assert(false && "unreachable");
        }
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
    }

    Options options = std::get<Options>(parse);

    /* attempt to connect to MPD */
    struct mpd_connection *mpd = ashuffle_connect(options, NULL);

    ShuffleChain songs(WINDOW_SIZE);

    /* build the list of songs to shuffle through */
    if (options.file_in != NULL) {
        build_songs_file(mpd, options.ruleset, options.file_in, &songs,
                         options.check_uris);
        fclose(options.file_in);
    } else {
        build_songs_mpd(mpd, options.ruleset, &songs);
    }

    // For integration testing, we sometimes just want to have ashuffle
    // dump the list of songs in its shuffle chain.
    if (options.test.print_all_songs_and_exit) {
        std::vector<std::string> all_songs = songs.Items();
        for (auto song : all_songs) {
            std::cout << song << std::endl;
        }
        return 0;
    }

    if (songs.Len() == 0) {
        puts("Song pool is empty.");
        return -1;
    }
    printf("Picking random songs out of a pool of %u.\n", songs.Len());

    /* Seed the random number generator */
    srand(time(NULL));

    /* do the main action */
    if (options.queue_only) {
        for (unsigned i = 0; i < options.queue_only; i++) {
            shuffle_single(mpd, &songs);
        }
        printf("Added %u songs.\n", options.queue_only);
    } else {
        shuffle_loop(mpd, &songs, options, NULL);
    }

    mpd_connection_free(mpd);
    return 0;
}
