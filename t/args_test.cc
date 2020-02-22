#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "args.h"
#include "rule.h"
#include "util.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

template <typename... Args>
std::optional<ParseError> ParseOnly(Args... strs) {
    std::vector<std::string> args = {strs...};
    std::variant<Options, ParseError> result = Options::Parse(args);
    if (ParseError *err = std::get_if<ParseError>(&result); err != nullptr) {
        return *err;
    }
    return std::nullopt;
}

#define PARSE_ONLY(...) ParseOnly(__VA_ARGS__)

void test_default() {
    auto result = Options::Parse(std::vector<std::string>());
    ok(std::get_if<ParseError>(&result) == nullptr, "empty parse works");

    Options opts = std::get<Options>(result);
    ok(opts.ruleset.empty(), "no rules by default");
    cmp_ok(opts.queue_only, "==", 0, "no 'queue only' by default");
    ok(opts.file_in == NULL, "no input file by default");
    cmp_ok(opts.check_uris, "==", true, "check_uris on by default");
    cmp_ok(opts.queue_buffer, "==", 0, "no queue buffer by default");
    ok(opts.host == std::nullopt, "no host by default");
    cmp_ok(opts.port, "==", 0, "no port by default");
    cmp_ok(opts.test.print_all_songs_and_exit, "==", false,
           "no print_all_songs_and_exit by default");
}

