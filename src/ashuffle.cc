#include <sys/types.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <absl/strings/str_format.h>
#include <mpd/idle.h>

#include "args.h"
#include "ashuffle.h"
#include "load.h"
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

void TryFirst(mpd::MPD *mpd, ShuffleChain *songs) {
    std::unique_ptr<mpd::Status> status = mpd->CurrentStatus();
    // No need to do anything if the player is already going.
    if (status->IsPlaying()) {
        return;
    }

    // If we're not playing, then add a song, and start playing it.
    mpd->Add(songs->Pick());
    // Passing the former queue length, because PlayAt is zero-indexed.
    mpd->PlayAt(status->QueueLength());
}

void TryEnqueue(mpd::MPD *mpd, ShuffleChain *songs, const Options &options) {
    std::unique_ptr<mpd::Status> status = mpd->CurrentStatus();

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
            unsigned to_enqueue = options.queue_buffer;
            // If we're not currently "on" a song, then we need to not only
            // enqueue options->queue_buffer songs, but also the song we're
            // about to play, so increment the `to_enqueue' count by one.
            if (past_last || queue_empty) {
                to_enqueue += 1;
            }
            for (unsigned i = queue_songs_remaining; i < to_enqueue; i++) {
                mpd->Add(songs->Pick());
            }
        } else {
            mpd->Add(songs->Pick());
        }
    }

    /* If we added a song, and the player was not already playing, we need
     * to re-start it. */
    if (should_add && (past_last || queue_empty)) {
        /* Since the 'status' was before we added our song, and the queue
         * is zero-indexed, the length will be the position of the song we
         * just added. Play that song */
        mpd->PlayAt(status->QueueLength());
        /* Immediately pause playback if mpd single mode is on */
        if (status->Single()) {
            mpd->Pause();
        }
    }
}

void PromptPassword(mpd::MPD *mpd, std::function<std::string()> &getpass_f) {
    /* keep looping till we get a bad error, or we get a good password. */
    while (true) {
        using status = mpd::MPD::PasswordStatus;
        std::string pass = getpass_f();
        if (mpd->ApplyPassword(pass) == status::kAccepted) {
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

}  // namespace

/* Keep adding songs when the queue runs out */
void Loop(mpd::MPD *mpd, ShuffleChain *songs, const Options &options,
          TestDelegate test_d) {
    static_assert(MPD_IDLE_QUEUE == MPD_IDLE_PLAYLIST,
                  "QUEUE Now different signal.");
    mpd::IdleEventSet set(MPD_IDLE_DATABASE, MPD_IDLE_QUEUE, MPD_IDLE_PLAYER);

    // If the test delegate's `skip_init` is set to true, then skip the
    // initializer.
    if (!test_d.skip_init) {
        TryFirst(mpd, songs);
        TryEnqueue(mpd, songs, options);
    }

    // Loop forever if test delegates are not set.
    while (test_d.until_f == nullptr || test_d.until_f()) {
        /* wait till the player state changes */
        mpd::IdleEventSet events = mpd->Idle(set);
        /* Only update the database if our original list was built from
         * MPD. */
        if (events.Has(MPD_IDLE_DATABASE) && options.file_in == nullptr) {
            songs->Clear();
            MPDLoader loader(mpd, options.ruleset);
            loader.Load(songs);
            printf("Picking random songs out of a pool of %u.\n", songs->Len());
        } else if (events.Has(MPD_IDLE_QUEUE) || events.Has(MPD_IDLE_PLAYER)) {
            TryEnqueue(mpd, songs, options);
        }
    }
}

std::unique_ptr<mpd::MPD> Connect(const mpd::Dialer &d, const Options &options,
                                  std::function<std::string()> &getpass_f) {
    /* Attempt to get host from command line if available. Otherwise use
     * MPD_HOST variable if available. Otherwise use 'localhost'. */
    std::string mpd_host_raw =
        options.host.has_value()
            ? *options.host
            : getenv("MPD_HOST") ? getenv("MPD_HOST") : "localhost";
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

    mpd::Dialer::result r = d.Dial(addr);

    if (std::string *err = std::get_if<std::string>(&r); err != nullptr) {
        Die("Failed to connect to mpd: %s", *err);
    }
    std::unique_ptr<mpd::MPD> mpd =
        std::move(std::get<std::unique_ptr<mpd::MPD>>(r));

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
    mpd::MPD::Authorization auth = mpd->CheckCommands(required);
    if (!mpd_host.password && !auth.authorized) {
        // If the user did *not* supply a password, and we are missing a
        // required command, then try to prompt the user to provide a password.
        // Once we get/apply a password, try the required commands again...
        PromptPassword(mpd.get(), getpass_f);
        auth = mpd->CheckCommands(required);
    }
    // If we still can't connect, inform the user which commands are missing,
    // and exit.
    if (!auth.authorized) {
        std::cerr << "Missing MPD Commands:" << std::endl;
        for (std::string &cmd : auth.missing) {
            std::cerr << "  " << cmd << std::endl;
        }
        Die("password applied, but required command still not allowed.");
    }
    return mpd;
}

}  // namespace ashuffle
