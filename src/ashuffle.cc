#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>

#include <mpd/client.h>

#include "args.h"
#include "ashuffle.h"
#include "getpass.h"
#include "rule.h"
#include "shuffle.h"
#include "util.h"

/* 25 seconds is the default timeout */
static const int TIMEOUT = 25000;

/* The size of the rolling shuffle window */
const int WINDOW_SIZE = 7;

/* These MPD commands are required for ashuffle to run */
constexpr std::array<std::string_view, 5> kRequiredCommands = {
    "add", "status", "play", "pause", "idle",
};

void mpd_perror(struct mpd_connection *mpd) {
    assert(mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS &&
           "must be an error present");
    die("MPD error: %s", mpd_connection_get_error_message(mpd));
}

void mpd_perror_if_error(struct mpd_connection *mpd) {
    if (mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        mpd_perror(mpd);
    }
}

/* check wheter a song is allowed by the given ruleset */
bool ruleset_accepts_song(const std::vector<Rule> &ruleset,
                          struct mpd_song *song) {
    for (const Rule &rule : ruleset) {
        if (!rule.Accepts(song)) {
            return false;
        }
    }
    return true;
}

bool ruleset_accepts_uri(struct mpd_connection *mpd,
                         const std::vector<Rule> &ruleset, char *uri) {
    bool accepted = false;
    /* search for the song URI in MPD */
    mpd_search_db_songs(mpd, true);
    mpd_search_add_uri_constraint(mpd, MPD_OPERATOR_DEFAULT, uri);
    if (mpd_search_commit(mpd) != true) {
        mpd_perror(mpd);
    }

    struct mpd_song *song = mpd_recv_song(mpd);
    mpd_perror_if_error(mpd);
    if (song != NULL) {
        if (ruleset_accepts_song(ruleset, song)) {
            accepted = true;
        }

        /* free the song we got from MPD */
        mpd_song_free(song);

        /* even though we're searching for a single song, libmpdclient
         * still acts like we're reading a song list. We read an aditional
         * element to convince MPD this is the end of the song list. */
        (void)mpd_recv_song(mpd);
    } else {
        fprintf(stderr, "Song uri '%s' not found.\n", uri);
    }

    return accepted;
}

static std::vector<std::string> mpd_song_uri_list(struct mpd_connection *mpd) {
    /* ask for a list of songs */
    if (mpd_send_list_all_meta(mpd, NULL) != true) {
        mpd_perror(mpd);
    }

    std::vector<std::string> result;

    /* parse out the pairs */
    struct mpd_song *song = mpd_recv_song(mpd);
    const enum mpd_error err = mpd_connection_get_error(mpd);
    if (err == MPD_ERROR_CLOSED) {
        die("MPD server closed the connection while getting the list of\n"
            "all songs. If MPD error logs say \"Output buffer is full\",\n"
            "consider setting max_output_buffer_size to a higher value\n"
            "(e.g. 32768) in your MPD config.");
    } else if (err != MPD_ERROR_SUCCESS) {
        mpd_perror(mpd);
    }
    for (; song; song = mpd_recv_song(mpd)) {
        result.push_back(mpd_song_get_uri(song));

        /* free the current song */
        mpd_song_free(song);
    }
    return result;
}

/* build the list of songs to shuffle from using
 * the supplied file. */
int build_songs_file(struct mpd_connection *mpd,
                     const std::vector<Rule> &ruleset, FILE *input,
                     ShuffleChain *songs, bool check) {
    char *uri = NULL;
    ssize_t length = 0;
    size_t ignored = 0;
    std::vector<std::string> all_uris;

    if (check) {
        all_uris = mpd_song_uri_list(mpd);
        std::sort(all_uris.begin(), all_uris.end());
    }

    length = getline(&uri, &ignored, input);
    while (!feof(input) && !ferror(input)) {
        if (length < 1) {
            die("invalid URI in input stream");
        }

        /* if this line has terminating newline attached, set it
         * to null (effectively removing the newline). */
        if (uri[length - 1] == '\n') {
            length -= 1;
            uri[length] = '\0';
        }

        if (check) {
            if (all_uris.empty()) {
                // No URIs in MPD, so the song can't possibly exist.
                goto skip_uri;
            }
            if (!std::binary_search(all_uris.begin(), all_uris.end(), uri)) {
                // We have some uris in `all_uris', but the given URI
                // is not in there. Skip this URI.
                goto skip_uri;
            }
            if (!ruleset.empty() && !ruleset_accepts_uri(mpd, ruleset, uri)) {
                // User-specified some rules, and they don't match this URI,
                // so skip this uri.
                goto skip_uri;
            }
        }

        songs->Add(std::string(uri));

    skip_uri:

        /* free the temporary memory */
        free(uri);
        uri = NULL;

        /* get the next uri */
        length = getline(&uri, &ignored, input);
    }
    // Free any memory allocated by our final getline.
    if (uri != NULL) {
        free(uri);
    }

    return 0;
}