void test_basic_short() {
    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    Options opts = std::get<Options>(Options::Parse({
        "-o",
        "5",
        "-n",
        "-q",
        "10",
        "-e",
        "artist",
        "test artist",
        "artist",
        "another one",
        "-f",
        "/dev/zero",
        "-p",
        "1234",
    }));

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic short detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic short queue only");
    ok(opts.file_in != nullptr, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic short nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic short queue buffer");
    cmp_ok(opts.port, "==", 1234);
}

void test_basic_long() {
    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    Options opts = std::get<Options>(Options::Parse({
        "--only",
        "5",
        "--no-check",
        "--file",
        "/dev/zero",
        "--exclude",
        "artist",
        "test artist",
        "artist",
        "another one",
        "--queue-buffer",
        "10",
        "--host",
        "foo",
        "--port",
        "1234",
    }));

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic long detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic long queue only");
    ok(opts.file_in != NULL, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic long nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic long queue buffer");
    ok(opts.host == "foo", "basic long host");
    cmp_ok(opts.port, "==", 1234, "basic long port");
}

void test_basic_mixed_long_short() {
    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    Options opts = std::get<Options>(Options::Parse({
        "-o",
        "5",
        "--file",
        "/dev/zero",
        "-n",
        "--queue-buffer",
        "10",
        "--exclude",
        "artist",
        "test artist",
        "artist",
        "another one",
    }));

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic mixed detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic mixed queue only");
    ok(opts.file_in != NULL, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic mixed nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic mixed queue buffer");
}

void test_bad_strtou() {
    std::optional<ParseError> res;
    res = ParseOnly("--only", "0x5.0");
    ok(res.has_value(), "bad strtou fails parse");
    like(res->msg.data(), "couldn't convert .* '0x5\\.0'");

    res = ParseOnly("--queue-buffer", "20U");
    ok(res.has_value(), "bad strtou fails parse");
    like(res->msg.data(), "couldn't convert .* '20U'");
}

void test_rule_basic() {
    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    Options opts =
        std::get<Options>(Options::Parse({"-e", "artist", "__artist__"}));

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic rule parsed at least one rule");

    skip(opts.ruleset.size() < 1, 2, "skipping rule tests, no rule parsed");

    // Now we pull out the first rule, and then check it against our
    // test songs.
    Rule &r = opts.ruleset[0];

    TEST_SONG(matching, TAG(MPD_TAG_ARTIST, "__artist__"));
    TEST_SONG(not_matching, TAG(MPD_TAG_ARTIST, "not artist"));

    ok(!r.Accepts(&matching), "basic rule arg should exclude match song");
    ok(r.Accepts(&not_matching),
       "basic rule arg should not exclude other song");

    end_skip;
}

void test_file_stdin() {
    Options opts;

    opts = std::get<Options>(Options::Parse({"-f", "-"}));
    ok(opts.file_in == stdin, "'-f -' parses as stdin");

    opts = std::get<Options>(Options::Parse({"--file", "-"}));
    ok(opts.file_in == stdin, "'--file -' parses as stdin");
}

#define TEST_PARSE_FAIL(prefix, _res, err_match)                               \
    do {                                                                       \
        std::optional<ParseError> res = (_res);                                \
        ok(res.has_value(), prefix " parse should fail");                      \
        skip(!res.has_value(), 2, prefix " parsed OK, skipping error checks"); \
        ok(res->type == ParseError::Type::kGeneric,                            \
           prefix " error is generic");                                        \
        like(res->msg.data(), err_match, prefix " error message matches");     \
        end_skip;                                                              \
    } while (0)

void test_partials() {
#define TEST_PARTIAL(name, _res, err_match) \
    TEST_PARSE_FAIL("partial " name, (_res), err_match)

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    TEST_PARTIAL("only short", ParseOnly("-o"),
                 "no argument supplied for '-o'");
    TEST_PARTIAL("only long", ParseOnly("--only"),
                 "no argument supplied for '--only'");

    TEST_PARTIAL("file short", ParseOnly("-f"),
                 "no argument supplied for '-f'");
    TEST_PARTIAL("file long", ParseOnly("--file"),
                 "no argument supplied for '--file'");

    TEST_PARTIAL("queue buffer short", ParseOnly("-q"),
                 "no argument supplied for '-q'");
    TEST_PARTIAL("queue buffer long", ParseOnly("--queue-buffer"),
                 "no argument supplied for '--queue-buffer'");

    TEST_PARTIAL("exclude short no arg", ParseOnly("-e"),
                 "no argument supplied for '-e'");
    TEST_PARTIAL("exclude short only match", ParseOnly("-e", "artist"),
                 "no value supplied for match 'artist'");
    TEST_PARTIAL("exclude short only match mutli",
                 ParseOnly("-e", "artist", "whatever", "artist"),
                 "no value supplied for match 'artist'");

    TEST_PARTIAL("exclude long no arg", ParseOnly("--exclude"),
                 "no argument supplied for '--exclude'");
    TEST_PARTIAL("exclude long only match", ParseOnly("--exclude", "artist"),
                 "no value supplied for match 'artist'");
    TEST_PARTIAL("exclude long only match mutli",
                 ParseOnly("--exclude", "artist", "whatever", "artist"),
                 "no value supplied for match 'artist'");

    TEST_PARTIAL("host long", ParseOnly("--host"),
                 "no argument supplied for '--host'");
    TEST_PARTIAL("port long", ParseOnly("--port"),
                 "no argument supplied for '--port'");
    TEST_PARTIAL("port short ", ParseOnly("-p"),
                 "no argument supplied for '-p'");

    TEST_PARTIAL("test option no arg",
                 ParseOnly("--test_enable_option_do_not_use"),
                 "no argument supplied for '--test_enable_option_do_not_use'");
}

void test_help() {
    ok(ParseOnly("-h")->type == ParseError::Type::kHelp, "'-h' parses as help");
    ok(ParseOnly("--help")->type == ParseError::Type::kHelp,
       "'--help' parses as help");
    ok(ParseOnly("-?")->type == ParseError::Type::kHelp, "'-?' parses as help");
}

void test_bad_option() {
    TEST_PARSE_FAIL("bad option 'blah'", ParseOnly("blah"),
                    "bad option 'blah'");
    TEST_PARSE_FAIL("--bad_arg in middle",
                    ParseOnly("-n", "--bad_arg", "-o", "5"),
                    "bad option '--bad_arg'");
    TEST_PARSE_FAIL("bad option '-b' at end", ParseOnly("-n", "-o", "5", "-b"),
                    "bad option '-b'");
    TEST_PARSE_FAIL("--test_enable_option with bad option",
                    ParseOnly("--test_enable_option_do_not_use", "invalid"),
                    "bad test option 'invalid'");
}

void test_test_option() {
    Options opts = std::get<Options>(Options::Parse({
        "--test_enable_option_do_not_use",
        "print_all_songs_and_exit",
    }));

    cmp_ok(opts.test.print_all_songs_and_exit, "==", true,
           "test_option: print_all_songs_and_exit is set");
}

int main() {
    plan(NO_PLAN);

    test_bad_option();
    test_bad_strtou();
    test_basic_long();
    test_basic_mixed_long_short();
    test_basic_short();
    test_default();
    test_file_stdin();
    test_help();
    test_partials();
    test_rule_basic();
    test_test_option();

    done_testing();
}
