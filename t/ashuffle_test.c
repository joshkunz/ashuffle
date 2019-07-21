#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "args.h"
#include "util.h"
#include "shuffle.h"
#include "ashuffle.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

// Main tests
// connect
// build_songs_file
//  + with/without nocheck
// build_songs_mpd
//  + ruleset accepts uri?
// shuffle_single
// shuffle_idle
//  + try_first
//  + try_enqueue

void list_push_song(struct list* l, struct mpd_song *s) {
    struct datum d = {
        .length = sizeof(struct mpd_song),
        .data = (void *) s,
    };
    list_push(l, &d);
}

void test_build_songs_mpd_basic() {
    struct mpd_connection* c = xmalloc(sizeof(struct mpd_connection));

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    struct list ruleset;
    list_init(&ruleset);

    TEST_SONG_URI(song_a);
    TEST_SONG_URI(song_b);
    list_push_song(&c->song_iter, &song_a);
    list_push_song(&c->song_iter, &song_b);

    int result = build_songs_mpd(c, &ruleset, &chain);
    cmp_ok(result, "==", 0, "build_songs_mpd basic returns ok");
    cmp_ok(shuffle_length(&chain), "==", 2, "build_songs_mpd_basic: 2 songs added to shuffle chain");

    mpd_connection_free(c);
    shuffle_free(&chain);
    list_free(&ruleset);
}

int main() {
    plan(NO_PLAN);

    test_build_songs_mpd_basic();


    done_testing();
}