/* build the list of songs to shuffle from using MPD */
int build_songs_mpd(struct mpd_connection *mpd,
                    const std::vector<Rule> &ruleset, ShuffleChain *songs) {
    /* ask for a list of songs */
    if (mpd_send_list_all_meta(mpd, NULL) != true) {
        mpd_perror(mpd);
    }

    /* parse out the pairs */
    struct mpd_song *song = mpd_recv_song(mpd);
    const enum mpd_error err = mpd_connection_get_error(mpd);
    if (err == MPD_ERROR_CLOSED) {
        die("MPD server closed the connection while getting the list of\n"
            "all songs. If MPD error logs say \"Output buffer is full\",\n"
            "consider setting max_output_buffer_size to a higher value\n"
            "(e.g. 32768) in your MPD config.");
    } else if (err != MPD_ERROR_SUCCESS) {
        mpd_perror(mpd);
    }
    for (; song; song = mpd_recv_song(mpd)) {
        /* if this song is allowed, add it to the list */
        if (ruleset_accepts_song(ruleset, song)) {
            songs->Add(mpd_song_get_uri(song));
        }
        /* free the current song */
        mpd_song_free(song);
    }
    return 0;
}

/* Append a random song from the given list of
 * songs to the queue */
void shuffle_single(struct mpd_connection *mpd, ShuffleChain *songs) {
    if (mpd_run_add(mpd, songs->Pick().c_str()) != true) {
        mpd_perror(mpd);
    }
}

int try_first(struct mpd_connection *mpd, ShuffleChain *songs) {
    struct mpd_status *status;
    status = mpd_run_status(mpd);
    if (status == NULL) {
        puts(mpd_connection_get_error_message(mpd));
        return -1;
    }

    if (mpd_status_get_state(status) != MPD_STATE_PLAY) {
        shuffle_single(mpd, songs);
        if (!mpd_run_play_pos(mpd, mpd_status_get_queue_length(status))) {
            mpd_perror(mpd);
        }
    }

    mpd_status_free(status);
    return 0;
}

int try_enqueue(struct mpd_connection *mpd, ShuffleChain *songs,
                const Options &options) {
    struct mpd_status *status = mpd_run_status(mpd);

    /* Check for error while fetching the status */
    if (status == NULL) {
        /* print the error message from the server */
        puts(mpd_connection_get_error_message(mpd));
        return -1;
    }

    bool past_last = mpd_status_get_song_pos(status) == -1;
    bool queue_empty = mpd_status_get_queue_length(status) == 0;

    unsigned queue_songs_remaining = 0;
    if (!past_last) {
        /* +1 on song_pos because it is zero-indexed */
        queue_songs_remaining = (mpd_status_get_queue_length(status) -
                                 (mpd_status_get_song_pos(status) + 1));
    }

    bool should_add = false;
    if (past_last) {
        /* Always add if we've progressed past the last song. Even if
         * --queue_buffer, we should have already enqueued a song by now. */
        should_add = true;
    } else if (options.queue_buffer != 0 &&
               queue_songs_remaining < options.queue_buffer) {
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
                shuffle_single(mpd, songs);
            }
        } else {
            shuffle_single(mpd, songs);
        }
    }

    /* If we added a song, and the player was not already playing, we need
     * to re-start it. */
    if (should_add && (past_last || queue_empty)) {
        /* Since the 'status' was before we added our song, and the queue
         * is zero-indexed, the length will be the position of the song we
         * just added. Play that song */
        if (!mpd_run_play_pos(mpd, mpd_status_get_queue_length(status))) {
            mpd_perror(mpd);
        }
        /* Immediately pause playback if mpd single mode is on */
        if (mpd_status_get_single(status)) {
            if (mpd_run_pause(mpd, true)) {
                mpd_perror(mpd);
            }
        }
    }

    /* free the status we retrieved */
    mpd_status_free(status);
    return 0;
}

