#include <memory>
#include <string_view>

#include <mpd/tag.h>
#include <tap.h>

#include "mpd.h"
#include "rule.h"

#include "t/mpd_fake.h"

using namespace ashuffle;

static void test_basic() {
    Rule rule;

    ok(rule.Empty(), "no matchers on empty rule");
    rule.AddPattern(MPD_TAG_ARTIST, "foo fighters");
    ok(!rule.Empty(), "rule no longer empty after adding one matcher");

    fake::Song matching({{MPD_TAG_ARTIST, "foo fighters"}});
    fake::Song non_matching({{MPD_TAG_ARTIST, "some randy"}});

    ok(!rule.Accepts(matching), "should exclude song with matching tag");
    ok(rule.Accepts(non_matching), "includes song with non-matching tag");
}

static void test_submatch() {
    Rule rule;
    rule.AddPattern(MPD_TAG_ARTIST, "foo");

    fake::Song matching({{MPD_TAG_ARTIST, "foo fighters"}});
    fake::Song mid_word_matching({{MPD_TAG_ARTIST, "floofoofaf"}});
    fake::Song mid_word_matching_case({{MPD_TAG_ARTIST, "fLOoFoOfaF"}});

    ok(!rule.Accepts(matching), "should exclude song with submatch");
    ok(!rule.Accepts(mid_word_matching),
       "should exclude song with submatch mid-word");
    ok(!rule.Accepts(mid_word_matching_case),
       "should exclude song with submatch mid-word (case insensitive)");
}

static void test_multi() {
    Rule rule;
    rule.AddPattern(MPD_TAG_ALBUM, "__album__");
    rule.AddPattern(MPD_TAG_ARTIST, "__artist__");

    fake::Song full_match({
        {MPD_TAG_ARTIST, "__artist__"},
        {MPD_TAG_ALBUM, "__album__"},
    });
    fake::Song partial_match_artist({
        {MPD_TAG_ARTIST, "__artist__"},
        {MPD_TAG_ALBUM, "no match"},
    });
    fake::Song partial_match_album({
        {MPD_TAG_ARTIST, "no match"},
        {MPD_TAG_ALBUM, "__album__"},
    });
    fake::Song no_match({
        {MPD_TAG_ARTIST, "no match"},
        {MPD_TAG_ALBUM, "no match"},
    });

    ok(!rule.Accepts(full_match), "should match if all fields match");
    ok(!rule.Accepts(partial_match_artist),
       "should match if any field matches (artist)");
    ok(!rule.Accepts(partial_match_album),
       "should match if any field matches (album)");
    ok(rule.Accepts(no_match),
       "no match if no matching fields, even with multiple possibilities");
}

int main() {
    plan(NO_PLAN);

    test_basic();
    test_submatch();
    test_multi();

    done_testing();
}
