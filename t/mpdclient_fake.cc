#include "mpdclient_fake.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include <absl/strings/str_format.h>
#include <mpd/capabilities.h>
#include <mpd/connection.h>
#include <mpd/database.h>
#include <mpd/pair.h>
#include <mpd/password.h>
#include <mpd/player.h>
#include <mpd/queue.h>
#include <mpd/recv.h>
#include <mpd/search.h>
#include <mpd/song.h>

#define UNUSED __attribute__((unused))

static struct {
    std::string field_name;
    enum mpd_tag_type tag;
} _MPD_IPARSE_EXPECTED;

void SetTagNameIParse(std::string_view name, enum mpd_tag_type wanted) {
    _MPD_IPARSE_EXPECTED.field_name = name;
    _MPD_IPARSE_EXPECTED.tag = wanted;
}

enum mpd_tag_type mpd_tag_name_iparse(const char *field) {
    if (_MPD_IPARSE_EXPECTED.field_name != field) {
        std::cerr << field << " does not match expected value "
                  << _MPD_IPARSE_EXPECTED.field_name << std::endl;
        return MPD_TAG_UNKNOWN;
    }
    return _MPD_IPARSE_EXPECTED.tag;
}

// Build a song with the given URI, and tags.
mpd_song::mpd_song(std::string_view in_uri, std::vector<TagValue> tvs) {
    uri = in_uri;
    for (const TagValue &t : tvs) {
        if (tags.find(t.tag) != tags.end()) {
            std::cerr << absl::StrFormat("tag value %d already defined", t.tag)
                      << std::endl;
        }
        tags[t.tag] = t.value;
    }
}

std::optional<struct mpd_song> mpd_connection::Playing() {
    if (queue.size() <= state.queue_pos) {
        return std::optional<struct mpd_song>();
    }
    return queue[state.queue_pos];
}

const char *mpd_song_get_tag(const struct mpd_song *song,
                             enum mpd_tag_type type, unsigned idx) {
    assert(idx == 0 && "mpdclient fake only supports sing-valued tags");
    auto found = song->tags.find(type);
    if (found == song->tags.end()) {
        return nullptr;
    }
    return found->second.data();
}

static struct mpd_connection *ConnectionSwap(struct mpd_connection *next) {
    static struct mpd_connection *conn;
    struct mpd_connection *ret = conn;
    conn = next;
    return ret;
}

void SetConnection(struct mpd_connection &c) { (void)ConnectionSwap(&c); }

static struct {
    std::string host;
    unsigned port;
    unsigned delay;
} _MPD_SERVER;

void SetServer(const std::string &host, unsigned port, unsigned delay) {
    _MPD_SERVER.host = host;
    _MPD_SERVER.port = port;
    _MPD_SERVER.delay = delay;
}

struct mpd_status {
    unsigned queue_length;
    bool single;
    int song_pos;
    enum mpd_state state;
};

struct mpd_connection *mpd_connection_new(const char *host, unsigned port,
                                          unsigned timeout_ms) {
    struct mpd_connection *ret = ConnectionSwap(nullptr);
    // If we have no connection to return, just return immediately.
    if (ret == nullptr) {
        return ret;
    }
    if (host != _MPD_SERVER.host) {
        ret->SetError(MPD_ERROR_RESOLVER, "host not found");
    } else if (port != _MPD_SERVER.port) {
        ret->SetError(MPD_ERROR_RESOLVER, "port not found");
    } else if (_MPD_SERVER.delay > timeout_ms) {
        ret->SetError(MPD_ERROR_TIMEOUT, "connection timed out");
    } else {
        ret->SetSuccess();
    }
    return ret;
}

bool mpd_connection_clear_error(struct mpd_connection *connection) {
    switch (connection->error.error) {
        case MPD_ERROR_RESOLVER:
        case MPD_ERROR_TIMEOUT:
            return false;
        default:
            connection->SetSuccess();
            return true;
    }
}

enum mpd_error mpd_connection_get_error(
    const struct mpd_connection *connection) {
    return connection->error.error;
}

enum mpd_server_error mpd_connection_get_server_error(
    const struct mpd_connection *connection) {
    return connection->error.server_error;
}

const char *mpd_connection_get_error_message(
    const struct mpd_connection *connection) {
    return connection->error.msg.data();
}

const char *mpd_song_get_uri(const struct mpd_song *song) {
    return song->uri.data();
}

