#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <vector>
#include <algorithm>
#include <string>

#include <mpd/client.h>
#include <tap.h>

#include "args.h"
#include "ashuffle.h"
#include "rule.h"
#include "shuffle.h"
#include "util.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

void xclearenv() {
    if (clearenv()) {
        perror("xclearenv");
        abort();
    }
}

void xsetenv(const char *key, const char *value) {
    if (setenv(key, value, 1) != 0) {
        perror("xsetenv");
        abort();
    }
}

void list_push_song(struct list *l, struct mpd_song *s) {
    struct datum d = {
        .length = sizeof(struct mpd_song),
        .data = (void *)s,
    };
    list_push(l, &d);
}

void list_push_empty(struct list *l) {
    struct datum d = {
        .length = 0,
        .data = NULL,
    };
    list_push(l, &d);
}

void list_push_pair(struct list *l, struct mpd_pair *p) {
    struct datum d = {
        .length = sizeof(struct mpd_pair),
        .data = p,
    };
    list_push(l, &d);
}

void test_build_songs_mpd_basic() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);
    list_push_song(&c.song_iter, &song_a);
    list_push_song(&c.song_iter, &song_b);

    int result = build_songs_mpd(&c, ruleset, &chain);
    cmp_ok(result, "==", 0, "build_songs_mpd basic returns ok");
    cmp_ok(chain.Len(), "==", 2,
           "build_songs_mpd_basic: 2 songs added to shuffle chain");

    mpd_connection_free(&c);
}

void test_build_songs_mpd_filter() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    Rule artist_match;
    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    // Exclude all songs with the artist "__not_artist__".
    artist_match.AddPattern("artist", "__not_artist__");
    ruleset.push_back(artist_match);

    TEST_SONG(song_a, TAG(MPD_TAG_ARTIST, "__artist__"));
    TEST_SONG(song_b, TAG(MPD_TAG_ARTIST, "__not_artist__"));
    TEST_SONG(song_c, TAG(MPD_TAG_ARTIST, "__artist__"));

    list_push_song(&c.song_iter, &song_a);
    list_push_song(&c.song_iter, &song_b);
    list_push_song(&c.song_iter, &song_c);

    int result = build_songs_mpd(&c, ruleset, &chain);
    cmp_ok(result, "==", 0, "build_songs_mpd filter returns ok");
    cmp_ok(chain.Len(), "==", 2,
           "build_songs_mpd_filter: 2 songs added to shuffle chain");

    mpd_connection_free(&c);
}

void xfwrite(FILE *f, const char *msg) {
    if (!fwrite(msg, strlen(msg), 1, f)) {
        perror("couldn't write to file");
        abort();
    }
}

void xfwriteln(FILE *f, const char *msg) {
    // +2 for \n\0
    char *nl = (char *) xmalloc(strlen(msg) + 2);
    strcpy(nl, msg);
    strncat(nl, "\n", 2);
    xfwrite(f, nl);
    free(nl);
}

void test_build_songs_file_nocheck() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    mpd_connection_set_error(
        &c, MPD_ERROR_ARGUMENT,
        "ashuffle should not dial MPD when check is false");

    const unsigned window_size = 3;
    ShuffleChain chain(window_size);

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);
    TEST_SONG_URI(song_c);

    FILE *f = tmpfile();
    if (f == NULL) {
        perror("couldn't open tmpfile");
        abort();
    }

    xfwriteln(f, song_a.uri);
    xfwriteln(f, song_b.uri);
    xfwriteln(f, song_c.uri);

    // rewind, so build_songs_file can see the URIs we've written.
    rewind(f);

    std::vector<Rule> empty_ruleset;
    int result = build_songs_file(&c, empty_ruleset, f, &chain, false);
    cmp_ok(result, "==", 0, "build_songs_file nocheck returns ok");
    cmp_ok(chain.Len(), "==", 3,
           "build_songs_file_nocheck: 3 songs added to shuffle chain");

    // To make sure we parsed the file correctly, pick three songs out of the
    // shuffle chain, and make sure they match the three URIs we wrote. This
    // should be stable because we set a window size equal to the number of
    // song URIs, and sort the URIs we receive from shuffle_pick.
    std::vector<std::string> want = {song_a.uri, song_b.uri, song_c.uri};
    std::vector<std::string> got;
    got.push_back(chain.Pick());
    got.push_back(chain.Pick());
    got.push_back(chain.Pick());

    std::sort(want.begin(), want.end());
    std::sort(got.begin(), got.end());

    assert(want.size() == window_size &&
           "number of wanted URIs should match the window size");

    ok(want == got, "build_songs_file_nocheck, want == got"); 

    // tmpfile is automatically cleaned up here.
    fclose(f);

    mpd_connection_free(&c);
}

