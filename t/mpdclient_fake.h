#include <mpd/client.h>

#ifndef MPDCLIENT_FAKE_H
#define MPDCLIENT_FAKE_H

extern enum mpd_tag_type MPD_IPARSE_TAG_TYPE;

struct mpd_song {
    const char * tag_values[MPD_TAG_COUNT];
};

#endif
