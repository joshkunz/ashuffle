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
// build_songs_file
//  + with/without nocheck
// (DONE) build_songs_mpd
// shuffle_single
// shuffle_idle
//  + try_first
//  + try_enqueue

void list_push_song(struct list *l, struct mpd_song *s) {
    struct datum d = {
        .length = sizeof(struct mpd_song),
        .data = (void *)s,
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

int main() {
    plan(NO_PLAN);

    test_build_songs_mpd_basic();
    test_build_songs_mpd_filter();
    test_build_songs_file_nocheck();

    done_testing();
}
