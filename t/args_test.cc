#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <mpd/client.h>
#include <tap.h>

#include "args.h"
#include "list.h"
#include "rule.h"
#include "util.h"

#include "t/helpers.h"
#include "t/mpdclient_fake.h"

struct options_parse_result parse_only(const char *first, ...) {
    struct ashuffle_options opts;
    options_init(&opts);

    struct list args;
    list_init(&args);

    list_push_str(&args, first);

    va_list rest;
    va_start(rest, first);
    while (true) {
        const char *next = va_arg(rest, const char *);
        if (next == NULL) {
            break;
        }
        list_push_str(&args, next);
    }
    va_end(rest);

    const char **args_arr =
        (const char **)xmalloc(sizeof(const char *) * args.length);
    for (unsigned i = 0; i < args.length; i++) {
        args_arr[i] = list_at_str(&args, i);
    }

    struct options_parse_result res =
        options_parse(&opts, args.length, args_arr);

    free(args_arr);
    list_free(&args);
    options_free(&opts);

    return res;
}

#define PARSE_ONLY(...) parse_only(__VA_ARGS__, NULL)

void test_default() {
    struct ashuffle_options opts;

    options_init(&opts);

    struct options_parse_result res = options_parse(&opts, 0, NULL);
    cmp_ok(res.status, "==", PARSE_OK, "empty parse works");
    options_parse_result_free(&res);

    ok(opts.ruleset.empty(), "no rules by default");
    cmp_ok(opts.queue_only, "==", 0, "no 'queue only' by default");
    ok(opts.file_in == NULL, "no input file by default");
    cmp_ok(opts.check_uris, "==", true, "check_uris on by default");
    cmp_ok(opts.queue_buffer, "==", ARGS_QUEUE_BUFFER_NONE,
           "no queue buffer by default");
    ok(opts.host == NULL, "no host by default");
    cmp_ok(opts.port, "==", 0, "no port by default");
    cmp_ok(opts.test.print_all_songs_and_exit, "==", false,
           "no print_all_songs_and_exit by default");

    options_free(&opts);
}

