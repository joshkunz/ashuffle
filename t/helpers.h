#ifndef __T_HELPERS_H__
#define __T_HELPERS_H__

#include <vector>

#include "mpdclient_fake.h"

// Evaluates to the length of static array "a".
#define STATIC_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// Define a tag value on a song. Used with `TEST_SONG`.
#define TAG(name, value) \
    { (name), (value) }

// Define a test song with the given TAGs. Song is stored in the local variable
// `name`.
#define TEST_SONG(name, ...)                             \
    std::vector<TagValue> __tags_##name = {__VA_ARGS__}; \
    struct mpd_song name(#name, __tags_##name)

#endif
