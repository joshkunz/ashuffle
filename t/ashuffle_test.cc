#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <mpd/error.h>
#include <mpd/idle.h>
#include <mpd/pair.h>
#include <mpd/status.h>
#include <mpd/tag.h>
#include <tap.h>

#include "args.h"
#include "ashuffle.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

#include "t/mpd_fake.h"

using namespace ashuffle;

void xclearenv() {
    if (clearenv()) {
        perror("xclearenv");
        abort();
    }
}

void xsetenv(const char *key, const char *value) {
    if (setenv(key, value, 1) != 0) {
        perror("xsetenv");
        abort();
    }
}

TestDelegate init_only_d = {
    .skip_init = false,
    .until_f = [] { return false; },
};

TestDelegate loop_once_d{
    .skip_init = true,
    .until_f =
        [] {
            static unsigned count;
            // Returns true, false, true, false. This is so we can re-use the
            // same delegate in multiple tests.
            return (count++ % 2) == 0;
        },
};

void test_shuffle_loop_init_empty() {
    fake::MPD mpd;

    fake::Song song_a("song_a");
    mpd.db.push_back(song_a);

    Options options;

    ShuffleChain chain;
    chain.Add(song_a.URI());

    shuffle_loop(&mpd, &chain, options, init_only_d);

    cmp_ok(mpd.queue.size(), "==", 1,
           "shuffle_loop_init_empty: added one song to queue");
    ok(mpd.state.playing, "shuffle_loop_init_empty: playing after init");
    ok(mpd.state.song_position == 0,
       "shuffle_loop_init_empty: queue position on first song");
}

void test_shuffle_loop_init_playing() {
    fake::MPD mpd;
    fake::Song song_a("song_a");
    mpd.db.push_back(song_a);

    ShuffleChain chain;
    chain.Add(song_a.URI());

    // Pretend like we already have a song in our queue, and we're playing.
    mpd.queue.push_back(song_a);
    mpd.PlayAt(0);

    shuffle_loop(&mpd, &chain, Options(), init_only_d);

    // We shouldn't add anything to the queue if we're already playing,
    // ashuffle should start silently.
    cmp_ok(mpd.queue.size(), "==", 1,
           "shuffle_loop_init_playing: no songs added to queue");
    ok(mpd.state.playing, "shuffle_loop_init_playing: playing after init");
    ok(mpd.state.song_position == 0,
       "shuffle_loop_init_playing: queue position on first song");
}

void test_shuffle_loop_init_stopped() {
    fake::MPD mpd;

    fake::Song song_a("song_a"), song_b("song_b");
    mpd.db.push_back(song_a);
    mpd.db.push_back(song_b);

    ShuffleChain chain;
    chain.Add(song_a.URI());

    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    mpd.queue.push_back(song_b);
    mpd.state.song_position = 0;
    mpd.state.playing = false;

    shuffle_loop(&mpd, &chain, Options(), init_only_d);

    // We should add a new item to the queue, and start playing.
    cmp_ok(mpd.queue.size(), "==", 2,
           "shuffle_loop_init_stopped: added one song to queue");
    ok(mpd.state.playing, "shuffle_loop_init_stopped: playing after init");
    ok(mpd.state.song_position == 1,
       "shuffle_loop_init_stopped: queue position on second song");
}

void test_shuffle_loop_basic() {
    fake::MPD mpd;

    fake::Song song_a("song_a"), song_b("song_b");
    mpd.db.push_back(song_a);
    mpd.db.push_back(song_b);

    ShuffleChain chain;
    chain.Add(song_a.URI());

    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    mpd.queue.push_back(song_b);
    mpd.state.playing = false;
    // signal "past the end of the queue" using an empty song_position.
    mpd.state.song_position = std::nullopt;

    // Make future Idle calls return IDLE_QUEUE
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    shuffle_loop(&mpd, &chain, Options(), loop_once_d);

    // We should add a new item to the queue, and start playing.
    cmp_ok(mpd.queue.size(), "==", 2,
           "shuffle_loop_basic: added one song to queue");
    ok(mpd.state.playing, "shuffle_loop_basic: playing after loop");
    ok(mpd.state.song_position == 1,
       "shuffle_loop_basic: queue position on second song");

    // The currently playing item should be song_a (the only song in the
    // shuffle chain). If the mpd state is invalid, no playing song is returned,
    // and we skip this check.
    if (std::optional<fake::Song> p = mpd.Playing(); p) {
        ok(p == song_a, "shuffle_loop_basic: queued and played song_a");
    }
}

