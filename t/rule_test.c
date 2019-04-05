#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <tap.h>
#include <mpd/client.h>

#include "rule.h"
#include "list.h"

#include "t/mpdclient_fake.h"
#include "t/helpers.h"

#define TAG(name, value) {(name), (value)}

#define TEST_SONG(name, ...) \
    test_tag_value_t __tags_ ## name[] = { __VA_ARGS__ }; \
    struct mpd_song name = test_build_song(__tags_ ## name, STATIC_ARRAY_LEN(__tags_ ## name))

void test_basic() {
    struct song_rule rule;
    rule_init(&rule);

    set_tag_name_iparse_result(MPD_TAG_UNKNOWN);
    int res = rule_add_criteria(&rule, "don't care", "don't care");
    cmp_ok(res, "!=", 0, "add_criteria should fail on MPD_TAG_UNKNOWN");
    cmp_ok(rule.matchers.length, "==", 0, "no matchers after failed add_critera");

    set_tag_name_iparse_result(MPD_TAG_ARTIST);
    res = rule_add_criteria(&rule, "artist", "foo fighters");
    cmp_ok(res, "==", 0, "add_criteria with regular tag works");

    TEST_SONG(matching,
        TAG(MPD_TAG_ARTIST, "foo fighters"));
    TEST_SONG(non_matching,
        TAG(MPD_TAG_ARTIST, "some randy"));

    ok(!rule_match(&rule, &matching), "should exclude song with matching tag");
    ok(rule_match(&rule, &non_matching), "incldues song with non-matching tag");
}

int main() {
    plan(NO_PLAN);

    test_basic();

    done_testing();
}
