#define _POSIX_C_SOURCE 201904L
#include <assert.h>
#include <mpd/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/list.h"
#include "src/util.h"

#include "mpdclient_fake.h"

#define UNUSED __attribute__((unused))

static struct {
    const char *field_name;
    enum mpd_tag_type tag;
} _MPD_IPARSE_EXPECTED;

/* Fake interface to match MPD */

enum mpd_tag_type mpd_tag_name_iparse(const char *field) {
    if (strcmp(_MPD_IPARSE_EXPECTED.field_name, field) != 0) {
        fprintf(stderr, "%s does not match expected value %s\n", field,
                _MPD_IPARSE_EXPECTED.field_name);
        return MPD_TAG_UNKNOWN;
    }
    return _MPD_IPARSE_EXPECTED.tag;
}

const char *mpd_song_get_tag(const struct mpd_song *song,
                             enum mpd_tag_type type, unsigned idx) {
    assert(idx == 0 && "mpdclient fake only supports sing-valued tags");
    return song->tag_values[type];
}

static struct mpd_connection *mpd_connection_swap(struct mpd_connection *c) {
    static struct mpd_connection *conn;
    struct mpd_connection *ret = conn;
    conn = c;
    return ret;
}

void mpd_set_connection(struct mpd_connection *c) {
    (void)mpd_connection_swap(c);
}

static struct {
    const char *host;
    unsigned port;
    unsigned delay;
} _MPD_SERVER;

