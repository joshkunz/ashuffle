#include <sys/types.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <absl/strings/str_format.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <mpd/idle.h>

#include "args.h"
#include "ashuffle.h"
#include "load.h"
#include "log.h"
#include "mpd.h"
#include "mpd_client.h"
#include "rule.h"
#include "shuffle.h"
#include "util.h"

namespace ashuffle {

namespace {

/* These MPD commands are required for ashuffle to run */
constexpr std::array<std::string_view, 5> kRequiredCommands = {
    "add", "status", "play", "pause", "idle",
};

absl::Status TryFirst(mpd::MPD *mpd, ShuffleChain *songs) {
    absl::StatusOr<std::unique_ptr<mpd::Status>> status = mpd->CurrentStatus();
    if (!status.ok()) {
        Log().Error("Failed to query current MPD Status: %s",
                    status.status().ToString());
        return status.status();
    }

    // No need to do anything if the player is already going.
    if ((*status)->IsPlaying()) {
        return absl::OkStatus();
    }

    // If we're not playing, then add a song, and start playing it.
    if (auto s = mpd->Add(songs->Pick()); !s.ok()) {
        Log().Error("Failed to add song to MPD: %s", s.ToString());
        return s;
    }
    // Passing the former queue length, because PlayAt is zero-indexed.
    if (auto s = mpd->PlayAt((*status)->QueueLength()); !s.ok()) {
        Log().Error("Failed to play newly added song: %s", s.ToString());
        return s;
    }
    return absl::OkStatus();
}

absl::Status TryEnqueue(mpd::MPD *mpd, ShuffleChain *songs,
                        const Options &options) {
    absl::StatusOr<std::unique_ptr<mpd::Status>> status_or =
        mpd->CurrentStatus();
    if (!status_or.ok()) {
        Log().Error("Failed to fetch MPD status");
        return status_or.status();
    }
    std::unique_ptr<mpd::Status> status = std::move(*status_or);

    // We're "past" the last song, if there is no current song position.
    bool past_last = !status->SongPosition().has_value();
    bool queue_empty = status->QueueLength() == 0;

    unsigned queue_songs_remaining = 0;
    if (!past_last) {
        /* +1 on song_pos because it is zero-indexed */
        queue_songs_remaining =
            (status->QueueLength() - (*status->SongPosition() + 1));
    }

    bool should_add = false;
    if (past_last) {
        /* Always add if we've progressed past the last song. Even if
         * --queue_buffer, we should have already enqueued a song by now. */
        should_add = true;
    } else if (queue_songs_remaining < options.queue_buffer) {
        /* If a queue buffer is set, check to see how any songs are left. If
         * we're past the end of our queue buffer, allow enquing a song. */
        should_add = true;
    } else if (queue_empty) {
        /* If the queue is totally empty, enqueue. */
        should_add = true;
    }

    /* Add another song to the list and restart the player */
    if (should_add) {
        if (options.queue_buffer != 0) {
            int needed = static_cast<int>(options.queue_buffer) -
                         static_cast<int>(queue_songs_remaining);
            // If we're not currently "on" a song, then we need to not only
            // enqueue options->queue_buffer songs, but also the song we're
            // about to play, so increment the `to_enqueue' count by one.
            if (past_last || queue_empty) {
                needed += 1;
            }
            while (needed > 0) {
                std::vector<std::string> picked = songs->Pick();
                needed -= static_cast<int>(picked.size());
                if (auto status = mpd->Add(picked); !status.ok()) {
                    Log().Error("Failed to add picked song: %s",
                                status.ToString());
                    return status;
                }
            }
        } else {
            if (auto status = mpd->Add(songs->Pick()); !status.ok()) {
                Log().Error("Failed to add picked song: %s", status.ToString());
                return status;
            }
        }
    }

    /* If we added a song, and the player was not already playing, we need
     * to re-start it. */
    if (should_add && (past_last || queue_empty)) {
        /* Since the 'status' was before we added our song, and the queue
         * is zero-indexed, the length will be the position of the song we
         * just added. Play that song */
        if (auto s = mpd->PlayAt(status->QueueLength()); !s.ok()) {
            Log().Error("Failed to start newly enqueued song: %s",
                        s.ToString());
            return s;
        }
        /* Immediately pause playback if mpd single mode is on */
        if (status->Single()) {
            if (auto status = mpd->Pause(); !status.ok()) {
                Log().Error("Failed to stop playback in single mode: %s",
                            status.ToString());
                return status;
            }
        }
    }
    return absl::OkStatus();
}

void PromptPassword(mpd::MPD *mpd, std::function<std::string()> &getpass_f) {
    /* keep looping till we get a bad error, or we get a good password. */
    while (true) {
        using status = mpd::MPD::PasswordStatus;
        std::string pass = getpass_f();
        absl::StatusOr<status> result = mpd->ApplyPassword(pass);
        if (!result.ok()) {
            std::cerr << "Failed to apply password:" << result.status()
                      << std::endl;
            continue;
        }
        if (*result == status::kAccepted) {
            return;
        }
        fputs("incorrect password.\n", stderr);
    }
}

struct MPDHost {
    std::string host;
    std::optional<std::string> password;

