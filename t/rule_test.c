#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <tap.h>
#include <mpd/client.h>

#include "mpdclient_fake.h"

#include "rule.h"

void test_basic() {
    pass("worked!");
}

int main() {
    plan(NO_PLAN);

    test_basic();

    done_testing();
}
