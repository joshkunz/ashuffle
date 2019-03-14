#include <stdbool.h>
#include <string.h>

#include <tap.h>

#include "shuffle.h"

void test_basic() {
    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    const char * test_str = "test";
    shuffle_add(&chain, test_str);
    is(shuffle_pick(&chain), test_str, "pick returns only string");

    /* Bit of explanation here: If we have a 1-item chain, it should
     * be OK to run `pick` twice (having it return the same string both
     * times). */
    lives_ok({ (void) shuffle_pick(&chain); }, "double-pick 1-item chain");
    is(shuffle_pick(&chain), test_str, "double-pick on 1-item chain matches");
    shuffle_free(&chain);
}

int main() {
    plan(NO_PLAN);

    test_basic();

    done_testing();
}