void test_build_songs_file_check() {
    // step 1. Initialize the MPD connection.
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    // step 2. Build the ruleset, and add an exclusions for __not_artist__
    std::vector<Rule> ruleset;

    Rule artist_match;
    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    // Exclude all songs with the artist "__not_artist__".
    artist_match.AddPattern("artist", "__not_artist__");
    ruleset.push_back(artist_match);

    // step 3. Prepare the shuffle_chain.
    const unsigned window_size = 2;
    ShuffleChain chain(window_size);

    // step 4. Prepare our songs/song list. The song_list will be used for
    // subsequent calls to `mpd_recv_song`.
    TEST_SONG(song_a, TAG(MPD_TAG_ARTIST, "__artist__"));
    TEST_SONG(song_b, TAG(MPD_TAG_ARTIST, "__not_artist__"));
    TEST_SONG(song_c, TAG(MPD_TAG_ARTIST, "__artist__"));
    // This song will not be present in the MPD library, so it doesn't need
    // any tags.
    TEST_SONG_URI(song_d);

    // When matching songs, ashuffle will first query for a list of songs,
    // and then match against that static list. Only if a song is in the library
    // will it be matched against the ruleset (since matching requires
    // expensive MPD queries to resolve the URI).
    list_init(&c.song_iter);
    list_push_song(&c.song_iter, &song_a);
    list_push_song(&c.song_iter, &song_b);
    list_push_song(&c.song_iter, &song_c);
    // Don't push song_d, so we can validate that only songs in the MPD
    // library are allowed.
    // list_push_song(&c.song_iter, &song_d)

    // Empty to terminate the list of songs from the `listall' query.
    list_push_empty(&c.song_iter);

    // For the songs that are actually in the database, we will check
    // against the ruleset. When matching each song URI against the
    // rules, ashuffle executes mpd_recv_song twice. Once to receive
    // the actual song object from libmpdclient, and once to "finish"
    // the iteration. So, for each URI we want to test, we also need to
    // insert an empty entry after it.
    list_push_song(&c.song_iter, &song_a);
    list_push_empty(&c.song_iter);
    list_push_song(&c.song_iter, &song_b);
    list_push_empty(&c.song_iter);
    list_push_song(&c.song_iter, &song_c);
    list_push_empty(&c.song_iter);
    // No song_d here, since we should never query for it, it should be
    // filtered out earlier.

    // step 5. Set up our test input file, but writing the URIs of our songs.
    FILE *f = tmpfile();
    if (f == NULL) {
        perror("couldn't open tmpfile");
        abort();
    }

    xfwriteln(f, song_a.uri);
    xfwriteln(f, song_b.uri);
    xfwriteln(f, song_c.uri);
    // But we do want to write song_d here, so that ashuffle has to check it.
    xfwriteln(f, song_d.uri);

    // rewind, so build_songs_file can see the URIs we've written.
    rewind(f);

    // step 6. Run! (and validate)

    int result = build_songs_file(&c, ruleset, f, &chain, true);
    cmp_ok(result, "==", 0, "build_songs_file check returns ok");
    cmp_ok(chain.Len(), "==", 2,
           "build_songs_file_check: 2 songs added to shuffle chain");

    // This check works like the nocheck case, but instead of expecting us
    // to pick all 3 songs that were written into the input file, we only want
    // to pick song_a and song_c which are not excluded by the ruleset
    std::vector<std::string> want = {song_a.uri, song_c.uri};
    std::vector<std::string> got;
    got.push_back(chain.Pick());
    got.push_back(chain.Pick());

    std::sort(want.begin(), want.end());
    std::sort(got.begin(), got.end());

    assert(want.size() == window_size &&
           "number of wanted URIs should match the window size");

    ok(want == got, "build_songs_file_nocheck, want == got"); 

    // cleanup.
    fclose(f);

    mpd_connection_free(&c);
}

