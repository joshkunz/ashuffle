#include <assert.h>
#include <mpd/client.h>

#include "mpdclient_fake.h"

enum mpd_tag_type MPD_IPARSE_TAG_TYPE;

enum mpd_tag_type mpd_tag_name_iparse(const char * field __attribute__((unused))) {
    return MPD_IPARSE_TAG_TYPE;
}

const char * mpd_song_get_tag(const struct mpd_song * song, enum mpd_tag_type type, unsigned idx) {
    assert(idx == 0 && "mpdclient fake only supports sing-valued tags");
    return song->tag_values[type];
}
