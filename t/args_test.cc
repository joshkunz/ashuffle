#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include <absl/time/time.h>
#include <mpd/tag.h>

#include "args.h"
#include "rule.h"

#include "t/mpd_fake.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::MatchesRegex;
using ::testing::NotNull;
using ::testing::Values;

TEST(ParseTest, Empty) {
    fake::TagParser tagger;
    auto result = Options::Parse(tagger, std::vector<std::string>());
    ASSERT_EQ(std::get_if<ParseError>(&result), nullptr);

    Options opts = std::move(std::get<Options>(result));

    EXPECT_TRUE(opts.ruleset.empty()) << "there should be no rules by default";
    EXPECT_EQ(opts.queue_only, 0U);
    EXPECT_THAT(opts.file_in, IsNull());
    EXPECT_TRUE(opts.check_uris);
    EXPECT_EQ(opts.queue_buffer, 0U);
    EXPECT_EQ(opts.host, std::nullopt);
    EXPECT_EQ(opts.port, 0U);
    EXPECT_FALSE(opts.test.print_all_songs_and_exit);
    EXPECT_TRUE(opts.group_by.empty());
    EXPECT_EQ(opts.tweak.window_size, 7);
    EXPECT_EQ(opts.tweak.play_on_startup, true);
    EXPECT_EQ(opts.tweak.suspend_timeout, absl::ZeroDuration());
    EXPECT_EQ(opts.tweak.exit_on_db_update, false);
}

TEST(ParseTest, Short) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
    });

    // clang-format off
    auto result = Options::Parse(tagger, {
        "-o", "5",
        "-n",
        "-q", "10",
        "-e", "artist", "test artist", "artist", "another one",
        "-f", "/dev/zero",
        "-p", "1234",
        "-g", "artist",
        "-t", "window-size=3",
    });
    // clang-format on

    ASSERT_EQ(std::get_if<ParseError>(&result), nullptr)
        << std::get<ParseError>(result);

    Options opts = std::move(std::get<Options>(result));

    EXPECT_EQ(opts.ruleset.size(), 1U);
    EXPECT_EQ(opts.queue_only, 5U);
    EXPECT_THAT(opts.file_in, NotNull());
    EXPECT_FALSE(opts.check_uris);
    EXPECT_EQ(opts.queue_buffer, 10U);
    EXPECT_EQ(opts.port, 1234U);
    EXPECT_THAT(opts.group_by, ElementsAre(MPD_TAG_ARTIST));
    EXPECT_EQ(opts.tweak.window_size, 3);
}

TEST(ParseTest, Long) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
    });

    // clang-format off
    auto result = Options::Parse(tagger, {
            "--only", "5",
            "--no-check",
            "--file", "/dev/zero",
            "--exclude", "artist", "test artist", "artist", "another one",
            "--queue-buffer", "10",
            "--host", "foo",
            "--port", "1234",
            "--group-by", "artist",
            "--tweak", "window-size=5",
    });
    // clang-format on

    ASSERT_EQ(std::get_if<ParseError>(&result), nullptr)
        << std::get<ParseError>(result);

    Options opts = std::move(std::get<Options>(result));

    EXPECT_EQ(opts.ruleset.size(), 1U);
    EXPECT_EQ(opts.queue_only, 5U);
    EXPECT_THAT(opts.file_in, NotNull());
    EXPECT_FALSE(opts.check_uris);
    EXPECT_EQ(opts.queue_buffer, 10U);
    EXPECT_EQ(opts.host, "foo");
    EXPECT_EQ(opts.port, 1234U);
    EXPECT_THAT(opts.group_by, ElementsAre(MPD_TAG_ARTIST));
    EXPECT_EQ(opts.tweak.window_size, 5);
}