void test_shuffle_single() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);
    list_push_song(&c.db, &song_b);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    cmp_ok(c.queue.length, "==", 0,
           "shuffle_single: queue empty before song added");
    shuffle_single(&c, &chain);

    cmp_ok(c.queue.length, "==", 1,
           "shuffle_single: queue length 1 after song added");

    struct mpd_song *queue_head = (struct mpd_song *) list_at(&c.queue, 0)->data;
    is(queue_head->uri, song_a.uri,
       "shuffle_single: ensure that song_a was added");

    mpd_connection_free(&c);
}

// This function returns true, false, true, false... etc for each call. It
// can be used with shuffle_loop to make the internal loop run exactly once.
static bool once_f() {
    static unsigned count;
    return (count++ % 2) == 0;
}

// When used with shuffle_loop, this function will only allow the
// initialization code to run.
static bool only_init_f() { return false; }

void test_shuffle_loop_init_empty() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    struct shuffle_test_delegate delegate = {
        .skip_init = false,
        .until_f = only_init_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    cmp_ok(result, "==", 0, "shuffle_loop_init_empty: shuffle_loop returns 0");
    cmp_ok(c.queue.length, "==", 1,
           "shuffle_loop_init_empty: added one song to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_init_empty: playing after init");
    cmp_ok(c.state.queue_pos, "==", 0,
           "shuffle_loop_init_empty: queue position on first song");

    mpd_connection_free(&c);
    options_free(&options);
}

void test_shuffle_loop_init_playing() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    // Pretend like we already have a song in our queue, and we're playing.
    list_init(&c.queue);
    list_push_song(&c.queue, &song_a);

    c.state.play_state = MPD_STATE_PLAY;
    c.state.queue_pos = 0;

    struct shuffle_test_delegate delegate = {
        .skip_init = false,
        .until_f = only_init_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    // We shouldn't add anything to the queue if we're already playing,
    // ashuffle should start silently.
    cmp_ok(result, "==", 0,
           "shuffle_loop_init_playing: shuffle_loop returns 0");
    cmp_ok(c.queue.length, "==", 1,
           "shuffle_loop_init_playing: no songs added to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_init_playing: playing after init");
    cmp_ok(c.state.queue_pos, "==", 0,
           "shuffle_loop_init_playing: queue position on first song");

    mpd_connection_free(&c);
    options_free(&options);
}

void test_shuffle_loop_init_stopped() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);
    list_push_song(&c.db, &song_b);

    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    list_init(&c.queue);
    list_push_song(&c.queue, &song_b);
    c.state.queue_pos = 0;
    c.state.play_state = MPD_STATE_STOP;

    struct shuffle_test_delegate delegate = {
        .skip_init = false,
        .until_f = only_init_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    // We should add a new item to the queue, and start playing.
    cmp_ok(result, "==", 0,
           "shuffle_loop_init_stopped: shuffle_loop returns 0");
    cmp_ok(c.queue.length, "==", 2,
           "shuffle_loop_init_stopped: added one song to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_init_stopped: playing after init");
    cmp_ok(c.state.queue_pos, "==", 1,
           "shuffle_loop_init_stopped: queue position on second song");

    list_free(&c.db);
    list_free(&c.queue);
    options_free(&options);
}