    MPDHost(std::string_view in) {
        std::size_t idx = in.find("@");
        if (idx != std::string_view::npos) {
            password = in.substr(0, idx);
            host = in.substr(idx + 1, in.size() - idx);
        } else {
            host = in;
        }
    }
};

std::optional<std::unique_ptr<Loader>> Reloader(mpd::MPD *mpd,
                                                const Options &options) {
    // Nothing we can do when `--file` is provided. The user is just stuck
    // with whatever URIs we parsed the first time.
    if (options.file_in != nullptr) {
        return std::nullopt;
    }
    return std::make_unique<MPDLoader>(mpd, options.ruleset, options.group_by);
}

}  // namespace

/* Keep adding songs when the queue runs out */
absl::Status Loop(mpd::MPD *mpd, ShuffleChain *songs, const Options &options,
                  TestDelegate test_d) {
    static_assert(MPD_IDLE_QUEUE == MPD_IDLE_PLAYLIST,
                  "QUEUE Now different signal.");
    mpd::IdleEventSet set(MPD_IDLE_DATABASE, MPD_IDLE_QUEUE, MPD_IDLE_PLAYER);

    // If the test delegate's `skip_init` is set to true, then skip the
    // initializer.
    if (options.tweak.play_on_startup) {
        if (auto status = TryFirst(mpd, songs); !status.ok()) {
            return status;
        }
        if (auto status = TryEnqueue(mpd, songs, options); !status.ok()) {
            return status;
        }
    }

    // Tracks if we should be enqueuing new songs.
    bool active = true;

    // Loop forever if test delegates are not set.
    while (test_d.until_f == nullptr || test_d.until_f()) {
        /* wait till the player state changes */
        absl::StatusOr<mpd::IdleEventSet> events = mpd->Idle(set);
        if (!events.ok()) {
            Log().Error("Failed to idle for MPD events: %s",
                        events.status().ToString());
            return events.status();
        }

        if (events->Has(MPD_IDLE_DATABASE) && options.tweak.exit_on_db_update) {
            std::cout << "Database updated, exiting." << std::endl;
            std::exit(0);
        }

        /* Only update the database if our original list was built from
         * MPD. */
        if (events->Has(MPD_IDLE_DATABASE) && options.file_in == nullptr) {
            std::optional<std::unique_ptr<Loader>> reloader =
                Reloader(mpd, options);
            if (reloader.has_value()) {
                songs->Clear();
                (*reloader)->Load(songs);
                PrintChainLength(std::cout, *songs);
            }
        } else if (events->Has(MPD_IDLE_QUEUE) ||
                   events->Has(MPD_IDLE_PLAYER)) {
            if (options.tweak.suspend_timeout != absl::ZeroDuration()) {
                auto status_or = mpd->CurrentStatus();
                if (!status_or.ok()) {
                    Log().Error("Failed to fetch status in suspend handler");
                    return status_or.status();
                }
                std::unique_ptr<mpd::Status> status = std::move(*status_or);
                if (status->QueueLength() == 0) {
                    test_d.sleep_f(options.tweak.suspend_timeout);
                    auto status_or = mpd->CurrentStatus();
                    if (!status_or.ok()) {
                        Log().Error(
                            "Failed to fetch status in suspend handler");
                        return status_or.status();
                    }
                    status = std::move(*status_or);
                    active = status->QueueLength() == 0;
                }
            }
            if (!active) {
                continue;
            }
            if (auto status = TryEnqueue(mpd, songs, options); !status.ok()) {
                Log().Error("Failed regular enqueue");
                return status;
            }
        }
    }
    return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<mpd::MPD>> Connect(
    const mpd::Dialer &d, const Options &options,
    std::function<std::string()> &getpass_f) {
    /* Attempt to get host from command line if available. Otherwise use
     * MPD_HOST variable if available. Otherwise use 'localhost'. */
    const char *env_host =
        getenv("MPD_HOST") != nullptr ? getenv("MPD_HOST") : "localhost";
    std::string mpd_host_raw =
        options.host.has_value() ? *options.host : env_host;
    MPDHost mpd_host(mpd_host_raw);

    /* Same thing for the port, use the command line defined port, environment
     * defined, or the default port */
    unsigned mpd_port =
        options.port
            ? options.port
            : (unsigned)(getenv("MPD_PORT") ? atoi(getenv("MPD_PORT")) : 6600);

    mpd::Address addr = {
        .host = mpd_host.host,
        .port = mpd_port,
    };

    absl::StatusOr<std::unique_ptr<mpd::MPD>> r = d.Dial(addr);
    if (!r.ok()) {
        Log().Error("Failed to connect to mpd: %s", r.status().ToString());
        return r.status();
    }
    std::unique_ptr<mpd::MPD> mpd = std::move(*r);

    /* Password Workflow:
     * 1. If the user supplied a password, then apply it. No matter what.
     * 2. Check if we can execute all required commands. If not then:
     *  2.a Fail if the user gave us a password that didn't work.
     *  2.b Prompt the user to enter a password, and try again.
     * 3. If the user successfully entered a password, then check that all
     *    required commands can be executed again. If we still can't execute
     *    all required commands, then fail. */
    if (mpd_host.password) {
        // We don't actually care if the password was accepted here. We still
        // need to check the available commands either way.
        (void)mpd->ApplyPassword(*mpd_host.password);
    }

    // Need a vector for the types to match up.
    const std::vector<std::string_view> required(kRequiredCommands.begin(),
                                                 kRequiredCommands.end());
    absl::StatusOr<mpd::MPD::Authorization> auth = mpd->CheckCommands(required);
    if (!auth.ok()) {
        Log().Error("Failed to check required commands: %s",
                    auth.status().ToString());
        return auth.status();
    }
    if (!mpd_host.password && !auth->authorized) {
        // If the user did *not* supply a password, and we are missing a
        // required command, then try to prompt the user to provide a password.
        // Once we get/apply a password, try the required commands again...
        PromptPassword(mpd.get(), getpass_f);
        auth = mpd->CheckCommands(required);
        if (!auth.ok()) {
            Log().Error("Failed to check required commands: %s",
                        auth.status().ToString());
            return auth.status();
        }
    }
    // If we still can't connect, inform the user which commands are missing,
    // and exit.
    if (!auth->authorized) {
        std::cerr << "Missing MPD Commands:" << std::endl;
        for (std::string &cmd : auth->missing) {
            std::cerr << "  " << cmd << std::endl;
        }
        Die("password applied, but required command still not allowed.");
    }
    return mpd;
}

// Print the size of the database to the given stream, accounting for grouping.
void PrintChainLength(std::ostream &stream, const ShuffleChain &songs) {
    if (songs.Len() == 0) {
        stream << "Song pool is empty." << std::endl;
        return;
    }

    // If we're grouping.
    if (songs.Len() != songs.LenURIs()) {
        stream << absl::StrFormat("Picking from %u groups (%u songs).",
                                  songs.Len(), songs.LenURIs())
               << std::endl;
    } else {
        stream << "Picking random songs out of a pool of " << songs.Len() << "."
               << std::endl;
    }
}

}  // namespace ashuffle
