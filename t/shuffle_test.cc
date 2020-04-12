#include "shuffle.h"

#include <stdlib.h>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <absl/strings/str_cat.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::ContainerEq;
using ::testing::Each;
using ::testing::Range;
using ::testing::Values;
using ::testing::WhenSorted;

TEST(ShuffleChainTest, AddPick) {
    ShuffleChain chain;
    std::string test_str("test");

    chain.Add(test_str);

    EXPECT_EQ(chain.Pick(), test_str);
    EXPECT_EQ(chain.Pick(), test_str)
        << "could not double-pick from the same 1-item chain.";
}

MATCHER_P(IsInCollection, c, "") { return c.find(arg) != c.end(); }

TEST(ShuffleChainTest, PickN) {
    constexpr int test_rounds = 5000;
    const std::unordered_set<std::string> test_items{"item 1", "item 2",
                                                     "item 3"};

    ShuffleChain chain;

    for (auto& s : test_items) {
        chain.Add(s);
    }

    std::vector<std::string> picked;
    for (int i = 0; i < test_rounds; i++) {
        picked.emplace_back(chain.Pick());
    }

    EXPECT_THAT(picked, Each(IsInCollection(test_items)))
        << "ShuffleChain picked item not in chain!";
}

class WindowTest : public testing::TestWithParam<int> {
   public:
    ShuffleChain chain_;

    // This method is purely for documentation.
    int WindowSize() { return GetParam(); };

    void SetUp() override {
        chain_ = ShuffleChain(WindowSize());
        for (int i = 0; i < WindowSize(); i++) {
            chain_.Add(absl::StrCat("item ", i));
        }
    };
};

TEST_P(WindowTest, Repeats) {
    // The first window_size items should all be unique, so when we check the
    // length of "picked", it should match window_size.
    std::unordered_set<std::string> picked;
    for (int i = 0; i < WindowSize(); i++) {
        picked.insert(chain_.Pick());
    }

    EXPECT_EQ(picked.size(), static_cast<unsigned>(WindowSize()))
        << absl::StrCat("first ", WindowSize(), " items should be unique");

    // Since we only put in window_size songs, we should now be forced to get
    // a repeat by picking one more song.
    picked.insert(chain_.Pick());

    EXPECT_EQ(picked.size(), static_cast<unsigned>(WindowSize()))
        << "should have gotten a repeat by picking one more song";
}

INSTANTIATE_TEST_SUITE_P(SmallWindows, WindowTest, Range(1, 25 + 1));
INSTANTIATE_TEST_SUITE_P(BigWindows, WindowTest, Values(50, 99, 100, 1000));

// In this test we seed rand (srand) with a known value so we have
// deterministic randomness from `rand`. These values are known to be
// random according to `rand()` so we're just validating that `shuffle` is
// actually picking according to rand.
// Note: This test may break if we change how we store items in the list, or
//  how we index the song list when picking randomly. It's hard to test
//  that something is random :/.
TEST(ShuffleChainTest, IsRandom) {
    srand(4);

    ShuffleChain chain(2);

    chain.Add("test a");
    chain.Add("test b");
    chain.Add("test c");

    std::vector<std::string> want{"test b", "test c", "test a", "test b"};
    std::vector<std::string> got = {chain.Pick(), chain.Pick(), chain.Pick(),
                                    chain.Pick()};

    EXPECT_THAT(got, ContainerEq(want));
}

TEST(ShuffleChainTest, Items) {
    ShuffleChain chain(2);

    const std::vector<std::string> test_uris{"test a", "test b", "test c"};

    chain.Add(test_uris[0]);
    chain.Add(test_uris[1]);
    chain.Add(test_uris[2]);

    // This is a gross hack to ensure that we've initialized the window pool.
    // We want to make sure shuffle_chain also picks up songs in the window.
    // If the internel implementation of the chain changes, then this will
    // be OK, it just won't do anything.
    (void)chain.Pick();

    std::vector<std::string> got = chain.Items();

    EXPECT_THAT(got, WhenSorted(ContainerEq(test_uris)));
}