void test_shuffle_loop_basic() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);
    list_push_song(&c.db, &song_b);

    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    list_init(&c.queue);
    list_push_song(&c.queue, &song_b);
    // If we've gone past the end of the queue, libmpdclient signals this
    // by setting the queue position to -1 (likely because it is unset in the
    // mpd status response).
    c.state.queue_pos = -1;
    c.state.play_state = MPD_STATE_STOP;

    // Make future IDLE calls return IDLE_QUEUE
    set_idle_result(MPD_IDLE_QUEUE);

    struct shuffle_test_delegate delegate = {
        .skip_init = true,
        .until_f = once_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    // We should add a new item to the queue, and start playing.
    cmp_ok(result, "==", 0, "shuffle_loop_basic: shuffle_loop returns 0");
    cmp_ok(c.queue.length, "==", 2,
           "shuffle_loop_basic: added one song to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_basic: playing after loop");
    cmp_ok(c.state.queue_pos, "==", 1,
           "shuffle_loop_basic: queue position on second song");

    // The currently playing item should be song_a (the only song in the
    // shuffle chain). If the mpd state is invalid, no playing song is returned,
    // and we skip this check.
    if (mpd_playing(&c)) {
        is(mpd_playing(&c)->uri, song_a.uri,
           "shuffle_loop_basic: queued and played song_a");
    }

    mpd_connection_free(&c);
    options_free(&options);
}

void test_shuffle_loop_empty() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    // Pretend like the queue was just emptied.
    list_init(&c.queue);
    c.state.queue_pos = 0;

    // Make future IDLE calls return IDLE_QUEUE
    set_idle_result(MPD_IDLE_QUEUE);

    struct shuffle_test_delegate delegate = {
        .skip_init = true,
        .until_f = once_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    // We should add a new item to the queue, and start playing.
    cmp_ok(result, "==", 0, "shuffle_loop_empty: shuffle_loop returns 0");
    cmp_ok(c.queue.length, "==", 1,
           "shuffle_loop_empty: added one song to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_empty: playing after loop");
    cmp_ok(c.state.queue_pos, "==", 0,
           "shuffle_loop_empty: queue position on first song");

    // The currently playing item should be song_a (the only song in the
    // shuffle chain). If the mpd state is invalid, no playing song is returned,
    // and we skip this check.
    if (mpd_playing(&c)) {
        is(mpd_playing(&c)->uri, song_a.uri,
           "shuffle_loop_empty: queued and played song_a");
    }

    mpd_connection_free(&c);
    options_free(&options);
}

void test_shuffle_loop_empty_buffer() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);
    options.queue_buffer = 3;

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    // Pretend like the queue was just emptied.
    list_init(&c.queue);
    c.state.queue_pos = -1;

    // Make future IDLE calls return IDLE_QUEUE
    set_idle_result(MPD_IDLE_QUEUE);

    struct shuffle_test_delegate delegate = {
        .skip_init = true,
        .until_f = once_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    // We should add 4 new items to the queue, and start playing on the first
    // one.
    cmp_ok(result, "==", 0,
           "shuffle_loop_empty_buffer: shuffle_loop returns 0");
    // queue_buffer + the currently playing song.
    cmp_ok(c.queue.length, "==", 4,
           "shuffle_loop_empty_buffer: added one song to queue");
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_empty_buffer: playing after loop");
    cmp_ok(c.state.queue_pos, "==", 0,
           "shuffle_loop_empty_buffer: queue position on first song");

    if (mpd_playing(&c)) {
        is(mpd_playing(&c)->uri, song_a.uri,
           "shuffle_loop_empty_buffer: queued and played song_a");
    }

    mpd_connection_free(&c);
    options_free(&options);
}