void test_shuffle_loop_empty() {
    fake::MPD mpd;

    fake::Song song_a("song_a");
    mpd.db.push_back(song_a);

    ShuffleChain chain;
    chain.Add(song_a.URI());

    // Make future IDLE calls return IDLE_QUEUE
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    shuffle_loop(&mpd, &chain, Options(), loop_once_d);

    // We should add a new item to the queue, and start playing.
    cmp_ok(mpd.queue.size(), "==", 1,
           "shuffle_loop_empty: added one song to queue");
    ok(mpd.state.playing, "shuffle_loop_empty: playing after loop");
    ok(mpd.state.song_position == 0,
       "shuffle_loop_empty: queue position on first song");

    // The currently playing item should be song_a (the only song in the
    // shuffle chain). If the mpd state is invalid, no playing song is returned,
    // and we skip this check.
    if (std::optional<fake::Song> p = mpd.Playing(); p) {
        ok(p == song_a, "shuffle_loop_empty: queued and played song_a");
    }
}

void test_shuffle_loop_empty_buffer() {
    fake::MPD mpd;

    fake::Song song_a("song_a");
    mpd.db.push_back(song_a);

    ShuffleChain chain;
    chain.Add(song_a.URI());

    Options options;
    options.queue_buffer = 3;

    // Make future IDLE calls return IDLE_QUEUE
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    shuffle_loop(&mpd, &chain, options, loop_once_d);

    // We should add 4 new items to the queue, and start playing on the first
    // one.
    // 4 = queue_buffer + the currently playing song.
    cmp_ok(mpd.queue.size(), "==", 4,
           "shuffle_loop_empty_buffer: added one song to queue");
    ok(mpd.state.playing, "shuffle_loop_empty_buffer: playing after loop");
    ok(mpd.state.song_position == 0,
       "shuffle_loop_empty_buffer: queue position on first song");

    if (std::optional<fake::Song> p = mpd.Playing(); p) {
        ok(p == song_a, "shuffle_loop_empty_buffer: queued and played song_a");
    }
}

void test_shuffle_loop_buffer_partial() {
    fake::MPD mpd;

    fake::Song song_a("song_a"), song_b("song_b");
    mpd.db.push_back(song_a);

    ShuffleChain chain;
    chain.Add(song_a.uri);

    Options options;
    options.queue_buffer = 3;

    // Pretend like the queue already has a few songs in it, and we're in
    // the middle of playing it. We normally don't need to do anything,
    // but we may need to update the queue buffer.
    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    mpd.PlayAt(1);

    // Make future IDLE calls return IDLE_QUEUE
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    shuffle_loop(&mpd, &chain, options, loop_once_d);

    // We had 3 songs in the queue, and we were playing the second song, so
    // we only need to add 2 more songs to fill out the queue buffer.
    cmp_ok(mpd.queue.size(), "==", 5,
           "shuffle_loop_partial_buffer: added one song to queue");
    // We should still be playing the same song as before.
    ok(mpd.state.playing, "shuffle_loop_partial_buffer: playing after loop");
    ok(mpd.state.song_position == 1,
       "shuffle_loop_partial_buffer: queue position on the same song");

    if (std::optional<fake::Song> p = mpd.Playing(); p) {
        ok(p == song_b,
           "shuffle_loop_partial_buffer: playing the same song as before");
    }
}

static std::string failing_getpass_f() {
    fail("called failing getpass!");
    abort();
}

void test_connect_no_password() {
    // Make sure the environment doesn't influence the test.
    xclearenv();

    fake::MPD mpd;
    fake::Dialer dialer(mpd);
    // by default we should try and connect to localhost on the default port.
    dialer.check = mpd::Address{"localhost", 6600};

    std::function<std::string()> pass_f = failing_getpass_f;
    std::unique_ptr<mpd::MPD> result = Connect(dialer, Options(), pass_f);

    ok(*dynamic_cast<fake::MPD *>(result.get()) == mpd,
       "connect_no_password: same mpd instance");
}