/* Keep adding songs when the queue runs out */
int shuffle_loop(struct mpd_connection *mpd, ShuffleChain *songs,
                 const Options &options, struct shuffle_test_delegate *test_d) {
    static_assert(MPD_IDLE_QUEUE == MPD_IDLE_PLAYLIST,
                  "QUEUE Now different signal.");
    enum mpd_idle idle_mask =
        (enum mpd_idle)(MPD_IDLE_DATABASE | MPD_IDLE_QUEUE | MPD_IDLE_PLAYER);

    // If the test delegate's `skip_init` is set to true, then skip the
    // initializer.
    if (!test_d || !test_d->skip_init) {
        if (try_first(mpd, songs) != 0) {
            return -1;
        }
        if (try_enqueue(mpd, songs, options) != 0) {
            return -1;
        }
    }

    // Loop forever if test delegates are not set.
    while (test_d == NULL || test_d->until_f()) {
        /* wait till the player state changes */
        enum mpd_idle event = mpd_run_idle_mask(mpd, idle_mask);
        mpd_perror_if_error(mpd);
        bool idle_db = !!(event & MPD_IDLE_DATABASE);
        bool idle_queue = !!(event & MPD_IDLE_QUEUE);
        bool idle_player = !!(event & MPD_IDLE_PLAYER);
        /* Only update the database if our original list was built from
         * MPD. */
        if (idle_db && options.file_in == NULL) {
            songs->Empty();
            build_songs_mpd(mpd, options.ruleset, songs);
            printf("Picking random songs out of a pool of %u.\n", songs->Len());
        } else if (idle_queue || idle_player) {
            if (try_enqueue(mpd, songs, options) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static char *default_getpass() {
    return as_getpass(stdin, stdout, "mpd password: ");
}

static void get_mpd_password(struct mpd_connection *mpd, char *(*getpass_f)()) {
    char *(*do_getpass)() = getpass_f;
    if (do_getpass == NULL) {
        do_getpass = default_getpass;
    }
    /* keep looping till we get a bad error, or we get a good password. */
    while (true) {
        char *pass = do_getpass();
        mpd_run_password(mpd, pass);
        const enum mpd_error err = mpd_connection_get_error(mpd);
        if (err == MPD_ERROR_SUCCESS) {
            return;
        } else if (err == MPD_ERROR_SERVER) {
            enum mpd_server_error server_err =
                mpd_connection_get_server_error(mpd);
            if (server_err != MPD_SERVER_ERROR_PASSWORD) {
                mpd_perror(mpd);
            }
            mpd_connection_clear_error(mpd);
            fprintf(stderr, "incorrect password.\n");
        } else {
            mpd_perror(mpd);
        }
    }
}

/* If a password is required, "password" is used if not null, otherwise
 * a password is obtained from stdin. */
bool is_mpd_password_needed(struct mpd_connection *mpd) {
    // Fetch a list of the commands we're not allowed to run. In most
    // installs, this should be empty.
    if (!mpd_send_disallowed_commands(mpd)) {
        mpd_perror(mpd);
    }
    std::vector<std::string> disallowed_commands;
    struct mpd_pair *command = mpd_recv_command_pair(mpd);
    while (command != NULL) {
        disallowed_commands.push_back(command->value);
        mpd_return_pair(mpd, command);
        command = mpd_recv_command_pair(mpd);
    }
    mpd_perror_if_error(mpd);

    bool password_needed = false;
    for (std::string_view cmd : kRequiredCommands) {
        if (std::find(disallowed_commands.begin(), disallowed_commands.end(),
                      cmd) != disallowed_commands.end()) {
            fprintf(stderr, "required MPD command \"%s\" not allowed by MPD.\n",
                    cmd.data());
            password_needed = true;
            break;
        }
    }
    return password_needed;
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

struct mpd_connection *ashuffle_connect(const Options &options,
                                        char *(*getpass_f)()) {
    struct mpd_connection *mpd;

    /* Attempt to get host from command line if available. Otherwise use
     * MPD_HOST variable if available. Otherwise use 'localhost'. */
    std::string mpd_host_raw =
        options.host.has_value()
            ? *options.host
            : getenv("MPD_HOST") ? getenv("MPD_HOST") : xstrdup("localhost");
    MPDHost mpd_host(mpd_host_raw);

    /* Same thing for the port, use the command line defined port, environment
     * defined, or the default port */
    unsigned mpd_port =
        options.port
            ? options.port
            : (unsigned)(getenv("MPD_PORT") ? atoi(getenv("MPD_PORT")) : 6600);

    /* Create a new connection to mpd */
    mpd = mpd_connection_new(mpd_host.host.data(), mpd_port, TIMEOUT);

    if (mpd == NULL) {
        die("Could not connect due to lack of memory.");
    } else if (mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        die("Could not connect to %s:%u.", mpd_host.host.data(), mpd_port);
    }

    /* Password Workflow:
     * 1. If the user supplied a password, then apply it. No matter what.
     * 2. Check if we can execute all required commands. If not then:
     *  2.a Fail if the user gave us a password that didn't work.
     *  2.b Prompt the user to enter a password, and try again.
     * 3. If the user successfully entered a password, then check that all
     *    required commands can be executed again. If we still can't execute
     *    all required commands, then fail. */
    if (mpd_host.password.has_value()) {
        mpd_run_password(mpd, mpd_host.password->data());
        mpd_perror_if_error(mpd);
    }
    bool need_mpd_password = is_mpd_password_needed(mpd);
    if (mpd_host.password.has_value() && need_mpd_password) {
        die("password applied, but required command still not allowed.");
    }
    if (need_mpd_password) {
        get_mpd_password(mpd, getpass_f);
    }
    if (is_mpd_password_needed(mpd)) {
        die("password applied, but required command still not allowed.");
    }
    return mpd;
}