void test_shuffle_loop_buffer_partial() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);
    options.queue_buffer = 3;

    ShuffleChain chain;
    chain.Add(song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    // Pretend like the queue already has a few songs in it, and we're in
    // the middle of playing it. We normally don't need to do anything,
    // but we may need to update the queue buffer.
    list_init(&c.queue);
    list_push_song(&c.queue, &song_b);
    list_push_song(&c.queue, &song_b);
    list_push_song(&c.queue, &song_b);
    c.state.queue_pos = 1;
    c.state.play_state = MPD_STATE_PLAY;

    // Make future IDLE calls return IDLE_QUEUE
    set_idle_result(MPD_IDLE_QUEUE);

    struct shuffle_test_delegate delegate = {
        .skip_init = true,
        .until_f = once_f,
    };

    int result = shuffle_loop(&c, &chain, &options, &delegate);

    cmp_ok(result, "==", 0,
           "shuffle_loop_partial_buffer: shuffle_loop returns 0");
    // We had 3 songs in the queue, and we were playing the second song, so
    // we only need to add 2 more songs to fill out the queue buffer.
    cmp_ok(c.queue.length, "==", 5,
           "shuffle_loop_partial_buffer: added one song to queue");
    // We should still be playing the same song as before.
    cmp_ok(c.state.play_state, "==", MPD_STATE_PLAY,
           "shuffle_loop_partial_buffer: playing after loop");
    cmp_ok(c.state.queue_pos, "==", 1,
           "shuffle_loop_partial_buffer: queue position on the same song");

    if (mpd_playing(&c)) {
        is(mpd_playing(&c)->uri, song_b.uri,
           "shuffle_loop_partial_buffer: playing the same song as before");
    }

    mpd_connection_free(&c);
    options_free(&options);
}

static char *failing_getpass_f() {
    die("called failing getpass!");
    return NULL;  // unreachable
}

void test_connect_no_password() {
    // Make sure the environment doesn't influence the test.
    xclearenv();
    // Default host/port;
    mpd_set_server("localhost", 6600, 0);

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    struct mpd_pair not_needed = {"", "__uneeded__"};
    list_init(&c.pair_iter);
    list_push_pair(&c.pair_iter, &not_needed);

    mpd_set_connection(&c);

    struct mpd_connection *result = ashuffle_connect(&opts, failing_getpass_f);

    ok(result == &c, "connect_no_password: connection matches set connection");
    cmp_ok(c.error.error, "==", MPD_ERROR_SUCCESS,
           "connect_no_password: connection successful");

    mpd_connection_free(&c);
    options_free(&opts);
}

struct test_connect_host_port {
    const char *host;
    unsigned port;
};

