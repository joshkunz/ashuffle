#ifndef __ASHUFFLE_T_HELPERS_H__
#define __ASHUFFLE_T_HELPERS_H__

#include <vector>

#include "mpdclient_fake.h"

// Define a tag value on a song. Used with `TEST_SONG`.
#define TAG(name, value) \
    { (name), (value) }

// Define a test song with the given TAGs. Song is stored in the local variable
// `name`.
#define TEST_SONG(name, ...)                             \
    std::vector<TagValue> __tags_##name = {__VA_ARGS__}; \
    struct mpd_song name(#name, __tags_##name)

#endif
