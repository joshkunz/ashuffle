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
#include "load.h"
#include "mpd_client.h"
#include "shuffle.h"

using namespace ashuffle;

namespace {

// The size of the rolling shuffle window.
const int kWindowSize = 7;

std::unique_ptr<Loader> BuildLoader(mpd::MPD* mpd, const Options& opts) {
    if (opts.file_in != nullptr && opts.check_uris) {
        return std::make_unique<CheckFileLoader>(mpd, opts.ruleset,
                                                 opts.file_in);
    } else if (opts.file_in != nullptr) {
        return std::make_unique<FileLoader>(opts.file_in);
    }

    return std::make_unique<MPDLoader>(mpd, opts.ruleset);
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::variant<Options, ParseError> parse =
        Options::ParseFromC(*mpd::client::Parser(), argv, argc);
    if (ParseError* err = std::get_if<ParseError>(&parse); err != nullptr) {
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
        std::cerr << DisplayHelp;
        exit(EXIT_FAILURE);
    }

    Options options = std::move(std::get<Options>(parse));

    std::function<std::string()> pass_f = [] {
        return GetPass(stdin, stdout, "mpd password: ");
    };
    /* attempt to connect to MPD */
    std::unique_ptr<mpd::MPD> mpd =
        Connect(*mpd::client::Dialer(), options, pass_f);

    ShuffleChain songs(kWindowSize);

    {
        // We construct the loader in a new scope, since loaders can
        // consume a lot of memory.
        std::unique_ptr<Loader> loader = BuildLoader(mpd.get(), options);
        loader->Load(&songs);
    }

    // For integration testing, we sometimes just want to have ashuffle
    // dump the list of songs in its shuffle chain.
    if (options.test.print_all_songs_and_exit) {
        std::vector<std::string> all_songs = songs.Items();
        for (auto song : all_songs) {
            std::cout << song << std::endl;
        }
        exit(EXIT_SUCCESS);
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
        Loop(mpd.get(), &songs, options);
    }

    return 0;
}
