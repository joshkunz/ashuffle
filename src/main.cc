#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <mpd/connection.h>

#include "args.h"
#include "ashuffle.h"
#include "getpass.h"
#include "mpd_client.h"
#include "shuffle.h"

using namespace ashuffle;

int main(int argc, const char *argv[]) {
    std::variant<Options, ParseError> parse =
        Options::ParseFromC(*mpd::client::Parser(), argv, argc);
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

    std::function<std::string()> pass_f = [] {
        return GetPass(stdin, stdout, "mpd password: ");
    };
    /* attempt to connect to MPD */
    std::unique_ptr<mpd::MPD> mpd =
        ashuffle_connect(*mpd::client::Dialer(), options, pass_f);

    ShuffleChain songs(WINDOW_SIZE);

    /* build the list of songs to shuffle through */
    if (options.file_in != NULL) {
        build_songs_file(mpd.get(), options.ruleset, options.file_in, &songs,
                         options.check_uris);
        fclose(options.file_in);
    } else {
        build_songs_mpd(mpd.get(), options.ruleset, &songs);
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
        fputs("Song pool is empty.", stderr);
        exit(EXIT_FAILURE);
    }
    printf("Picking random songs out of a pool of %u.\n", songs.Len());

    /* Seed the random number generator */
    srand(time(NULL));

    /* do the main action */
    if (options.queue_only) {
        for (unsigned i = 0; i < options.queue_only; i++) {
            mpd->Add(songs.Pick());
        }
        printf("Added %u songs.\n", options.queue_only);
    } else {
        shuffle_loop(mpd.get(), &songs, options);
    }

    return 0;
}