TEST(ParseTest, MixedLongShort) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
    });

    // clang-format off
    Options opts =
        std::get<Options>(Options::Parse(tagger, {
            "-o", "5",
            "--file", "/dev/zero",
            "-n",
            "--queue-buffer", "10",
            "--exclude", "artist", "test artist", "artist", "another one",
    }));
    // clang-format on

    EXPECT_EQ(opts.ruleset.size(), 1U);
    EXPECT_EQ(opts.queue_only, 5U);
    EXPECT_THAT(opts.file_in, NotNull());
    EXPECT_FALSE(opts.check_uris);
    EXPECT_EQ(opts.queue_buffer, 10U);
}

TEST(ParseTest, Rule) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
    });

    Options opts = std::get<Options>(
        Options::Parse(tagger, {"-e", "artist", "__artist__"}));

    ASSERT_FALSE(opts.ruleset.empty());

    // Now we pull out the first rule, and then check it against our
    // test songs. This is to assert that we parsed the rule correctly.
    Rule &r = opts.ruleset[0];

    fake::Song matching({{MPD_TAG_ARTIST, "__artist__"}});
    fake::Song not_matching({{MPD_TAG_ARTIST, "not artist"}});

    EXPECT_FALSE(r.Accepts(matching))
        << "basic rule arg should exclude match song";
    EXPECT_TRUE(r.Accepts(not_matching))
        << "basic rule arg should not exclude non-matching song";
}

TEST(ParseTest, RuleWithMultipleTags) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
        {"album", MPD_TAG_ALBUM},
    });

    Options opts = std::get<Options>(Options::Parse(
        tagger, {"-e", "artist", "__artist__", "album", "__album__"}));

    ASSERT_FALSE(opts.ruleset.empty());

    EXPECT_EQ(opts.ruleset.size(), 1);

    Rule &r = opts.ruleset[0];

    EXPECT_EQ(r.Size(), 2) << "Rule should have two matching patterns";

    fake::Song matching({
        {MPD_TAG_ARTIST, "__artist__"},
        {MPD_TAG_ALBUM, "__album__"},
    });
    fake::Song partial_artist({{MPD_TAG_ARTIST, "__artist__"}});
    fake::Song partial_album({{MPD_TAG_ALBUM, "__album__"}});

    EXPECT_FALSE(r.Accepts(matching));
    EXPECT_TRUE(r.Accepts(partial_artist));
    EXPECT_TRUE(r.Accepts(partial_album));
}

TEST(ParseTest, MultipleRules) {
    fake::TagParser tagger({
        {"artist", MPD_TAG_ARTIST},
    });

    // clang-format off
    Options opts = std::get<Options>(
        Options::Parse(tagger, {
            "-e", "artist", "__artist__",
            "-e", "artist", "__another_artist__"
        }));
    // clang-format on

    ASSERT_EQ(opts.ruleset.size(), 2) << "Expected two rules";

    constexpr char message[] = "Each ruleset should have exactly one pattern";
    EXPECT_EQ(opts.ruleset[0].Size(), 1) << message;
    EXPECT_EQ(opts.ruleset[1].Size(), 1) << message;
}

TEST(ParseTest, FileInStdin) {
    Options opts;
    fake::TagParser tagger;

    opts = std::get<Options>(Options::Parse(tagger, {"-f", "-"}));
    EXPECT_EQ(opts.file_in, &std::cin);

    opts = std::get<Options>(Options::Parse(tagger, {"--file", "-"}));
    EXPECT_EQ(opts.file_in, &std::cin);
}

TEST(ParseTest, ByAlbum) {
    Options opts;
    fake::TagParser tagger;

    opts = std::get<Options>(Options::Parse(fake::TagParser(), {"--by-album"}));
    EXPECT_THAT(opts.group_by, ElementsAre(MPD_TAG_ALBUM, MPD_TAG_DATE))
        << "--by-album should be equivalent to --group-by album date";
}

TEST(ParseTest, TweakPlayOnStartup) {
    std::vector<std::tuple<std::string, bool>> cases = {
        {"on", true},   {"true", true}, {"yes", true},    {"1", true},
        {"True", true}, {"yEs", true},  {"off", false},   {"false", false},
        {"no", false},  {"0", false},   {"False", false}, {"nO", false},
    };

    for (auto [val, want] : cases) {
        Options opts = std::get<Options>(Options::Parse(
            fake::TagParser(), {"--tweak", "play-on-startup=" + val}));
        EXPECT_EQ(opts.tweak.play_on_startup, want) << "Case: " << val;
    }
}

