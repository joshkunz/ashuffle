#include <istream>
#include <memory>
#include <sstream>

#include "args.h"
#include "load.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

#include "t/mpd_fake.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::ContainerEq;
using ::testing::WhenSorted;

TEST(MPDLoaderTest, Basic) {
    fake::MPD mpd;
    mpd.db.emplace_back("song_a");
    mpd.db.emplace_back("song_b");

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    MPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset);
    loader.Load(&chain);

    std::vector<std::vector<std::string>> want = {{"song_a"}, {"song_b"}};
    EXPECT_THAT(chain.Items(), WhenSorted(ContainerEq(want)));
}

TEST(MPDLoaderTest, WithFilter) {
    fake::MPD mpd;

    mpd.db.push_back(fake::Song("song_a", {{MPD_TAG_ARTIST, "__artist__"}}));
    mpd.db.push_back(
        fake::Song("song_b", {{MPD_TAG_ARTIST, "__not_artist__"}}));
    mpd.db.push_back(fake::Song("song_c", {{MPD_TAG_ARTIST, "__artist__"}}));

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    Rule rule;
    // Exclude all songs with the artist "__not_artist__".
    rule.AddPattern(MPD_TAG_ARTIST, "__not_artist__");
    ruleset.push_back(rule);

    MPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset);
    loader.Load(&chain);

    std::vector<std::vector<std::string>> want = {{"song_a"}, {"song_c"}};
    EXPECT_THAT(chain.Items(), WhenSorted(ContainerEq(want)));
}

TEST(MPDLoaderTest, WithGroup) {
    fake::MPD mpd;
    mpd.db.push_back(fake::Song("song_a", {{MPD_TAG_ALBUM, "__album__"}}));
    mpd.db.push_back(fake::Song("song_b", {{MPD_TAG_ALBUM, "__album__"}}));

    std::vector<enum mpd_tag_type> group_by = {MPD_TAG_ARTIST};

    ShuffleChain chain;
    std::vector<Rule> ruleset;

    MPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset, group_by);
    loader.Load(&chain);

    std::vector<std::string> want = {"song_a", "song_b"};
    EXPECT_THAT(chain.Pick(), WhenSorted(ContainerEq(want)));
}

std::unique_ptr<std::istream> TestStream(std::vector<std::string> lines) {
    return std::make_unique<std::istringstream>(absl::StrJoin(lines, "\n"));
}

TEST(FileLoaderTest, Basic) {
    ShuffleChain chain;
    fake::Song song_a("song_a"), song_b("song_b"), song_c("song_c");

    std::unique_ptr<std::istream> s = TestStream({
        song_a.URI(),
        song_b.URI(),
        song_c.URI(),
    });

    FileLoader loader(s.get());
    loader.Load(&chain);

    std::vector<std::vector<std::string>> want = {
        {song_a.URI()}, {song_b.URI()}, {song_c.URI()}};

    EXPECT_THAT(chain.Items(), WhenSorted(ContainerEq(want)));
}

TEST(FileMPDLoaderTest, Basic) {
    // step 1. Initialize the MPD connection.
    fake::MPD mpd;

    // step 2. Build the ruleset, and add an exclusions for __not_artist__
    std::vector<Rule> ruleset;

    Rule artist_match;
    // Exclude all songs with the artist "__not_artist__".
    artist_match.AddPattern(MPD_TAG_ARTIST, "__not_artist__");
    ruleset.push_back(artist_match);

    // step 3. Prepare the shuffle_chain.
    ShuffleChain chain;

    // step 4. Prepare our songs/song list. The song_list will be used for
    // subsequent calls to `mpd_recv_song`.
    fake::Song song_a("song_a", {{MPD_TAG_ARTIST, "__artist__"}});
    fake::Song song_b("song_b", {{MPD_TAG_ARTIST, "__not_artist__"}});
    fake::Song song_c("song_c", {{MPD_TAG_ARTIST, "__artist__"}});
    // This song will not be present in the MPD library, so it doesn't need
    // any tags.
    fake::Song song_d("song_d");

    mpd.db.push_back(song_a);
    mpd.db.push_back(song_b);
    mpd.db.push_back(song_c);
    // Don't push song_d, so we can validate that only songs in the MPD
    // library are allowed.
    // mpd.db.push_back(song_d)

    // step 5. Set up our test input file, but writing the URIs of our songs.
    std::unique_ptr<std::istream> s = TestStream({
        song_a.URI(),
        song_b.URI(),
        song_c.URI(),
        // But we do want to write song_d here, so that ashuffle has to check
        // it.
        song_d.URI(),
    });

    // step 6. Run! (and validate)
    std::vector<enum mpd_tag_type> group_by;
    FileMPDLoader loader(static_cast<mpd::MPD *>(&mpd), ruleset, group_by,
                         s.get());
    loader.Load(&chain);

    std::vector<std::vector<std::string>> want = {{song_a.URI()},
                                                  {song_c.URI()}};
    EXPECT_THAT(chain.Items(), WhenSorted(ContainerEq(want)));
}