void test_basic_short() {
    struct ashuffle_options opts;

    options_init(&opts);

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    const char *test_args[] = {
        "-o", "5",         "-n",          "-q",     "10",
        "-e", "artist",    "test artist", "artist", "another one",
        "-f", "/dev/zero", "-p",          "1234",
    };
    struct options_parse_result res =
        options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
    cmp_ok(res.status, "==", PARSE_OK, "basic parse works OK");
    options_parse_result_free(&res);

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic short detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic short queue only");
    ok(opts.file_in != NULL, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic short nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic short queue buffer");
    cmp_ok(opts.port, "==", 1234);

    options_free(&opts);
}

void test_basic_long() {
    struct ashuffle_options opts;

    options_init(&opts);

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    const char *test_args[] = {
        "--only",    "5",           "--no-check",     "--file",
        "/dev/zero", "--exclude",   "artist",         "test artist",
        "artist",    "another one", "--queue-buffer", "10",
        "--host",    "foo",         "--port",         "1234",
    };
    struct options_parse_result res =
        options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
    cmp_ok(res.status, "==", PARSE_OK, "basic parse works OK");
    options_parse_result_free(&res);

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic long detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic long queue only");
    ok(opts.file_in != NULL, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic long nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic long queue buffer");
    is(opts.host, "foo", "basic long host");
    cmp_ok(opts.port, "==", 1234, "basic long port");

    options_free(&opts);
}

void test_basic_mixed_long_short() {
    struct ashuffle_options opts;

    options_init(&opts);

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    const char *test_args[] = {
        "-o", "5",         "--file", "/dev/zero",   "-n",     "--queue-buffer",
        "10", "--exclude", "artist", "test artist", "artist", "another one",
    };
    struct options_parse_result res =
        options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
    cmp_ok(res.status, "==", PARSE_OK, "basic parse works OK");
    options_parse_result_free(&res);

    cmp_ok(opts.ruleset.size(), ">=", 1, "basic mixed detected rule");
    cmp_ok(opts.queue_only, "==", 5, "basic mixed queue only");
    ok(opts.file_in != NULL, "basic file in present");
    cmp_ok(opts.check_uris, "==", false, "basic mixed nocheck");
    cmp_ok(opts.queue_buffer, "==", 10, "basic mixed queue buffer");

    options_free(&opts);
}

void test_bad_strtou() {
    struct options_parse_result res;

    res = PARSE_ONLY("--only", "0x5.0");
    cmp_ok(res.status, "==", PARSE_FAILURE, "bad strtou fails parse");
    like(res.msg, "couldn't convert .* '0x5\\.0' .*");
    options_parse_result_free(&res);

    res = PARSE_ONLY("--queue-buffer", "20U");
    cmp_ok(res.status, "==", PARSE_FAILURE, "bad strtou fails parse");
    like(res.msg, "couldn't convert .* '20U' .*");
    options_parse_result_free(&res);
}

void test_rule_basic() {
    struct ashuffle_options opts;
    options_init(&opts);
    const char *test_args[] = {"-e", "artist", "__artist__"};

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    struct options_parse_result res =
        options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
    cmp_ok(res.status, "==", PARSE_OK, "parse basic rule OK");
    options_parse_result_free(&res);

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

    options_free(&opts);
}

void test_file_stdin() {
    const char *file_flags[] = {"-f", "--file"};

    for (unsigned i = 0; i < STATIC_ARRAY_LEN(file_flags); i++) {
        const char *flag_under_test = file_flags[i];
        struct ashuffle_options opts;
        options_init(&opts);

        const char *test_args[] = {flag_under_test, "-"};
        struct options_parse_result res =
            options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
        cmp_ok(res.status, "==", PARSE_OK, "[%s] parse works OK",
               flag_under_test);
        options_parse_result_free(&res);

        ok(opts.file_in == stdin, "%s parses as stdin", flag_under_test);

        options_free(&opts);
    }
}

void test_partials() {
#define TEST_PARTIAL(name, _res, err_match)              \
    do {                                                 \
        struct options_parse_result res = (_res);        \
        cmp_ok(res.status, "==", PARSE_FAILURE,          \
               "partial " name " should fail to parse"); \
        like(res.msg, err_match);                        \
        options_parse_result_free(&res);                 \
    } while (0)

    SetTagNameIParse("artist", MPD_TAG_ARTIST);

    TEST_PARTIAL("only short", PARSE_ONLY("-o"),
                 "no argument supplied for '-o'");
    TEST_PARTIAL("only long", PARSE_ONLY("--only"),
                 "no argument supplied for '--only'");

    TEST_PARTIAL("file short", PARSE_ONLY("-f"),
                 "no argument supplied for '-f'");
    TEST_PARTIAL("file long", PARSE_ONLY("--file"),
                 "no argument supplied for '--file'");

    TEST_PARTIAL("queue buffer short", PARSE_ONLY("-q"),
                 "no argument supplied for '-q'");
    TEST_PARTIAL("queue buffer long", PARSE_ONLY("--queue-buffer"),
                 "no argument supplied for '--queue-buffer'");

    TEST_PARTIAL("exclude short no arg", PARSE_ONLY("-e"),
                 "no argument supplied for '-e'");
    TEST_PARTIAL("exclude short only match", PARSE_ONLY("-e", "artist"),
                 "no value supplied for match 'artist'");
    TEST_PARTIAL("exclude short only match mutli",
                 PARSE_ONLY("-e", "artist", "whatever", "artist"),
                 "no value supplied for match 'artist'");

    TEST_PARTIAL("exclude long no arg", PARSE_ONLY("--exclude"),
                 "no argument supplied for '--exclude'");
    TEST_PARTIAL("exclude long only match", PARSE_ONLY("--exclude", "artist"),
                 "no value supplied for match 'artist'");
    TEST_PARTIAL("exclude long only match mutli",
                 PARSE_ONLY("--exclude", "artist", "whatever", "artist"),
                 "no value supplied for match 'artist'");

    TEST_PARTIAL("host long", PARSE_ONLY("--host"),
                 "no argument supplied for '--host'");
    TEST_PARTIAL("port long", PARSE_ONLY("--port"),
                 "no argument supplied for '--port'");
    TEST_PARTIAL("port short ", PARSE_ONLY("-p"),
                 "no argument supplied for '-p'");

    TEST_PARTIAL("test option no arg",
                 PARSE_ONLY("--test_enable_option_do_not_use"),
                 "no argument supplied for '--test_enable_option_do_not_use'");
}

void test_help() {
    const char *help_flags[] = {"-h", "--help", "-?"};

    for (unsigned i = 0; i < STATIC_ARRAY_LEN(help_flags); i++) {
        const char *flag_under_test = help_flags[i];

        struct options_parse_result res = PARSE_ONLY(flag_under_test);
        cmp_ok(res.status, "==", PARSE_HELP, "[%s] parses as help",
               flag_under_test);
        options_parse_result_free(&res);
    }
}

void test_bad_option() {
    struct options_parse_result res;

    res = PARSE_ONLY("blah");
    cmp_ok(res.status, "==", PARSE_FAILURE, "blah fails to parse");
    like(res.msg, "bad option 'blah'");
    options_parse_result_free(&res);

    res = PARSE_ONLY("-n", "--bad_arg", "-o", "5");
    cmp_ok(res.status, "==", PARSE_FAILURE,
           "--bad_arg in middle fails full parse");
    like(res.msg, "bad option '--bad_arg'");
    options_parse_result_free(&res);

    res = PARSE_ONLY("-n", "-o", "5", "-b");
    cmp_ok(res.status, "==", PARSE_FAILURE,
           "bad arg -b at end fails full parse");
    like(res.msg, "bad option '-b'");
    options_parse_result_free(&res);

    res = PARSE_ONLY("--test_enable_option_do_not_use", "invalid");
    cmp_ok(res.status, "==", PARSE_FAILURE,
           "test_enable_option fails to parse with bad option");
    like(res.msg, "bad test option 'invalid'");
    options_parse_result_free(&res);
}

void test_test_option() {
    struct ashuffle_options opts;

    options_init(&opts);

    const char *test_args[] = {"--test_enable_option_do_not_use",
                               "print_all_songs_and_exit"};

    struct options_parse_result res =
        options_parse(&opts, STATIC_ARRAY_LEN(test_args), test_args);
    cmp_ok(res.status, "==", PARSE_OK, "parsing test_option_works");
    options_parse_result_free(&res);

    cmp_ok(opts.test.print_all_songs_and_exit, "==", true,
           "test_option: print_all_songs_and_exit is set");

    options_free(&opts);
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