void test_connect_parse_host() {
    struct host_port_case {
        // Want is used to set the actual server host/port.
        struct test_connect_host_port want;
        // Password is the password that will be set for the fake MPD server.
        const char *password;
        // Env are the values that will be stored in the MPD_* environment
        // variables. If they are NULL or 0, they will remain unset.
        struct test_connect_host_port env;
        // Flag are the values that will be given in flags. If they are NULL
        // or 0, the respective flag will not be set.
        struct test_connect_host_port flag;
    } cases[] = {
        // by default, connect to localhost:6600
        [0] =
            {
                .want = {"localhost", 6600},
                .password = NULL,
                .env = {NULL, 0},
                .flag = {NULL, 0},
            },
        // If only MPD_HOST is set with a password, and no MPD_PORT
        [1] =
            {
                .want = {"localhost", 6600},
                .password = "foo",
                .env = {"foo@localhost", 0},
                .flag = {NULL, 0},
            },
        // MPD_HOST with a domain-like string, and MPD_PORT is set.
        [2] =
            {
                .want = {"something.random.com", 123},
                .password = NULL,
                .env = {"something.random.com", 123},
                .flag = {NULL, 0},
            },
        // MPD_HOST is a unix socket, MPD_PORT unset.
        [3] =
            {
                // port is Needed for test, unused by libmpdclient
                .want = {"/test/mpd.socket", 6600},
                .password = NULL,
                .env = {"/test/mpd.socket", 0},
                .flag = {NULL, 0},
            },
        // MPD_HOST is a unix socket, with a password.
        [4] =
            {
                // port is Needed for test, unused by libmpdclient
                .want = {"/another/mpd.socket", 6600},
                .password = "with_pass",
                .env = {"with_pass@/another/mpd.socket", 0},
                .flag = {NULL, 0},
            },
        // --host example.com, port unset. environ unset.
        [5] =
            {
                .want = {"example.com", 6600},
                .password = NULL,
                .env = {NULL, 0},
                .flag = {"example.com", 0},
            },
        // --host some.host.com --port 5512, environ unset
        [6] =
            {
                .want = {"some.host.com", 5512},
                .password = NULL,
                .env = {NULL, 0},
                .flag = {"some.host.com", 5512},
            },
        // flag host, with password. environ unset.
        [7] =
            {
                .want = {"yet.another.host", 7781},
                .password = "secret_password",
                .env = {NULL, 0},
                .flag = {"secret_password@yet.another.host", 7781},
            },
        // Flags should override MPD_HOST and MPD_PORT environment variables.
        [8] =
            {
                .want = {"real.host", 1234},
                .password = NULL,
                .env = {"default.host", 6600},
                .flag = {"real.host", 1234},
            },
    };

    for (unsigned i = 0; i < STATIC_ARRAY_LEN(cases); i++) {
        xclearenv();
        mpd_set_server(cases[i].want.host, cases[i].want.port, 0);

        if (cases[i].env.host) {
            xsetenv("MPD_HOST", cases[i].env.host);
        }
        char *str_env_port = xsprintf("%u", cases[i].env.port);
        if (cases[i].env.port) {
            xsetenv("MPD_PORT", str_env_port);
        }

        struct list flags;
        list_init(&flags);

        if (cases[i].flag.host) {
            list_push_str(&flags, "--host");
            list_push_str(&flags, cases[i].flag.host);
        }
        if (cases[i].flag.port) {
            char *str_flag_port = xsprintf("%u", cases[i].flag.port);
            list_push_str(&flags, "--port");
            list_push_str(&flags, str_flag_port);
            free(str_flag_port);
        }

        struct ashuffle_options opts;
        options_init(&opts);

        if (flags.length > 0) {
            const char **arg_array = (const char **) xmalloc(sizeof(char *) * flags.length);
            for (unsigned j = 0; j < flags.length; j++) {
                arg_array[j] = list_at_str(&flags, j);
            }
            struct options_parse_result res;
            res = options_parse(&opts, flags.length, arg_array);
            cmp_ok(res.status, "==", PARSE_OK,
                   "connect_parse_host[%u]: failed to parse option flags", i);
            if (res.status != PARSE_OK) {
                diag("  parse result: %s", res.msg);
            }
            free(arg_array);
        }

        struct mpd_connection c;
        memset(&c, 0, sizeof(c));
        if (cases[i].password) {
            c.password = cases[i].password;
        }
        mpd_set_connection(&c);

        struct mpd_connection *result =
            ashuffle_connect(&opts, failing_getpass_f);
        ok(result == &c,
           "connect_parse_host[%u]: connection matches set connection", i);
        cmp_ok(c.error.error, "==", MPD_ERROR_SUCCESS,
               "connect_parse_host[%u]: connection successful", i);

        free(str_env_port);
        list_free(&flags);
        mpd_connection_free(&c);
        options_free(&opts);
    }
}

void test_connect_env_password() {
    xclearenv();
    // Default host/port;
    mpd_set_server("localhost", 6600, 0);

    // set our password in the environment
    xsetenv("MPD_HOST", "test_password@localhost");

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    c.password = "test_password";

    mpd_set_connection(&c);

    struct mpd_connection *result = ashuffle_connect(&opts, failing_getpass_f);

    ok(result == &c, "connect_env_password: connection matches set connection");
    cmp_ok(c.error.error, "==", MPD_ERROR_SUCCESS,
           "connect_env_password: connection successful");

    mpd_connection_free(&c);
    options_free(&opts);
}

static unsigned _GOOD_PASSWORD_COUNT = 0;

char *good_password_f() {
    _GOOD_PASSWORD_COUNT += 1;
    return xstrdup("good_password");
}

void test_connect_env_bad_password() {
    xclearenv();
    // Default host/port;
    mpd_set_server("localhost", 6600, 0);

    // set our password in the environment
    xsetenv("MPD_HOST", "bad_password@localhost");

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    c.password = "good_password";

    mpd_set_connection(&c);

    // using good_password_f, just in-case ashuffle_connect decides to prompt
    // for a password. It should fail without ever calling good_password_f.
    dies_ok({ (void)ashuffle_connect(&opts, good_password_f); },
            "connect_env_bad_password: fail to connect with bad password");

    mpd_connection_free(&c);
    options_free(&opts);
}

