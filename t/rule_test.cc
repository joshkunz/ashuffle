#include "rule.h"

#include <memory>
#include <string_view>

#include <mpd/tag.h>

#include "mpd.h"

#include "t/mpd_fake.h"

#include <gtest/gtest.h>

using namespace ashuffle;

TEST(Rule, Empty) {
    Rule rule;
    EXPECT_TRUE(rule.Empty()) << "rule with no matchers should be empty";

    rule.AddPattern(MPD_TAG_ARTIST, "foo fighters");
    EXPECT_FALSE(rule.Empty()) << "rule with matcher should not be empty";
}

TEST(Rule, Accepts) {
    Rule rule;
    rule.AddPattern(MPD_TAG_ARTIST, "foo fighters");

    fake::Song matching({{MPD_TAG_ARTIST, "foo fighters"}});
    fake::Song non_matching({{MPD_TAG_ARTIST, "some randy"}});

    // Remember, these are exclusion rules, so if a song matches, it should
    // *not* be accepted by the rule.
    EXPECT_FALSE(rule.Accepts(matching));
    EXPECT_TRUE(rule.Accepts(non_matching));
}

TEST(Rule, PatternIsSubstring) {
    Rule rule;
    rule.AddPattern(MPD_TAG_ARTIST, "foo");

    fake::Song matching({{MPD_TAG_ARTIST, "foo fighters"}});
    fake::Song mid_word_matching({{MPD_TAG_ARTIST, "floofoofaf"}});

    EXPECT_FALSE(rule.Accepts(matching));
    EXPECT_FALSE(rule.Accepts(mid_word_matching));
}

TEST(Rule, PatternCaseInsensitive) {
    Rule rule;
    rule.AddPattern(MPD_TAG_ARTIST, "foo");

    fake::Song weird_case({{MPD_TAG_ARTIST, "fLOoFoOfaF"}});

    EXPECT_FALSE(rule.Accepts(weird_case))
        << "failed to match substring with different case";
}

TEST(Rule, MultiplePatterns) {
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

    EXPECT_FALSE(rule.Accepts(full_match))
        << "song accepted even though some fields match a pattern";
    // If any field doesn't match, the rule should consider the song
    // as accepted.
    EXPECT_TRUE(rule.Accepts(partial_match_artist));
    EXPECT_TRUE(rule.Accepts(partial_match_album));
    EXPECT_TRUE(rule.Accepts(no_match));
}

TEST(Rule, SongMissingPatternTag) {
    Rule rule;
    rule.AddPattern(MPD_TAG_ALBUM, "__album__");

    // This song does not even have an MPD_TAG_ALBUM tag.
    fake::Song missing_pattern_tag({
        {MPD_TAG_ARTIST, "__artist__"},
    });

    EXPECT_TRUE(rule.Accepts(missing_pattern_tag))
        << "Songs with missing tags should be accepted";
}
