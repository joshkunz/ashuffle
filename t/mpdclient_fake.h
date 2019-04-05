#include <mpd/client.h>

#ifndef MPDCLIENT_FAKE_H
#define MPDCLIENT_FAKE_H

/* To fully-declare the type of mpd_song. */

struct mpd_song {
    const char * tag_values[MPD_TAG_COUNT];
};

/* constructors */

typedef struct {
    enum mpd_tag_type tag;
    const char * val;
} test_tag_value_t;

/* Build a song from a list of test tag values. */
struct mpd_song test_build_song(test_tag_value_t * vals, size_t len);

/* Set the value returned by mpd_tag_name_iparse */
void set_tag_name_iparse_result(enum mpd_tag_type wanted);

#endif
