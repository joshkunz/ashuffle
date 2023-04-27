#include <stdlib.h>
#include <time.h>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include <absl/time/clock.h>
#include <mpd/connection.h>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "args.h"
#include "ashuffle.h"
#include "getpass.h"
#include "load.h"
#include "log.h"
#include "mpd_client.h"
#include "shuffle.h"
#include "version.h"

using namespace ashuffle;

// This is the maximum amount of time that ashuffle is allowed to be
// disconnected from MPD before it exits. This catches cases where the
// environment changes and it would be impossible for ashuffle to reconnect.
const absl::Duration kMaxDisconnectedTime = absl::Seconds(10);

// The amount of time to wait between reconnection attempts.
const absl::Duration kReconnectWait = absl::Milliseconds(250);

namespace {
std::unique_ptr<Loader> BuildLoader(mpd::MPD* mpd, const Options& opts) {
    if (opts.file_in != nullptr && opts.check_uris) {
        return std::make_unique<FileMPDLoader>(mpd, opts.ruleset, opts.group_by,
                                               opts.file_in);
    } else if (opts.file_in != nullptr) {
        return std::make_unique<FileLoader>(opts.file_in);
    }

    return std::make_unique<MPDLoader>(mpd, opts.ruleset, opts.group_by);
}

void LoopOnce(mpd::MPD* mpd, ShuffleChain& songs, const Options& options) {
    absl::Time start = absl::Now();
    absl::Status status = Loop(mpd, &songs, options);
    absl::Duration loop_length = absl::Now() - start;
    if (!status.ok()) {
        Log().Error("LOOP failed after %s with error: %s",
                    absl::FormatDuration(loop_length), status.ToString());
    } else {
        Log().Info("LOOP exited successfully after %s (probably a bug)",
                   absl::FormatDuration(loop_length));
    }
}

}  // namespace

int main(int argc, const char* argv[]) {
    std::variant<Options, ParseError> parse =
        Options::ParseFromC(*mpd::client::Parser(), argv, argc);
    if (ParseError* err = std::get_if<ParseError>(&parse); err != nullptr) {
        switch (err->type) {
            case ParseError::Type::kVersion:
                // Don't print help in this case, since the user specifically
                // requested we print the version.
                std::cout << "ashuffle version: " << kVersion << std::endl;
                exit(EXIT_SUCCESS);
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
        }
        std::cerr << DisplayHelp;
        exit(EXIT_FAILURE);
    }

    Options options = std::move(std::get<Options>(parse));

    if (!options.check_uris && !options.group_by.empty()) {
        std::cerr << "-g/--group-by not supported with no-check" << std::endl;
        exit(EXIT_FAILURE);
    }

    log::SetOutput(std::cerr);

    bool disable_reconnect = false;
    std::function<std::string()> pass_f = [&disable_reconnect] {
        disable_reconnect = true;
        std::string pass = GetPass(stdin, stdout, "mpd password: ");
        Log().InfoStr(
            "Disabling reconnect support since the password was "
            "provided interactively. Supply password via MPD_HOST "
            "environment variable to enable automatic "
            "reconnects");
        return pass;
    };

    /* attempt to connect to MPD */
    absl::StatusOr<std::unique_ptr<mpd::MPD>> mpd =
        Connect(*mpd::client::Dialer(), options, &pass_f);
    if (!mpd.ok()) {
        Die("Failed to connect to mpd: %s", mpd.status().ToString());
    }

    ShuffleChain songs((size_t)options.tweak.window_size);

    {
        // We construct the loader in a new scope, since loaders can
        // consume a lot of memory.
        std::unique_ptr<Loader> loader = BuildLoader(mpd->get(), options);
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
        PrintChainLength(std::cerr, songs);
        exit(EXIT_FAILURE);
    }

    PrintChainLength(std::cout, songs);

    if (options.queue_only) {
        size_t number_of_songs = 0;
        for (unsigned i = 0; i < options.queue_only; i++) {
            auto& picked_songs = songs.Pick();
            number_of_songs += picked_songs.size();
            if (auto status = (*mpd)->Add(picked_songs); !status.ok()) {
                Die("Failed to enqueue songs: %s", status.ToString());
            }
        }

        /* print number of songs or groups (and songs) added */
        std::cout << absl::StrFormat(
            "Added %u %s%s", options.queue_only,
            options.group_by.empty() ? "song" : "group",
            options.queue_only > 1 ? "s" : "");
        if (!options.group_by.empty()) {
            std::cout << absl::StrFormat(" (%u songs)", number_of_songs);
        }
        std::cout << "." << std::endl;
        exit(EXIT_SUCCESS);
        return 0;
    }

    LoopOnce(mpd->get(), songs, options);
    if (disable_reconnect) {
        exit(EXIT_FAILURE);
    }

    absl::Time disconnect_begin = absl::Now();
    while ((absl::Now() - disconnect_begin) < kMaxDisconnectedTime) {
        mpd = Connect(*mpd::client::Dialer(), options, kNonInteractiveGetpass);
        if (!mpd.ok()) {
            Log().Error("Failed to reconnect to MPD %s, been waiting %s",
                        mpd.status().ToString(),
                        absl::FormatDuration(absl::Now() - disconnect_begin));

            absl::SleepFor(kReconnectWait);
            continue;
        }

        if (auto l = Reloader(mpd->get(), options); l.has_value()) {
            (*l)->Load(&songs);
            PrintChainLength(std::cout, songs);
        }

        LoopOnce(mpd->get(), songs, options);

        // Re-set the disconnection timer after we successfully reconnect.
        disconnect_begin = absl::Now();
    }
    Log().Error("Could not reconnect after %s, aborting.",
                absl::FormatDuration(kMaxDisconnectedTime));

    exit(EXIT_FAILURE);
}
