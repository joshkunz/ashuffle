#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <tap.h>

#include "args.h"
#include "t/helpers.h"

void test_basic() {
    struct ashuffle_options opts;

    ashuffle_init(&opts);


}


int main() {
    plan(NO_PLAN);

    test_basic();

    done_testing();
}
