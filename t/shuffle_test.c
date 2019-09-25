#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <tap.h>

#include "list.h"
#include "shuffle.h"
#include "util.h"

#include "t/helpers.h"

void test_basic() {
    struct shuffle_chain chain;
    shuffle_init(&chain, 1);
    const char* test_str = "test";
    shuffle_add(&chain, test_str);
    is(shuffle_pick(&chain), test_str, "pick returns only string");

    /* Bit of explanation here: If we have a 1-item chain, it should
     * be OK to run `pick` twice (having it return the same string both
     * times). */
    lives_ok({ (void)shuffle_pick(&chain); }, "double-pick 1-item chain");
    is(shuffle_pick(&chain), test_str, "double-pick on 1-item chain matches");
    shuffle_free(&chain);
}

// Returns true if the list `lst` contains the string `a`.
bool check_contains(const char* a, const char* lst[], size_t lst_len) {
    for (size_t i = 0; i < lst_len; i++) {
        if (strcmp(a, lst[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Returns true if all items in `lst` are unique.
bool check_unique(const char* lst[], size_t lst_len) {
    for (size_t i = 0; i < lst_len; i++) {
        for (size_t j = i + 1; j < lst_len; j++) {
            if (strcmp(lst[i], lst[j]) == 0) {
                return false;
            }
        }
    }
    return true;
}

// Returns true if `lst_a` and `lst_b` contain identical strings.
bool check_array_equal_str(const char* lst_a[], size_t lst_a_len,
                           const char* lst_b[], size_t lst_b_len) {
    if (lst_a_len != lst_b_len) {
        return false;
    }
    for (size_t i = 0; i < lst_a_len; i++) {
        if (strcmp(lst_a[i], lst_b[i]) != 0) {
            diag("  Arrays differ at index %u", i);
            return false;
        }
    }
    return true;
}

void test_multi() {
    unsigned test_rounds = 5000;

    struct shuffle_chain chain;
    shuffle_init(&chain, 1);

    const char* test_items[] = {"item 1", "item 2", "item 3"};

    shuffle_add(&chain, test_items[0]);
    shuffle_add(&chain, test_items[1]);
    shuffle_add(&chain, test_items[2]);

    for (unsigned i = 0; i < 5000; i++) {
        const char* item = shuffle_pick(&chain);
        if (!check_contains(item, test_items, 3)) {
            fail("pick %u rounds", test_rounds);
            diag("  fail on round: %u", i);
            diag("  input: \"%s\"", item);
            goto fail;
        }
    }
    pass("pick %u rounds", test_rounds);
fail:
    shuffle_free(&chain);
}

void test_window_of_size(const unsigned window_size) {
    struct shuffle_chain chain;
    shuffle_init(&chain, window_size);

    for (unsigned i = 0; i < window_size; i++) {
        char* item = xsprintf("item %u", i);
        shuffle_add(&chain, item);
        free(item);
    }

    // Our buffer of picked items needs to be one larger than the window size
    // so we can test that there is at least one repeat.
    const char* picked[window_size + 1];

    for (unsigned i = 0; i < window_size + 1; i++) {
        picked[i] = shuffle_pick(&chain);
    }

    // Just looking at the first `window_size` items, they should all be
    // unique, since they came out of the window.
    ok(check_unique(picked, window_size),
       "pick window_size (%u) items, all are unique", window_size);

    // If we check all items there should be at least one repeat. Since
    // we just checked that the first three items are unique, then we know
    // that the last item is the repeat, as we expect.
    ok(!check_unique(picked, window_size + 1),
       "pick window_size (%u) + 1 items, there is one repeat", window_size);

    shuffle_free(&chain);
}

void test_windowing() {
    // test all the small windows.
    for (unsigned i = 1; i <= 25; i++) {
        test_window_of_size(i);
    }
    // Test a couple big, round windows.
    test_window_of_size(50);
    test_window_of_size(100);
}

// In this test we seed rand (srand) with a known value so we have
// deterministic randomness from `rand`. These values are known to be
// random according to `rand()` so we're just validating that `shuffle` is
// actually picking according to rand.
// Note: This test may break if we change how we store items in the list, or
//  how we index the song list when picking randomly. It's hard to test
//  that something is random :/.
void test_random() {
    srand(4);

    struct shuffle_chain chain;
    shuffle_init(&chain, 2);

    shuffle_add(&chain, "test a");
    shuffle_add(&chain, "test b");
    shuffle_add(&chain, "test c");

    is(shuffle_pick(&chain), "test b", "pick 1 is random");
    is(shuffle_pick(&chain), "test c", "pick 2 is random");
    is(shuffle_pick(&chain), "test a", "pick 3 is random");
    is(shuffle_pick(&chain), "test b", "pick 4 is random");

    shuffle_free(&chain);
}

void test_items() {
    struct shuffle_chain chain;
    shuffle_init(&chain, 2);

    const char* test_uris[] = {"test a", "test b", "test c"};

    shuffle_add(&chain, test_uris[0]);
    shuffle_add(&chain, test_uris[1]);
    shuffle_add(&chain, test_uris[2]);

    // This is a gross hack to ensure that we've initialized the window pool.
    // We want to make sure shuffle_chain also picks up songs in the window.
    (void)shuffle_pick(&chain);

    struct list got;
    list_init(&got);

    shuffle_items(&chain, &got);

    cmp_ok(got.length, "==", 3, "items: shuffle chain should have 3 items");

    // In-order to qsort, we need an array, not a list.
    const char** res_array = xmalloc(sizeof(char*) * got.length);
    for (unsigned i = 0; i < got.length; i++) {
        res_array[i] = list_at_str(&got, i);
    }

    qsort_str(test_uris, STATIC_ARRAY_LEN(test_uris));
    qsort_str(res_array, got.length);

    ok(check_array_equal_str(test_uris, STATIC_ARRAY_LEN(test_uris), res_array,
                             got.length),
       "items: shuffle chain should only contain inserted items");

    free(res_array);
    list_free(&got);
    shuffle_free(&chain);
}

int main() {
    plan(NO_PLAN);

    test_basic();
    test_multi();
    test_windowing();
    test_random();
    test_items();

    done_testing();
}
