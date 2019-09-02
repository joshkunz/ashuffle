#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "args.h"
#include "ashuffle.h"
#include "rule.h"
#include "shuffle.h"
#include "util.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

// Main tests
// connect
// shuffle_single
// shuffle_idle
//  + try_first
//  + try_enqueue

void xclearenv() {
    if (clearenv()) {
        perror("xclearenv");
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

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    struct list ruleset;
    list_init(&ruleset);

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);
    list_push_song(&c.song_iter, &song_a);
    list_push_song(&c.song_iter, &song_b);

    int result = build_songs_mpd(&c, &ruleset, &chain);
    cmp_ok(result, "==", 0, "build_songs_mpd basic returns ok");
    cmp_ok(shuffle_length(&chain), "==", 2,
           "build_songs_mpd_basic: 2 songs added to shuffle chain");

    mpd_connection_free(&c);
    shuffle_free(&chain);
    list_free(&ruleset);
}

void test_build_songs_mpd_filter() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    struct list ruleset;
    list_init(&ruleset);

    struct song_rule artist_match;
    rule_init(&artist_match);
    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    // Exclude all songs with the artist "__not_artist__".
    rule_add_criteria(&artist_match, "artist", "__not_artist__");
    struct datum artist_match_d = {
        .data = (void *)&artist_match,
        .length = sizeof(struct song_rule),
    };
    list_push(&ruleset, &artist_match_d);

    TEST_SONG(song_a, TAG(MPD_TAG_ARTIST, "__artist__"));
    TEST_SONG(song_b, TAG(MPD_TAG_ARTIST, "__not_artist__"));
    TEST_SONG(song_c, TAG(MPD_TAG_ARTIST, "__artist__"));

    list_push_song(&c.song_iter, &song_a);
    list_push_song(&c.song_iter, &song_b);
    list_push_song(&c.song_iter, &song_c);

    int result = build_songs_mpd(&c, &ruleset, &chain);
    cmp_ok(result, "==", 0, "build_songs_mpd filter returns ok");
    cmp_ok(shuffle_length(&chain), "==", 2,
           "build_songs_mpd_filter: 2 songs added to shuffle chain");

    mpd_connection_free(&c);
    rule_free(&artist_match);
    shuffle_free(&chain);
    list_free(&ruleset);
}

void xfwrite(FILE *f, const char *msg) {
    if (!fwrite(msg, strlen(msg), 1, f)) {
        perror("couldn't write to file");
        abort();
    }
}

void xfwriteln(FILE *f, const char *msg) {
    // +2 for \n\0
    char *nl = xmalloc(strlen(msg) + 2);
    strcpy(nl, msg);
    strncat(nl, "\n", 2);
    xfwrite(f, nl);
    free(nl);
}

// "a" and "b" *point* to the objects (strings) being compared, so we
// dereference and adapt the types.
static int compare_str(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
}

void test_build_songs_file_nocheck() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    mpd_connection_set_error(
        &c, MPD_ERROR_ARGUMENT,
        "ashuffle should not dial MPD when check is false");

    struct shuffle_chain chain;
    const unsigned window_size = 3;
    shuffle_init(&chain, window_size);

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

    int result = build_songs_file(&c, NULL, f, &chain, false);
    cmp_ok(result, "==", 0, "build_songs_file nocheck returns ok");
    cmp_ok(shuffle_length(&chain), "==", 3,
           "build_songs_file_nocheck: 3 songs added to shuffle chain");

    // To make sure we parsed the file correctly, pick three songs out of the
    // shuffle chain, and make sure they match the three URIs we wrote. This
    // should be stable because we set a window size equal to the number of
    // song URIs, and sort the URIs we receive from shuffle_pick.
    const char *want[] = {song_a.uri, song_b.uri, song_c.uri};
    const char *got[3];
    got[0] = shuffle_pick(&chain);
    got[1] = shuffle_pick(&chain);
    got[2] = shuffle_pick(&chain);

    qsort(want, STATIC_ARRAY_LEN(want), sizeof(want[0]), compare_str);
    qsort(got, STATIC_ARRAY_LEN(got), sizeof(got[0]), compare_str);

    assert(STATIC_ARRAY_LEN(want) == window_size &&
           "number of wanted URIs should match the window size");
    static_assert(STATIC_ARRAY_LEN(want) == STATIC_ARRAY_LEN(got),
                  "length of want and got should be equal so all elements can "
                  "be compared");

    for (unsigned i = 0; i < STATIC_ARRAY_LEN(want); i++) {
        bool equal = strcmp(want[i], got[i]) == 0;
        ok(equal, "build_songs_file_nocheck: want == got at %u", i);
        if (!equal) {
            diag("(want) %s != (got) %s", want[i], got[i]);
        }
    }

    // tmpfile is automatically cleaned up here.
    fclose(f);

    mpd_connection_free(&c);
    shuffle_free(&chain);
}