struct ConnectTestCase {
    // Want is used to set the actual server host/port.
    mpd::Address want;
    // Password is the password that will be set for the fake MPD server. If
    // this value is set, the dialed MPD fake will have zero permissions
    // initially.
    std::optional<std::string> password = std::nullopt;
    // Env are the values that will be stored in the MPD_* environment
    // variables. If they are empty or 0, they will remain unset.
    mpd::Address env = {};
    // Flag are the values that will be given in flags. If they are empty
    // or 0, the respective flag will not be set.
    mpd::Address flag = {};
};

void test_connect_parse_host() {
    std::vector<ConnectTestCase> cases = {
        // by default, connect to localhost:6600
        {
            .want = {"localhost", 6600},
        },
        // If only MPD_HOST is set with a password, and no MPD_PORT
        {
            .want = {"localhost", 6600},
            .password = "foo",
            .env = {.host = "foo@localhost"},
        },
        // MPD_HOST with a domain-like string, and MPD_PORT is set.
        {
            .want = {"something.random.com", 123},
            .env = {"something.random.com", 123},
        },
        // MPD_HOST is a unix socket, MPD_PORT unset.
        {
            // port is Needed for test, unused by libmpdclient
            .want = {"/test/mpd.socket", 6600},
            .env = {"/test/mpd.socket", 0},
        },
        // MPD_HOST is a unix socket, with a password.
        {
            // port is Needed for test, unused by libmpdclient
            .want = {"/another/mpd.socket", 6600},
            .password = "with_pass",
            .env = {.host = "with_pass@/another/mpd.socket"},
        },
        // --host example.com, port unset. environ unset.
        {
            .want = {"example.com", 6600},
            .flag = {.host = "example.com"},
        },
        // --host some.host.com --port 5512, environ unset
        {
            .want = {"some.host.com", 5512},
            .flag = {"some.host.com", 5512},
        },
        // flag host, with password. environ unset.
        {
            .want = {"yet.another.host", 7781},
            .password = "secret_password",
            .flag = {"secret_password@yet.another.host", 7781},
        },
        // Flags should override MPD_HOST and MPD_PORT environment variables.
        {
            .want = {"real.host", 1234},
            .env = {"default.host", 6600},
            .flag = {"real.host", 1234},
        },
    };

    for (unsigned i = 0; i < cases.size(); i++) {
        const auto &test = cases[i];

        xclearenv();

        if (!test.env.host.empty()) {
            xsetenv("MPD_HOST", test.env.host.data());
        }

        if (test.env.port) {
            // xsetenv copies the value string, so it's safe to use a
            // temporary here.
            xsetenv("MPD_PORT", std::to_string(test.env.port).data());
        }

        std::vector<std::string> flags;
        if (!test.flag.host.empty()) {
            flags.push_back("--host");
            flags.push_back(test.flag.host);
        }
        if (test.flag.port) {
            flags.push_back("--port");
            flags.push_back(std::to_string(test.flag.port));
        }

        Options opts;

        if (flags.size() > 0) {
            auto parse = Options::Parse(fake::TagParser(), flags);
            if (auto err = std::get_if<ParseError>(&parse); err != nullptr) {
                fail("connect_parse_host[%u]: failed to parse flags", i);
                diag("  parse result: %s", err->msg.data());
            }
            opts = std::get<Options>(parse);
        }

        fake::MPD mpd;
        if (test.password) {
            // Create two users, one with no allowed commands, and one with
            // the good set of allowed commands.
            mpd.users = {
                {"zero-privileges", {}},
                {*test.password, {"add", "status", "play", "pause", "idle"}},
            };
            // Then mark the default user, as the user with no privileges.
            // the default user in the fake allows all commands, so we need
            // to change it.
            mpd.active_user = "zero-privileges";
        }

        fake::Dialer dialer(mpd);
        dialer.check = test.want;

        std::function<std::string()> pass_f = failing_getpass_f;
        std::unique_ptr<mpd::MPD> result = Connect(dialer, opts, pass_f);

        ok(mpd == *dynamic_cast<fake::MPD *>(result.get()),
           "connect_parse_host[%u]: matches mpd connection", i);
    }
}

// FakePasswordProvider is a password function that always returns the
// given password, and counts the number of times that the password function
// is called.
class FakePasswordProvider {
   public:
    FakePasswordProvider(std::string p) : password(p){};

