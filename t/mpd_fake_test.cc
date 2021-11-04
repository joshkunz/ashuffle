#include <iostream>
#include <optional>
#include <vector>

#include <mpd/tag.h>

#include "t/mpd_fake.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::Eq;
using ::testing::Optional;

TEST(MPD, ListAllMetadataOmit) {
    // Test that even if we provide songs with metadata to a fake MPD, it
    // will hide that metadata in the produced songs.
    std::vector<fake::Song> songs = {
        fake::Song("first", {{MPD_TAG_ALBUM, "album_a"}}),
        fake::Song("second", {{MPD_TAG_ALBUM, "album_a"}}),
    };

    fake::MPD mpd;
    for (auto &song : songs) {
        mpd.db.push_back(song);
    }

    std::unique_ptr<mpd::SongReader> reader =
        mpd.ListAll(fake::MPD::MetadataOption::kOmit);

    ASSERT_FALSE(reader->Done()) << "The reader should have at least one song";

    while (!reader->Done()) {
        auto song = reader->Next();
        ASSERT_TRUE(song.has_value())
            << "Song returned from Next should be set";
        EXPECT_EQ((*song)->Tag(MPD_TAG_ALBUM), std::nullopt)
            << "Song should not have an album tag set";
    }

    EXPECT_THAT(songs[0].Tag(MPD_TAG_ALBUM), Optional(std::string("album_a")))
        << "Original song should not be mutated.";
}