void test_build_songs_file_check() {
    // step 1. Initialize the MPD connection.
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    // step 2. Build the ruleset, and add an exclusions for __not_artist__
    struct list ruleset;
    list_init(&ruleset);

    struct song_rule artist_match;
    rule_init(&artist_match);
    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    // Exclude all songs with the artist "__not_artist__".
    rule_add_criteria(&artist_match, "artist", "__not_artist__");
    struct datum artist_match_d = {
        .data = (void *)&artist_match,
        .length = sizeof(struct song_rule),
    };
    list_push(&ruleset, &artist_match_d);

    // step 3. Prepare the shuffle_chain.
    struct shuffle_chain chain;
    const unsigned window_size = 2;
    shuffle_init(&chain, window_size);

    // step 4. Prepare our songs/song list. The song_list will be used for
    // subsequent calls to `mpd_recv_song`.
    TEST_SONG(song_a, TAG(MPD_TAG_ARTIST, "__artist__"));
    TEST_SONG(song_b, TAG(MPD_TAG_ARTIST, "__not_artist__"));
    TEST_SONG(song_c, TAG(MPD_TAG_ARTIST, "__artist__"));

    // When matching each song URI against the rules, ashuffle executes
    // mpd_recv_song twice. Once to receive the actual song object from
    // libmpdclient, and once to "finish" the iteration. So, for each URI we
    // want to test, we also need to insert an empty entry after it.
    list_init(&c.song_iter);
    list_push_song(&c.song_iter, &song_a);
    list_push_empty(&c.song_iter);
    list_push_song(&c.song_iter, &song_b);
    list_push_empty(&c.song_iter);
    list_push_song(&c.song_iter, &song_c);
    list_push_empty(&c.song_iter);

    // step 5. Set up our test input file, but writing the URIs of our songs.
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

    // step 6. Run! (and validate)

    int result = build_songs_file(&c, &ruleset, f, &chain, true);
    cmp_ok(result, "==", 0, "build_songs_file check returns ok");
    cmp_ok(shuffle_length(&chain), "==", 2,
           "build_songs_file_check: 2 songs added to shuffle chain");

    // This check works like the nocheck case, but instead of expecting us
    // to pick all 3 songs that were written into the input file, we only want
    // to pick song_a and song_c which are not excluded by the ruleset
    const char *want[] = {song_a.uri, song_c.uri};
    const char *got[2];
    got[0] = shuffle_pick(&chain);
    got[1] = shuffle_pick(&chain);

    qsort(want, STATIC_ARRAY_LEN(want), sizeof(want[0]), compare_str);
    qsort(got, STATIC_ARRAY_LEN(got), sizeof(got[0]), compare_str);

    assert(STATIC_ARRAY_LEN(want) == window_size &&
           "number of wanted URIs should match the window size");
    static_assert(STATIC_ARRAY_LEN(want) == STATIC_ARRAY_LEN(got),
                  "length of want and got should be equal so all elements can "
                  "be compared");

    for (unsigned i = 0; i < STATIC_ARRAY_LEN(want); i++) {
        bool equal = strcmp(want[i], got[i]) == 0;
        ok(equal, "build_songs_file_check: want == got at %u", i);
        if (!equal) {
            diag("(want) %s != (got) %s", want[i], got[i]);
        }
    }

    // cleanup.
    fclose(f);

    mpd_connection_free(&c);
    shuffle_free(&chain);
    rule_free(&artist_match);
    list_free(&ruleset);
}

void test_shuffle_single() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);
    list_push_song(&c.db, &song_b);

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

    cmp_ok(c.queue.length, "==", 0,
           "shuffle_single: queue empty before song added");
    shuffle_single(&c, &chain);

    cmp_ok(c.queue.length, "==", 1,
           "shuffle_single: queue length 1 after song added");

    struct mpd_song *queue_head = list_at(&c.queue, 0)->data;
    is(queue_head->uri, song_a.uri,
       "shuffle_single: ensure that song_a was added");

    mpd_connection_free(&c);
    shuffle_free(&chain);
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

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    struct shuffle_test_delegate delegate = {
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
}

void test_shuffle_loop_init_playing() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

    list_init(&c.db);
    list_push_song(&c.db, &song_a);

    // Pretend like we already have a song in our queue, and we're playing.
    list_init(&c.queue);
    list_push_song(&c.queue, &song_a);

    c.state.play_state = MPD_STATE_PLAY;
    c.state.queue_pos = 0;

    struct shuffle_test_delegate delegate = {
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
}

void test_shuffle_loop_init_stopped() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

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
}

void test_shuffle_loop_basic() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

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
}

void test_shuffle_loop_empty() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

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
}

void test_shuffle_loop_empty_buffer() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);

    struct ashuffle_options options;
    options_init(&options);
    options.queue_buffer = 3;

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

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
}

void test_shuffle_loop_buffer_partial() {
    struct mpd_connection c;
    memset(&c, 0, sizeof(c));

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);

    struct ashuffle_options options;
    options_init(&options);
    options.queue_buffer = 3;

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    shuffle_add(&chain, song_a.uri);

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

    mpd_set_connection(&c);

    struct mpd_connection *result = ashuffle_connect(&opts, failing_getpass_f);

    ok(result == &c, "connect_no_password: connection matches set connection");
    cmp_ok(c.error.error, "==", MPD_ERROR_SUCCESS,
           "connect_no_password: connection successful");

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

    done_testing();
}
