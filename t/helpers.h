#include "t/mpdclient_fake.h"

#ifndef __T_HELPERS_H__
#define __T_HELPERS_H__

// Evaluates to the length of static array "a".
#define STATIC_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// Define a tag value on a song. Used with `TEST_SONG`.
#define TAG(name, value) \
    { (name), (value) }

// Define a test song with the given TAGs. Song is stored in the local variable
// `name`.
#define TEST_SONG(name, ...)                          \
    test_tag_value_t __tags_##name[] = {__VA_ARGS__}; \
    struct mpd_song name =                            \
        test_build_song(#name, __tags_##name, STATIC_ARRAY_LEN(__tags_##name))

#define TEST_SONG_URI(name) \
    struct mpd_song name = test_build_song(#name, NULL, 0)

#endif
