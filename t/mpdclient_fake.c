#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mpd/client.h>

#include "mpdclient_fake.h"

/* Fake interface to match MPD */

static enum mpd_tag_type _MPD_IPARSE_TAG_TYPE;

enum mpd_tag_type mpd_tag_name_iparse(const char * field __attribute__((unused))) {
    return _MPD_IPARSE_TAG_TYPE;
}

const char * mpd_song_get_tag(const struct mpd_song * song, enum mpd_tag_type type, unsigned idx) {
    assert(idx == 0 && "mpdclient fake only supports sing-valued tags");
    return song->tag_values[type];
}

/* Test helpers */

void set_tag_name_iparse_result(enum mpd_tag_type t) {
    _MPD_IPARSE_TAG_TYPE = t;
}

struct mpd_song test_build_song(test_tag_value_t * vals, size_t len) {
    struct mpd_song ret;
    memset(&ret, 0, sizeof(ret));

    for (size_t i = 0; i < len; i++) {
        enum mpd_tag_type cur_tag = vals[i].tag;
        const char * cur_val = ret.tag_values[cur_tag];
        /* Check and make sure we haven't already set that value yet. */
        if (cur_val != NULL) {
            fprintf(stderr, "tag value %d already defined as %s\n", cur_tag, cur_val);
            exit(1);
        }
        ret.tag_values[cur_tag] = vals[i].val;
    }
    return ret;
}