TEST(ParseTest, TweakSuspendTimeout) {
    std::vector<std::tuple<std::string, absl::Duration>> cases = {
        {"1s", absl::Seconds(1)},
        {"1m", absl::Minutes(1)},
        {"3h", absl::Hours(3)},
        {"250ms", absl::Milliseconds(250)},
    };

    for (auto [val, want] : cases) {
        Options opts = std::get<Options>(Options::Parse(
            fake::TagParser(), {"--tweak", "suspend-timeout=" + val}));
        EXPECT_EQ(opts.tweak.suspend_timeout, want) << "Case: " << val;
    }
}

TEST(ParseTest, TweakExitOnDBUpdate) {
    Options opts = std::get<Options>(Options::Parse(
        fake::TagParser(), {"--tweak", "exit-on-db-update=yes"}));
    EXPECT_EQ(opts.tweak.exit_on_db_update, true);
}

using ParseFailureParam =
    std::tuple<std::vector<std::string>, Matcher<std::string>>;

class ParseFailureTest : public testing::TestWithParam<ParseFailureParam> {
   public:
    std::optional<ParseError> result_;

    std::vector<std::string> Args() { return std::get<0>(GetParam()); }

    Matcher<std::string> ErrorMatcher() { return std::get<1>(GetParam()); }

    void SetUp() override {
        fake::TagParser tagger({
            {"artist", MPD_TAG_ARTIST},
        });

        std::variant<Options, ParseError> result =
            Options::Parse(tagger, Args());
        if (ParseError *err = std::get_if<ParseError>(&result);
            err != nullptr) {
            result_ = *err;
        }
    };
};

TEST_P(ParseFailureTest, ParseFail) {
    ASSERT_TRUE(result_.has_value());
    EXPECT_EQ(result_->type, ParseError::Type::kGeneric);
    EXPECT_THAT(result_->msg, ErrorMatcher());
}

std::vector<ParseFailureParam> partial_cases = {
    {{"-o"}, HasSubstr("no argument supplied for '-o'")},
    {{"--only"}, HasSubstr("no argument supplied for '--only'")},
    {{"-f"}, HasSubstr("no argument supplied for '-f'")},
    {{"--file"}, HasSubstr("no argument supplied for '--file'")},
    {{"-q"}, HasSubstr("no argument supplied for '-q'")},
    {{"--queue-buffer"},
     HasSubstr("no argument supplied for '--queue-buffer'")},
    {{"-e"}, HasSubstr("no argument supplied for '-e'")},
    {{"-e", "artist"}, HasSubstr("no value supplied for match 'artist'")},
    {{"-e", "artist", "whatever", "artist"},
     HasSubstr("no value supplied for match 'artist'")},
    {{"--exclude"}, HasSubstr("no argument supplied for '--exclude'")},
    {{"--exclude", "artist"},
     HasSubstr("no value supplied for match 'artist'")},
    {{"--exclude", "artist", "whatever", "artist"},
     HasSubstr("no value supplied for match 'artist'")},
    {{"--host"}, HasSubstr("no argument supplied for '--host'")},
    {{"-p"}, HasSubstr("no argument supplied for '-p'")},
    {{"--port"}, HasSubstr("no argument supplied for '--port'")},
    {{"--test_enable_option_do_not_use"},
     HasSubstr("no argument supplied for '--test_enable_option_do_not_use'")},
    {{"-g"}, HasSubstr("no argument supplied for '-g'")},
    {{"--group-by"}, HasSubstr("no argument supplied for '--group-by'")},
    {{"-g", "artist", "--by-album"},
     HasSubstr("'--by-album' can only be provided once")},
    {{"-g", "artist", "-g", "invalid"},
     HasSubstr("'-g' can only be provided once")},
    {{"--by-album", "-g", "artist"},
     HasSubstr("'-g' can only be provided once")},
    {{"--tweak"}, HasSubstr("no argument supplied for '--tweak'")},
    {{"--tweak", "window-size", "fail"},
     HasSubstr("tweak must be of the form <name>=<value>")},
    {{"--tweak", "window-size="},
     HasSubstr("tweak must be of the form <name>=<value>")},
    {{"--tweak", "play-on-startup="},
     HasSubstr("tweak must be of the form <name>=<value>")},
};

