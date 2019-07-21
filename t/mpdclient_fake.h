#include <mpd/client.h>

#ifndef MPDCLIENT_FAKE_H
#define MPDCLIENT_FAKE_H

/* To fully-declare the type of mpd_song. */

struct mpd_song {
    const char* uri;
    const char* tag_values[MPD_TAG_COUNT];
};

struct mpdfake_connection_error {
    const char* msg;
    enum mpd_error error;
    enum mpd_server_error server_error;
};

struct mpdfake_player_state {
    bool single;
    unsigned queue_pos;
    enum mpd_state play_state;
};

struct mpdfake_user {
    const char* password;
    struct list cmds;
};

struct mpdfake_auth {
    char* current_password;
    struct list users;
};

struct mpd_connection {
    struct mpdfake_connection_error error;
    struct mpdfake_player_state state;
    struct mpdfake_auth auth;
    struct list pair_iter;  // must contain struct mpd_pair
    struct list song_iter;  // must contain struct mpd_song
    struct list db;
    struct list queue;
};

/* constructors */

typedef struct {
    enum mpd_tag_type tag;
    const char* val;
} test_tag_value_t;

/* Build a song from a list of test tag values. */
struct mpd_song test_build_song(const char* uri, test_tag_value_t* vals,
                                size_t len);

/* Set the value returned by mpd_tag_name_iparse */
void set_tag_name_iparse_result(const char* name, enum mpd_tag_type wanted);

#endif
