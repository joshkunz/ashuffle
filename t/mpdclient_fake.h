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

/* MPD Connection helpers */

// Set the given server error `e` and associated message. This will also
// set the neccesary regular connection error as a server error.
void mpd_connection_set_server_error(struct mpd_connection* c,
                                     enum mpd_server_error e, const char* msg);

// Set the given error `e` and associated message on this connection.
void mpd_connection_set_error(struct mpd_connection* c, enum mpd_error e,
                              const char* msg);

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

// Set the enum that will be returned on subsequent calls to mpd_run_idle
void set_idle_result(enum mpd_idle);

// Return the currently playing song on the MPD connection.
const struct mpd_song* mpd_playing(struct mpd_connection* c);

#endif