void test_connect_env_ok_password_bad_perms() {
    xclearenv();
    // Default host/port;
    mpd_set_server("localhost", 6600, 0);

    // set our password in the environment
    xsetenv("MPD_HOST", "good_password@localhost");

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    c.password = "good_password";

    // We have a good password, *but*, we're missing
    list_init(&c.pair_iter);
    struct mpd_pair status_pair = {"", "status"};
    list_push_pair(&c.pair_iter, &status_pair);

    mpd_set_connection(&c);

    // We should terminate after seeing the bad permissions. If we end up
    // re-prompting (and getting a good password), we should succeed, and fail
    // the test.
    dies_ok({ (void)ashuffle_connect(&opts, good_password_f); },
            "connect_env_ok_password_bad_perms: fail to connect with bad "
            "permissions");

    mpd_connection_free(&c);
    options_free(&opts);
}

// If no password is supplied in the environment, but we have a restricted
// command, then we should prompt for a user password. Once that password
// matches, *and* we don't have any more disallowed required commands, then
// we should be OK.
void test_connect_bad_perms_ok_prompt() {
    xclearenv();
    mpd_set_server("localhost", 6600, 0);

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    c.password = "good_password";

    list_init(&c.pair_iter);
    struct mpd_pair add_pair = {"", "add"};
    list_push_pair(&c.pair_iter, &add_pair);

    mpd_set_connection(&c);

    unsigned good_password_before = _GOOD_PASSWORD_COUNT;

    struct mpd_connection *result = ashuffle_connect(&opts, good_password_f);

    unsigned good_password_after = _GOOD_PASSWORD_COUNT;

    ok(result == &c,
       "connect_bad_perms_ok_prompt: connection matches set connection");
    cmp_ok(c.error.error, "==", MPD_ERROR_SUCCESS,
           "connect_bad_perms_ok_prompt: connection successful");
    cmp_ok(good_password_before, "==", good_password_after - 1,
           "connect_bad_perms_ok_prompt: password prompt should have been "
           "called once");

    mpd_connection_free(&c);
    options_free(&opts);
}

void test_connect_bad_perms_prompt_bad_perms() {
    xclearenv();
    mpd_set_server("localhost", 6600, 0);

    struct ashuffle_options opts;
    options_init(&opts);

    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    c.password = "good_password";

    list_init(&c.pair_iter);
    struct mpd_pair play_pair = {"", "play"};
    // Add the disabled command twice, so after the prompt, we still recognise
    // the pasword as invalid.
    list_push_pair(&c.pair_iter, &play_pair);
    // An empty entry stops processing, for the first check.
    list_push_empty(&c.pair_iter);
    list_push_pair(&c.pair_iter, &play_pair);

    mpd_set_connection(&c);

    dies_ok({ (void)ashuffle_connect(&opts, good_password_f); },
            "connect_bad_perms_ok_prompt: fails to connect");

    mpd_connection_free(&c);
    options_free(&opts);
}

int main() {
    plan(NO_PLAN);

    test_build_songs_mpd_basic();
    test_build_songs_mpd_filter();
    test_build_songs_file_nocheck();
    test_build_songs_file_check();
    test_shuffle_single();

    test_shuffle_loop_init_empty();
    test_shuffle_loop_init_playing();
    test_shuffle_loop_init_stopped();

    test_shuffle_loop_basic();
    test_shuffle_loop_empty();
    test_shuffle_loop_empty_buffer();
    test_shuffle_loop_buffer_partial();

    test_connect_no_password();
    test_connect_parse_host();
    test_connect_env_password();
    test_connect_env_bad_password();
    test_connect_env_ok_password_bad_perms();
    test_connect_bad_perms_ok_prompt();
    test_connect_bad_perms_prompt_bad_perms();

    done_testing();
}
