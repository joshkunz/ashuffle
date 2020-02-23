#include <stdlib.h>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <absl/strings/str_cat.h>
#include <tap.h>

#include "shuffle.h"

using namespace ashuffle;

void test_basic() {
    ShuffleChain chain;
    std::string test_str("test");
    chain.Add(test_str);
    ok(chain.Pick() == test_str, "pick returns only string");

    /* Bit of explanation here: If we have a 1-item chain, it should
     * be OK to run `pick` twice (having it return the same string both
     * times). */
    lives_ok({ (void)chain.Pick(); }, "double-pick 1-item chain");
    ok(chain.Pick() == test_str, "double-pick on 1-item chain matches");
}

void test_multi() {
    constexpr unsigned test_rounds = 5000;
    const std::unordered_set<std::string> test_items{"item 1", "item 2",
                                                     "item 3"};

    ShuffleChain chain;

    for (auto s : test_items) {
        chain.Add(s);
    }

    for (unsigned i = 0; i < 5000; i++) {
        std::string item = chain.Pick();
        if (test_items.find(item) == test_items.end()) {
            fail("pick %u rounds", test_rounds);
            diag("  fail on round: %u", i);
            diag("  input: \"%s\"", item.data());
            return;
        }
    }
    pass("pick %u rounds", test_rounds);
}

void test_window_of_size(const unsigned window_size) {
    ShuffleChain chain(window_size);

    for (unsigned i = 0; i < window_size; i++) {
        chain.Add(absl::StrCat("item ", i));
    }

    // The first window_size items should all be unique, so when we check the
    // length of "picked", it should match window_size.
    std::unordered_set<std::string> picked;
    for (unsigned i = 0; i < window_size; i++) {
        picked.insert(chain.Pick());
    }

    ok(picked.size() == window_size,
       "pick window_size (%u) items, all are unique", window_size);

    // Since we only put in window_size songs, we should now be forced to get
    // a repeat by picking one more song.
    picked.insert(chain.Pick());
    ok(picked.size() == window_size,
       "pick window_size (%u) + 1 items, there is one repeat", window_size);
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

    ShuffleChain chain(2);

    chain.Add("test a");
    chain.Add("test b");
    chain.Add("test c");

    ok(chain.Pick() == "test b", "pick 1 is random");
    ok(chain.Pick() == "test c", "pick 2 is random");
    ok(chain.Pick() == "test a", "pick 3 is random");
    ok(chain.Pick() == "test b", "pick 4 is random");
}

void test_items() {
    ShuffleChain chain(2);

    const std::vector<std::string> test_uris{"test a", "test b", "test c"};

    chain.Add(test_uris[0]);
    chain.Add(test_uris[1]);
    chain.Add(test_uris[2]);

    // This is a gross hack to ensure that we've initialized the window pool.
    // We want to make sure shuffle_chain also picks up songs in the window.
    (void)chain.Pick();

    std::vector<std::string> got = chain.Items();
    cmp_ok(got.size(), "==", 3, "items: shuffle chain should have 3 items");
    std::sort(got.begin(), got.end());

    ok(test_uris == got,
       "items: shuffle chain should only contain inserted items");
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