void mpd_set_server(const char *host, unsigned port, unsigned delay) {
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

void mpd_connection_set_error(struct mpd_connection *c, enum mpd_error e,
                              const char *msg) {
    free((void *)c->error.msg);
    c->error.error = e;
    c->error.server_error = MPD_SERVER_ERROR_UNK;
    if (msg) {
        c->error.msg = xstrdup(msg);
    } else {
        c->error.msg = NULL;
    }
}

void mpd_connection_set_server_error(struct mpd_connection *c,
                                     enum mpd_server_error e, const char *msg) {
    free((void *)c->error.msg);
    c->error.server_error = e;
    c->error.error = MPD_ERROR_SERVER;
    if (msg) {
        c->error.msg = xstrdup(msg);
    } else {
        c->error.msg = NULL;
    }
}

void mpd_connection_free(struct mpd_connection *connection) {
    free((void *)connection->error.msg);
    list_free(&connection->pair_iter);
    list_free(&connection->song_iter);
    list_free(&connection->db);
    list_free(&connection->queue);
}

struct mpd_connection *mpd_connection_new(const char *host, unsigned port,
                                          unsigned timeout_ms) {
    struct mpd_connection *ret = mpd_connection_swap(NULL);
    // If we have no connection to return, just return immediately.
    if (ret == NULL) {
        return ret;
    }
    if (strcmp(host, _MPD_SERVER.host) != 0) {
        mpd_connection_set_error(ret, MPD_ERROR_RESOLVER, "host not found");
    } else if (port != _MPD_SERVER.port) {
        mpd_connection_set_error(ret, MPD_ERROR_RESOLVER, "port not found");
    } else if (_MPD_SERVER.delay > timeout_ms) {
        mpd_connection_set_error(ret, MPD_ERROR_TIMEOUT,
                                 "connection timed out");
    } else {
        mpd_connection_set_error(ret, MPD_ERROR_SUCCESS, "success");
    }
    return ret;
}

bool mpd_connection_clear_error(struct mpd_connection *connection) {
    switch (connection->error.error) {
        case MPD_ERROR_RESOLVER:
        case MPD_ERROR_TIMEOUT:
            return false;
        default:
            mpd_connection_set_error(connection, MPD_ERROR_SUCCESS, NULL);
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
    return connection->error.msg;
}

void mpd_song_free(struct mpd_song *song) { free(song); }

const char *mpd_song_get_uri(const struct mpd_song *song) { return song->uri; }

struct mpd_song *mpd_recv_song(struct mpd_connection *connection) {
    if (connection->song_iter.length == 0) {
        return NULL;
    }
    struct datum d;
    list_leak(&connection->song_iter, 0, &d);
    return (struct mpd_song *)d.data;
}

struct mpd_pair *mpd_recv_pair(struct mpd_connection *connection) {
    if (connection->pair_iter.length == 0) {
        return NULL;
    }
    struct datum d;
    list_leak(&connection->pair_iter, 0, &d);
    return (struct mpd_pair *)d.data;
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
    if ((song_pos + 1) > connection->queue.length) {
        mpd_connection_set_server_error(connection, MPD_SERVER_ERROR_ARG,
                                        "Bad song index");
        return false;
    }
    connection->state.play_state = MPD_STATE_PLAY;
    connection->state.queue_pos = song_pos;
    return true;
}

struct mpd_status *mpd_run_status(struct mpd_connection *c) {
    struct mpd_status *ret =
        (struct mpd_status *)xmalloc(sizeof(struct mpd_status));
    ret->queue_length = c->queue.length;
    ret->single = c->state.single;
    ret->song_pos = c->state.queue_pos;
    ret->state = c->state.play_state;

    return ret;
}

void mpd_status_free(struct mpd_status *status) { free(status); }

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

// Assumes that we've already set up the song_iter?
bool mpd_send_list_all_meta(UNUSED struct mpd_connection *connection,
                            const char *path) {
    assert(path == NULL && "only NULL path supported");
    return true;
}

// Assumes pair iter has been set up correctly.
bool mpd_send_disallowed_commands(UNUSED struct mpd_connection *connection) {
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

void mpd_return_pair(UNUSED struct mpd_connection *connection,
                     struct mpd_pair *pair) {
    free(pair);
}

static enum mpd_idle _IDLE;

void set_idle_result(enum mpd_idle idle) { _IDLE = idle; }

enum mpd_idle mpd_run_idle_mask(UNUSED struct mpd_connection *connection,
                                UNUSED enum mpd_idle mask) {
    return _IDLE;
}

bool mpd_run_add(struct mpd_connection *connection, const char *uri) {
    int found_idx = -1;
    for (unsigned i = 0; i < connection->db.length; i++) {
        struct mpd_song *song =
            (struct mpd_song *)list_at(&connection->db, i)->data;
        if (strcmp(song->uri, uri) == 0) {
            found_idx = (int)i;
            break;
        }
    }
    if (found_idx == -1) {
        mpd_connection_set_server_error(connection, MPD_SERVER_ERROR_NO_EXIST,
                                        "uri does not exist");
        return false;
    }
    list_push(&connection->queue, list_at(&connection->db, found_idx));
    return true;
}

bool mpd_run_password(struct mpd_connection *connection, const char *password) {
    if (strcmp(connection->password, password) == 0) {
        return true;
    }
    mpd_connection_set_server_error(connection, MPD_SERVER_ERROR_PASSWORD,
                                    "wrong password");
    return true;
}

/* Test helpers */

void set_tag_name_iparse_result(const char *check_name, enum mpd_tag_type t) {
    if (_MPD_IPARSE_EXPECTED.field_name != NULL) {
        free((void *)_MPD_IPARSE_EXPECTED.field_name);
    }
    _MPD_IPARSE_EXPECTED.field_name = xstrdup(check_name);
    _MPD_IPARSE_EXPECTED.tag = t;
}

struct mpd_song test_build_song(const char *uri, test_tag_value_t *vals,
                                size_t len) {
    struct mpd_song ret;
    memset(&ret, 0, sizeof(ret));
    ret.uri = uri;

    for (size_t i = 0; i < len; i++) {
        enum mpd_tag_type cur_tag = vals[i].tag;
        const char *cur_val = ret.tag_values[cur_tag];
        /* Check and make sure we haven't already set that value yet. */
        if (cur_val != NULL) {
            die("tag value %d already defined as %s", cur_tag, cur_val);
        }
        ret.tag_values[cur_tag] = vals[i].val;
    }
    return ret;
}

const struct mpd_song *mpd_playing(struct mpd_connection *c) {
    if (c->queue.length <= c->state.queue_pos) {
        return NULL;
    }
    return (const struct mpd_song *)list_at(&c->queue, c->state.queue_pos)
        ->data;
}