struct mpd_song *mpd_recv_song(struct mpd_connection *connection) {
    if (connection->song_iter.empty()) {
        return nullptr;
    }
    struct mpd_song *result = nullptr;
    if (connection->song_iter.front()) {
        result = new struct mpd_song;
        *result = *connection->song_iter.front();
    }
    connection->song_iter.pop_front();
    return result;
}

void mpd_song_free(struct mpd_song *song) { delete song; }

struct mpd_pair *mpd_recv_pair(struct mpd_connection *connection) {
    if (connection->pair_iter.empty()) {
        return nullptr;
    }
    struct mpd_pair *result = nullptr;
    if (connection->pair_iter.front()) {
        result = new struct mpd_pair;
        *result = *connection->pair_iter.front();
    }
    connection->pair_iter.pop_front();
    return result;
}

void mpd_return_pair(UNUSED struct mpd_connection *connection,
                     struct mpd_pair *pair) {
    delete pair;
}

struct mpd_pair *mpd_recv_pair_named(struct mpd_connection *connection,
                                     UNUSED const char *name) {
    return mpd_recv_pair(connection);
}

bool mpd_run_pause(struct mpd_connection *connection, bool mode) {
    if (mode) {
        connection->state.play_state = MPD_STATE_PLAY;
    } else {
        connection->state.play_state = MPD_STATE_PAUSE;
    }
    return true;
}

bool mpd_run_play_pos(struct mpd_connection *connection, unsigned song_pos) {
    // +1 because song_pos is zero-indexed.
    if ((song_pos + 1) > connection->queue.size()) {
        connection->SetServerError(MPD_SERVER_ERROR_ARG, "Bad song index");
        return false;
    }
    connection->state.play_state = MPD_STATE_PLAY;
    connection->state.queue_pos = song_pos;
    return true;
}

struct mpd_status *mpd_run_status(struct mpd_connection *c) {
    struct mpd_status *result = new struct mpd_status;
    result->queue_length = c->queue.size();
    result->single = c->state.single;
    result->song_pos = c->state.queue_pos;
    result->state = c->state.play_state;
    return result;
}

void mpd_status_free(struct mpd_status *status) { delete status; }

unsigned mpd_status_get_queue_length(const struct mpd_status *status) {
    return status->queue_length;
}

bool mpd_status_get_single(const struct mpd_status *status) {
    return status->single;
}

int mpd_status_get_song_pos(const struct mpd_status *status) {
    return status->song_pos;
}

enum mpd_state mpd_status_get_state(const struct mpd_status *status) {
    return status->state;
}

bool mpd_send_list_all_meta(UNUSED struct mpd_connection *connection,
                            const char *path) {
    assert(path == nullptr && "only NULL path supported");
    // Assume that the song_iter has already been set up correctly.
    return true;
}

bool mpd_send_disallowed_commands(UNUSED struct mpd_connection *connection) {
    // Assume that the pari_iter has already been set up correctly.
    return true;
}

bool mpd_search_db_songs(UNUSED struct mpd_connection *connection,
                         UNUSED bool exact) {
    return true;
}

bool mpd_search_add_uri_constraint(UNUSED struct mpd_connection *connection,
                                   UNUSED enum mpd_operator oper,
                                   UNUSED const char *value) {
    // We assume that the test suite already set up song_iter correctly.
    return true;
}

bool mpd_search_commit(UNUSED struct mpd_connection *connection) {
    return true;
}

static enum mpd_idle _IDLE;

void SetIdle(enum mpd_idle idle) { _IDLE = idle; }

enum mpd_idle mpd_run_idle_mask(UNUSED struct mpd_connection *connection,
                                UNUSED enum mpd_idle mask) {
    return _IDLE;
}

bool mpd_run_add(struct mpd_connection *connection, const char *uri) {
    auto idx =
        std::find_if(connection->db.begin(), connection->db.end(),
                     [uri](const struct mpd_song &s) { return s.uri == uri; });

    if (idx == connection->db.end()) {
        connection->SetServerError(MPD_SERVER_ERROR_NO_EXIST,
                                   "uri does not exist");
        return false;
    }
    connection->queue.push_back(*idx);
    return true;
}

bool mpd_run_password(struct mpd_connection *connection, const char *password) {
    if (password != connection->password) {
        connection->SetServerError(MPD_SERVER_ERROR_PASSWORD, "wrong password");
    }
    return true;
}
