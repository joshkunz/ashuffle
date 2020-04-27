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
        return std::make_unique<FileMPDLoader>(mpd, opts.ruleset, opts.group_by,
                                               opts.file_in);
    } else if (opts.file_in != nullptr) {
        return std::make_unique<FileLoader>(opts.file_in);
    }

    return std::make_unique<MPDLoader>(mpd, opts.ruleset, opts.group_by);
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::variant<Options, ParseError> parse =
        Options::ParseFromC(*mpd::client::Parser(), argv, argc);
    if (ParseError* err = std::get_if<ParseError>(&parse); err != nullptr) {
        switch (err->type) {
            case ParseError::Type::kUnknown:
                std::cerr << "unknown option parsing error. Please file a bug "
                          << "at https://github.com/joshkunz/ashuffle"
                          << std::endl;
                break;
            case ParseError::Type::kHelp:
                // We always print the help, so just break here.
                break;
            case ParseError::Type::kGeneric:
                std::cerr << "error: " << err->msg << std::endl;
                break;
            default:
                assert(false && "unreachable");
        }
        std::cerr << DisplayHelp;
        exit(EXIT_FAILURE);
    }

    Options options = std::move(std::get<Options>(parse));

    if (!options.check_uris && !options.group_by.empty()) {
        std::cerr << "-g/--group-by not supported with no-check" << std::endl;
        exit(EXIT_FAILURE);
    }

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
        bool first = true;
        for (auto&& group : songs.Items()) {
            if (!first) {
                std::cout << "---" << std::endl;
            }
            first = false;
            for (auto&& song : group) {
                std::cout << song << std::endl;
            }
        }
        exit(EXIT_SUCCESS);
    }

    if (songs.Len() == 0) {
        std::cerr << "Song pool is empty." << std::endl;
        exit(EXIT_FAILURE);
    }

    if (!options.group_by.empty()) {
        std::cout << absl::StrFormat("Picking from %u groups (%u songs).",
                                     songs.Len(), songs.LenURIs())
                  << std::endl;
    } else {
        std::cout << "Picking random songs out of a pool of " << songs.Len()
                  << "." << std::endl;
    }

    /* Seed the random number generator */
    srand(time(NULL));

    /* do the main action */
    if (options.queue_only) {
        for (unsigned i = 0; i < options.queue_only; i++) {
            mpd->Add(songs.Pick());
        }
        std::cout << "Added " << options.queue_only << " songs." << std::endl;
    } else {
        Loop(mpd.get(), &songs, options);
    }

    return 0;
}
