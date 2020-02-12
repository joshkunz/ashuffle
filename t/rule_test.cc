#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "list.h"
#include "rule.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

static void test_basic() {
    struct song_rule rule;
    rule_init(&rule);

    set_tag_name_iparse_result("don't care", MPD_TAG_UNKNOWN);
    int res = rule_add_criteria(&rule, "don't care", "don't care");
    cmp_ok(res, "!=", 0, "add_criteria should fail on MPD_TAG_UNKNOWN");
    cmp_ok(rule.matchers.length, "==", 0,
           "no matchers after failed add_critera");

    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    res = rule_add_criteria(&rule, "artist", "foo fighters");
    cmp_ok(res, "==", 0, "add_criteria with regular tag works");

    TEST_SONG(matching, TAG(MPD_TAG_ARTIST, "foo fighters"));
    TEST_SONG(non_matching, TAG(MPD_TAG_ARTIST, "some randy"));

    ok(!rule_match(&rule, &matching), "should exclude song with matching tag");
    ok(rule_match(&rule, &non_matching), "incldues song with non-matching tag");

    rule_free(&rule);
}

static void test_submatch() {
    struct song_rule rule;
    rule_init(&rule);

    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    int res = rule_add_criteria(&rule, "artist", "foo");
    cmp_ok(res, "==", 0);

    TEST_SONG(matching, TAG(MPD_TAG_ARTIST, "foo fighters"));

    TEST_SONG(mid_word_matching, TAG(MPD_TAG_ARTIST, "floofoofaf"));

    TEST_SONG(mid_word_matching_case, TAG(MPD_TAG_ARTIST, "fLOoFoOfaF"));

    ok(!rule_match(&rule, &matching), "should exclude song with submatch");
    ok(!rule_match(&rule, &mid_word_matching),
       "should exclude song with submatch mid-word");
    ok(!rule_match(&rule, &mid_word_matching_case),
       "should exclude song with submatch mid-word (case insensitive)");

    rule_free(&rule);
}

static void test_multi() {
    int res = -1;

    struct song_rule rule;
    rule_init(&rule);

    set_tag_name_iparse_result("album", MPD_TAG_ALBUM);
    res = rule_add_criteria(&rule, "album", "__album__");
    cmp_ok(res, "==", 0);
    set_tag_name_iparse_result("artist", MPD_TAG_ARTIST);
    res = rule_add_criteria(&rule, "artist", "__artist__");
    cmp_ok(res, "==", 0);

    TEST_SONG(full_match, TAG(MPD_TAG_ARTIST, "__artist__"),
              TAG(MPD_TAG_ALBUM, "__album__"));

    TEST_SONG(partial_match_artist, TAG(MPD_TAG_ARTIST, "__artist__"),
              TAG(MPD_TAG_ALBUM, "no match"));

    TEST_SONG(partial_match_album, TAG(MPD_TAG_ARTIST, "no match"),
              TAG(MPD_TAG_ALBUM, "__album__"));

    TEST_SONG(no_match, TAG(MPD_TAG_ARTIST, "no match"),
              TAG(MPD_TAG_ALBUM, "no match"));

    ok(!rule_match(&rule, &full_match), "should match if all fields match");
    ok(!rule_match(&rule, &partial_match_artist),
       "should match if any field matches (artist)");
    ok(!rule_match(&rule, &partial_match_album),
       "should match if any field matches (album)");
    ok(rule_match(&rule, &no_match),
       "no match if no matching fields, even with multiple possibilities");

    rule_free(&rule);
}

int main() {
    plan(NO_PLAN);

    test_basic();
    test_submatch();
    test_multi();

    done_testing();
}
