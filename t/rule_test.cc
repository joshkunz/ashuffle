#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "rule.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

static void test_basic() {
    Rule rule;

    SetTagNameIParse("don't care", MPD_TAG_UNKNOWN);
    Rule::Status res = rule.AddPattern("don't care", "don't care");
    cmp_ok(res, "==", Rule::Status::kFail,
           "add_criteria should fail on MPD_TAG_UNKNOWN");
    ok(rule.Empty(), "no matchers after failed add_critera");

    SetTagNameIParse("artist", MPD_TAG_ARTIST);
    res = rule.AddPattern("artist", "foo fighters");
    cmp_ok(res, "==", Rule::Status::kOK, "add_criteria with regular tag works");

    TEST_SONG(matching, TAG(MPD_TAG_ARTIST, "foo fighters"));
    TEST_SONG(non_matching, TAG(MPD_TAG_ARTIST, "some randy"));

    ok(!rule.Accepts(&matching), "should exclude song with matching tag");
    ok(rule.Accepts(&non_matching), "incldues song with non-matching tag");
}

static void test_submatch() {
    Rule rule;

    SetTagNameIParse("artist", MPD_TAG_ARTIST);
    Rule::Status res = rule.AddPattern("artist", "foo");
    cmp_ok(res, "==", Rule::Status::kOK);

    TEST_SONG(matching, TAG(MPD_TAG_ARTIST, "foo fighters"));
    TEST_SONG(mid_word_matching, TAG(MPD_TAG_ARTIST, "floofoofaf"));
    TEST_SONG(mid_word_matching_case, TAG(MPD_TAG_ARTIST, "fLOoFoOfaF"));

    ok(!rule.Accepts(&matching), "should exclude song with submatch");
    ok(!rule.Accepts(&mid_word_matching),
       "should exclude song with submatch mid-word");
    ok(!rule.Accepts(&mid_word_matching_case),
       "should exclude song with submatch mid-word (case insensitive)");
}

static void test_multi() {
    Rule::Status res;

    Rule rule;

    SetTagNameIParse("album", MPD_TAG_ALBUM);
    res = rule.AddPattern("album", "__album__");
    cmp_ok(res, "==", Rule::Status::kOK);
    SetTagNameIParse("artist", MPD_TAG_ARTIST);
    res = rule.AddPattern("artist", "__artist__");
    cmp_ok(res, "==", Rule::Status::kOK);

    TEST_SONG(full_match, TAG(MPD_TAG_ARTIST, "__artist__"),
              TAG(MPD_TAG_ALBUM, "__album__"));

    TEST_SONG(partial_match_artist, TAG(MPD_TAG_ARTIST, "__artist__"),
              TAG(MPD_TAG_ALBUM, "no match"));

    TEST_SONG(partial_match_album, TAG(MPD_TAG_ARTIST, "no match"),
              TAG(MPD_TAG_ALBUM, "__album__"));

    TEST_SONG(no_match, TAG(MPD_TAG_ARTIST, "no match"),
              TAG(MPD_TAG_ALBUM, "no match"));

    ok(!rule.Accepts(&full_match), "should match if all fields match");
    ok(!rule.Accepts(&partial_match_artist),
       "should match if any field matches (artist)");
    ok(!rule.Accepts(&partial_match_album),
       "should match if any field matches (album)");
    ok(rule.Accepts(&no_match),
       "no match if no matching fields, even with multiple possibilities");
}

int main() {
    plan(NO_PLAN);

    test_basic();
    test_submatch();
    test_multi();

    done_testing();
}