INSTANTIATE_TEST_SUITE_P(Partials, ParseFailureTest, ValuesIn(partial_cases));

std::vector<ParseFailureParam> strtou_cases = {
    {{"--only", "0x5.0"}, MatchesRegex("couldn't convert .* '0x5\\.0'")},
    {{"--queue-buffer", "20U"}, MatchesRegex("couldn't convert .* '20U'")},
    {{"--tweak", "window-size=20=x"},
     MatchesRegex("couldn't convert .* '20=x'")},
};

INSTANTIATE_TEST_SUITE_P(BadStrtou, ParseFailureTest, ValuesIn(strtou_cases));

std::vector<ParseFailureParam> constraint_cases = {
    {{"--tweak", "window-size=0"},
     HasSubstr("window-size must be >= 1 (0 given)")},
    {{"--tweak", "window-size=-2"},
     HasSubstr("window-size must be >= 1 (-2 given)")},
    {{"--tweak", "play-on-startup=2"},
     HasSubstr("play-on-startup must be a boolean value ('2' given)")},
    {{"--tweak", "suspend-timeout=2"},
     HasSubstr("suspend-timeout must be a duration with units e.g., 250ms ('2' "
               "given)")},
    {{"--tweak", "suspend-timeout=-1ms"},
     HasSubstr("suspend-timeout must be a positive duration ('-1ms' given)")},
};

INSTANTIATE_TEST_SUITE_P(Constraint, ParseFailureTest,
                         ValuesIn(constraint_cases));

TEST(ParseTest, TestOption) {
    fake::TagParser tagger;
    Options opts = std::get<Options>(
        Options::Parse(tagger, {
                                   "--test_enable_option_do_not_use",
                                   "print_all_songs_and_exit",
                               }));

    EXPECT_TRUE(opts.test.print_all_songs_and_exit);
}

TEST(ParseTest, ParseFromC) {
    fake::TagParser tagger;
    const char *c_argv[] = {"ashuffle", "-o", "33"};
    auto res = Options::ParseFromC(tagger, c_argv, 3);
    Options *opts = std::get_if<Options>(&res);
    ASSERT_NE(opts, nullptr) << "Options failed to parse from C";
    EXPECT_EQ(opts->queue_only, 33U);
}

TEST(ParseTest, VersionShort) {
    fake::TagParser tagger;

    std::variant<Options, ParseError> res = Options::Parse(tagger, {"-v"});
    ASSERT_TRUE(std::holds_alternative<ParseError>(res));

    ParseError e = std::get<ParseError>(res);
    EXPECT_EQ(e.type, ParseError::Type::kVersion);
}

TEST(ParseTest, VersionLong) {
    fake::TagParser tagger;

    std::variant<Options, ParseError> res =
        Options::Parse(tagger, {"--version"});
    ASSERT_TRUE(std::holds_alternative<ParseError>(res));

    ParseError e = std::get<ParseError>(res);
    EXPECT_EQ(e.type, ParseError::Type::kVersion);
}

TEST(ParseTest, VersionExtraOptions) {
    fake::TagParser tagger;

    std::variant<Options, ParseError> res =
        Options::Parse(tagger, {"-o", "1", "-v"});
    ASSERT_TRUE(std::holds_alternative<ParseError>(res));

    ParseError e = std::get<ParseError>(res);
    EXPECT_EQ(e.type, ParseError::Type::kVersion);
}