    std::string password = {};
    int call_count = 0;

    std::string operator()() {
        call_count++;
        return password;
    };
};

void test_connect_env_bad_password() {
    xclearenv();

    fake::MPD mpd;
    mpd.users = {
        {"zero-privileges", {}},
        {"good_password", {"add", "status", "play", "pause", "idle"}},
    };
    mpd.active_user = "zero-privileges";

    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"localhost", 6600};

    // Set a bad password via the environment.
    xsetenv("MPD_HOST", "bad_password@localhost");

    std::function<std::string()> pass_f = FakePasswordProvider("good_password");

    // using good_password_f, just in-case Connect decides to prompt
    // for a password. It should fail without ever calling good_password_f.
    dies_ok({ (void)Connect(dialer, Options(), pass_f); },
            "connect_env_bad_password: fail to connect with bad password");
}

void test_connect_env_ok_password_bad_perms() {
    xclearenv();

    fake::MPD mpd;
    mpd.users = {
        {"zero-privileges", {}},
        // The "test_password" has an extended set of privileges, but should
        // still be missing some required commands.
        {"test_password", {"add"}},
    };
    mpd.active_user = "zero-privileges";

    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"localhost", 6600};

    // set our password in the environment
    xsetenv("MPD_HOST", "good_password@localhost");

    std::function<std::string()> pass_f = FakePasswordProvider("good_password");

    // We should terminate after seeing the bad permissions. If we end up
    // re-prompting (and getting a good password), we should succeed, and fail
    // the test.
    dies_ok({ (void)Connect(dialer, Options(), pass_f); },
            "connect_env_ok_password_bad_perms: fail to connect with bad "
            "permissions");
}

// If no password is supplied in the environment, but we have a restricted
// command, then we should prompt for a user password. Once that password
// matches, *and* we don't have any more disallowed required commands, then
// we should be OK.
void test_connect_bad_perms_ok_prompt() {
    xclearenv();

    fake::MPD mpd;
    mpd.users = {
        {"zero-privileges", {}},
        {"good_password", {"add", "status", "play", "pause", "idle"}},
    };
    mpd.active_user = "zero-privileges";

    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"localhost", 6600};

    std::function<std::string()> pass_f = FakePasswordProvider("good_password");
    FakePasswordProvider *pp = pass_f.target<FakePasswordProvider>();

    cmp_ok(
        pp->call_count, "==", 0,
        "connect_bad_perms_ok_prompt: no call to password func to start with");

    std::unique_ptr<mpd::MPD> result = Connect(dialer, Options(), pass_f);

    ok(*dynamic_cast<fake::MPD *>(result.get()) == mpd,
       "connect_bad_perms_ok_prompt: mpd matches fake MPD");

    cmp_ok(
        pp->call_count, "==", 1,
        "connect_bad_perms_ok_prompt: should have one call to password func");
}

void test_connect_bad_perms_prompt_bad_perms() {
    xclearenv();

    fake::MPD mpd;
    mpd.users = {
        {"zero-privileges", {}},
        // Missing privileges for both passwords. "env_password" is given in
        // the env, but it's missing privleges. Then we prompt, to get
        // prompt_password, and that *also* fails, so the connect fails overall.
        {"env_password", {"play"}},
        {"prompt_password", {"add"}},
    };
    mpd.active_user = "zero-privileges";

    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"localhost", 6600};

    xsetenv("MPD_HOST", "env_password@localhost");

    std::function<std::string()> pass_f =
        FakePasswordProvider("prompt_password");

    dies_ok({ (void)Connect(dialer, Options(), pass_f); },
            "connect_bad_perms_prompt_bad_perms: fails to connect");
}

int main() {
    plan(NO_PLAN);

    test_shuffle_loop_init_empty();
    test_shuffle_loop_init_playing();
    test_shuffle_loop_init_stopped();

    test_shuffle_loop_basic();
    test_shuffle_loop_empty();
    test_shuffle_loop_empty_buffer();
    test_shuffle_loop_buffer_partial();

    test_connect_no_password();
    test_connect_parse_host();
    test_connect_env_bad_password();
    test_connect_env_ok_password_bad_perms();
    test_connect_bad_perms_ok_prompt();
    test_connect_bad_perms_prompt_bad_perms();

    done_testing();
}
