#ifndef __MPDCLIENT_FAKE_H__
#define __MPDCLIENT_FAKE_H__

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <mpd/client.h>

struct TagValue {
    enum mpd_tag_type tag;
    std::string value;
};

/* To fully-declare the type of mpd_song. */

struct mpd_song {
    std::string uri;
    std::unordered_map<enum mpd_tag_type, std::string> tags;

    // Build an empty song, with an empty URI, and no tags.
    mpd_song() : mpd_song(""){};

    // Build a song with only a URI, and an empty tag set.
    mpd_song(std::string_view uri) : mpd_song(uri, std::vector<TagValue>()){};

    // Build a song with the given URI, and tags.
    mpd_song(std::string_view uri, std::vector<TagValue>);
};

struct ConnectionError {
    std::string msg;
    enum mpd_error error;
    enum mpd_server_error server_error;
};

struct PlayerState {
    bool single;
    unsigned queue_pos;
    enum mpd_state play_state;

    PlayerState()
        : single(false), queue_pos(0), play_state(MPD_STATE_UNKNOWN) {}
};

struct mpd_connection {
    ConnectionError error;
    PlayerState state;
    // The password that mpd_run_password is check against.
    std::string password;

    std::deque<std::optional<struct mpd_pair>> pair_iter;
    std::deque<std::optional<struct mpd_song>> song_iter;
    std::deque<struct mpd_song> db;
    std::deque<struct mpd_song> queue;

    mpd_connection() { SetError(MPD_ERROR_SUCCESS, "success"); }

    // Set the given server error `e` and associated message. This will also
    // set the neccesary regular connection error as a server error.
    void SetServerError(enum mpd_server_error e, std::string msg) {
        error.error = MPD_ERROR_SERVER;
        error.server_error = e;
        error.msg = msg;
    }

    // Set the given error `e` and associated message on this connection.
    void SetError(enum mpd_error e, std::string msg) {
        error.error = e;
        error.server_error = MPD_SERVER_ERROR_UNK;
        error.msg = msg;
    }

    // Clear the error state of this connection, and set it to the default,
    // "success" status.
    void SetSuccess() { SetError(MPD_ERROR_SUCCESS, "success"); }

    // Get the currently playing song. If no song is playing, and empty
    // optional is returned.
    std::optional<struct mpd_song> Playing();
};

/* MPD Connection helpers */

// The the location of the MPD "server". Future calls to `mpd_connection_new'
// will only succeed if they match these values.
void SetServer(const std::string& host, unsigned port, unsigned delay);

// Sets the next connection that will be returned by mpd_connection_new. This
// allows the test harness to inject mpd_connections. MPD connections set
// using this function will only be returned once (from the next call to
// mpd_connection_new). If this function is not called before a call to
// mpd_connection_new, a NULL connection will be returned.
void SetConnection(struct mpd_connection& c);

// Set the value returned by mpd_tag_name_iparse. If the iparse'd name does
// not match `name`, then the unknown tag is returned, and an error is logged.
void SetTagNameIParse(std::string_view name, enum mpd_tag_type wanted);

// Set the enum that will be returned on subsequent calls to mpd_run_idle.
void SetIdle(enum mpd_idle);

#endif  // __MPDCLIENT_FAKE_H__
