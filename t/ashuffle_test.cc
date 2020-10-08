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

#include <absl/strings/str_join.h>
#include <absl/time/time.h>
#include <mpd/error.h>
#include <mpd/idle.h>
#include <mpd/pair.h>
#include <mpd/status.h>
#include <mpd/tag.h>

#include "args.h"
#include "ashuffle.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

#include "t/mpd_fake.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ashuffle;

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::ExitedWithCode;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::ValuesIn;
using ::testing::WhenDynamicCastTo;

void xsetenv(std::string k, std::string v) {
    if (setenv(k.data(), v.data(), 1) != 0) {
        perror("xsetenv");
        abort();
    }
}

void xclearenv() {
    // These are the only two flags we care about, we don't want to worry about
    // messing up the rest of the environment.
    unsetenv("MPD_HOST");
    unsetenv("MPD_PORT");
}

// This test delegate only allows the "init" part of the loop to run. No
// continous logic runs.
TestDelegate init_only_d = {
    .until_f = [] { return false; },
};

// This delegate *only* runs the core loop logic, and it only runs the
// logic once.
TestDelegate loop_once_d = {
    .until_f =
        [] {
            static unsigned count;
            // Returns true, false, true, false. This is so we can re-use the
            // same delegate in multiple tests.
            return (count++ % 2) == 0;
        },
};

class LoopTest : public testing::Test {
   public:
    fake::MPD mpd;
    ShuffleChain chain;
    Options opts;

    fake::Song song_a, song_b;

    // Set up MPD instance with two songs in the database, and a shuffle
    // chain with only "song_a".
    void SetUp() override {
        song_a = fake::Song("song_a");
        song_b = fake::Song("song_b");

        mpd.db.push_back(song_a);
        mpd.db.push_back(song_b);

        // Make future Idle calls return IDLE_QUEUE.
        mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

        // Add song_a to our chain.
        chain.Add(song_a.URI());
    }
};

TEST_F(LoopTest, InitEmptyQueue) {
    Loop(&mpd, &chain, opts, init_only_d);

    // We should have enqueued one song into the empty queue (song_a, the only
    // song in the chain), and started playing it.
    EXPECT_THAT(mpd.queue, ElementsAre(song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 0);
}

TEST_F(LoopTest, InitWhilePlaying) {
    // Pretend like we already have a song in our queue, and we're playing.
    mpd.queue.push_back(song_a);
    mpd.PlayAt(0);

    Loop(&mpd, &chain, opts, init_only_d);

    // We shouldn't add anything to the queue if we're already playing,
    // ashuffle should start silently.
    EXPECT_THAT(mpd.queue, ElementsAre(song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 0);
}

TEST_F(LoopTest, InitWhileStopped) {
    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    mpd.queue.push_back(song_b);
    mpd.state.song_position = 0;
    mpd.state.playing = false;

    Loop(&mpd, &chain, opts, init_only_d);

    // ashuffle should have picked a song, added it to the queue, then started
    // playing it. The previous song in the queue should still be there.
    // Note: song_a is the only song in the chain, so we know we'll pick it.
    EXPECT_THAT(mpd.queue, ElementsAre(song_b, song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_THAT(mpd.state.song_position, 1);
}

TEST_F(LoopTest, Requeue) {
    opts.tweak.play_on_startup = false;

    // Pretend like we already have a song in our queue, that was playing,
    // but now we've stopped.
    mpd.queue.push_back(song_b);
    mpd.state.playing = false;
    // signal "past the end of the queue" using an empty song_position.
    mpd.state.song_position = std::nullopt;

    Loop(&mpd, &chain, opts, loop_once_d);

    // We should add a new item to the queue, and start playing.
    EXPECT_THAT(mpd.queue, ElementsAre(song_b, song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 1);

    EXPECT_THAT(mpd.Playing(), Optional(song_a));
}

TEST_F(LoopTest, RequeueEmpty) {
    opts.tweak.play_on_startup = false;

    // Leaving the MPD queue empty.

    Loop(&mpd, &chain, opts, loop_once_d);

    // We should add a new item to the queue, and start playing.
    EXPECT_THAT(mpd.queue, ElementsAre(song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 0);
    EXPECT_THAT(mpd.Playing(), Optional(song_a));
}

TEST_F(LoopTest, RequeueEmptyWithQueueBuffer) {
    opts.tweak.play_on_startup = false;
    opts.queue_buffer = 3;

    Loop(&mpd, &chain, opts, loop_once_d);

    // We should add *4* new items to the queue, and start playing on the first
    // one.
    // 4 = queue_buffer + the currently playing song.

    // All elements are song_a, because that's the only song in the chain.
    EXPECT_THAT(mpd.queue, ElementsAre(song_a, song_a, song_a, song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 0);
    EXPECT_THAT(mpd.Playing(), Optional(song_a));
}

TEST_F(LoopTest, RequeueWithQueueBufferPartiallyFilled) {
    opts.tweak.play_on_startup = false;
    opts.queue_buffer = 3;

    // Make future IDLE calls return IDLE_QUEUE for this test.
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    // Pretend like the queue already has a few songs in it, and we're in
    // the middle of playing it. We normally don't need to do anything,
    // but we may need to update the queue buffer. We could get into this
    // situation if the user manually enqueued several songs, and then
    // jumped to one near the end.
    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    // Zero indexed, this is the second song.
    mpd.PlayAt(1);

    Loop(&mpd, &chain, opts, loop_once_d);

    // We had 3 songs in the queue, and we were playing the second song, so
    // we only need to add 2 more songs to fill out the queue buffer.
    EXPECT_THAT(mpd.queue, ElementsAre(song_b, song_b, song_b, song_a, song_a));
    // We should still be playing the same song as before.
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 1);
    EXPECT_THAT(mpd.Playing(), Optional(song_b));
}

// Test that when we have a partially filled queue buffer, and we have groups
// of songs, we re-queue just enough songs to fill the buffer.
TEST_F(LoopTest, RequeueWithQueueBufferPartiallyFilledAndGrouping) {
    opts.tweak.play_on_startup = false;
    opts.queue_buffer = 4;

    // Make future IDLE calls return IDLE_QUEUE for this test.
    mpd.idle_f = [] { return mpd::IdleEventSet(MPD_IDLE_QUEUE); };

    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    mpd.queue.push_back(song_b);
    mpd.PlayAt(1);  // Second song.

    chain.Clear();
    chain.Add(std::vector<std::string>{song_a.URI(), song_b.URI()});

    Loop(&mpd, &chain, opts, loop_once_d);

    // We start the chain with only one group <song_a, song_b>. The queue buffer
    // is 4, and there's only one song after the current song, so we need to
    // add 3 more songs. The one group in the chain has 2 songs, so we should
    // call Pick() twice, and end up with one extra song.
    EXPECT_THAT(mpd.queue, ElementsAre(song_b, song_b, song_b, song_a, song_b,
                                       song_a, song_b));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, 1);
    EXPECT_THAT(mpd.Playing(), Optional(song_b));
}

TEST_F(LoopTest, Suspend) {
    // Disable play on startup, so we can test loop behavior.
    opts.tweak.play_on_startup = false;
    // Doesn't matter what this is, just has to be > 0 to trigger the suspend
    // functionality.
    opts.tweak.suspend_timeout = absl::Milliseconds(1);

    // We start with the queue empy. Normally, that would trigger us to enqueue
    // more songs. Instead, suspend_timeout is non-zero, so we sleep
    // (call sleep_f in the delegate), and then re-check the queue length. If
    // the length has changed (say the user enqueued some new songs), then we
    // want to "suspend". No songs other than the one we enqueue in sleep_f
    // should be enqueued.

    TestDelegate delegate = {
        // Run the loop twice. That will let us verify that we're truly
        // "deactivated".
        .until_f =
            [] {
                static unsigned calls;
                return calls++ < 2;
            },
        .sleep_f = [this](absl::Duration) { mpd.queue.push_back(song_b); },
    };

    Loop(&mpd, &chain, opts, delegate);

    EXPECT_THAT(mpd.queue, ElementsAre(song_b));
    EXPECT_FALSE(mpd.state.playing);
    EXPECT_EQ(mpd.state.song_position, std::nullopt);

    // Now to verify un-suspend, we clear the queue, and re-loop. That should
    // unfreeze ashuffle, and it should enqueue another song.
    mpd.queue.clear();

    Loop(&mpd, &chain, opts, loop_once_d);

    EXPECT_THAT(mpd.queue, ElementsAre(song_a));
    EXPECT_TRUE(mpd.state.playing);
    EXPECT_THAT(mpd.Playing(), Optional(song_a));
}

struct ConnectTestCase {
    // Want is used to set the actual server host/port.
    mpd::Address want;
    // Password is the password that will be set for the fake MPD server. If
    // this value is set, the dialed MPD fake will have zero permissions
    // initially.
    std::optional<std::string> password = std::nullopt;
    // Input are the values to store in flags/environment variables
    // for the given test.
    mpd::Address input = {};
};

class ConnectParamTest : public testing::TestWithParam<ConnectTestCase> {
   public:
    fake::MPD mpd;

    std::string Host() { return GetParam().input.host; }

    unsigned Port() { return GetParam().input.port; }

    mpd::Address Want() { return GetParam().want; }

    void SetUp() override {
        xclearenv();

        if (auto &password = GetParam().password; password) {
            // Create two users, one with no allowed commands, and one with
            // the good set of allowed commands.
            mpd.users = {
                {"zero-privileges", {}},
                {*password, {"add", "status", "play", "pause", "idle"}},
            };
            // Then mark the default user, as the user with no privileges.
            // the default user in the fake allows all commands, so we need
            // to change it.
            mpd.active_user = "zero-privileges";
        }
    }

    void TearDown() override {
        // in-case we set any environment variables in our test, clear them.
        xclearenv();
    }
};

// FakePasswordProvider is a password function that always returns the
// given password, and counts the number of times that the password function
// is called.
class FakePasswordProvider {
   public:
    FakePasswordProvider() : password(""){};
    FakePasswordProvider(std::string p) : password(p){};

    std::string password = {};
    int call_count = 0;

    std::string operator()() {
        call_count++;
        return password;
    };
};

TEST_P(ConnectParamTest, ViaEnv) {
    if (!Host().empty()) {
        xsetenv("MPD_HOST", Host());
    }

    if (Port()) {
        xsetenv("MPD_PORT", std::to_string(Port()));
    }

    fake::Dialer dialer(mpd);
    dialer.check = Want();

    std::function<std::string()> pass_f = FakePasswordProvider();
    FakePasswordProvider *pp = pass_f.target<FakePasswordProvider>();

    std::unique_ptr<mpd::MPD> result = Connect(dialer, Options(), pass_f);

    EXPECT_THAT(result.get(), WhenDynamicCastTo<fake::MPD *>(Pointee(Eq(mpd))));
    EXPECT_EQ(pp->call_count, 0);
}

TEST_P(ConnectParamTest, ViaFlag) {
    std::vector<std::string> flags;
    if (!Host().empty()) {
        flags.push_back("--host");
        flags.push_back(Host());
    }
    if (Port()) {
        flags.push_back("--port");
        flags.push_back(std::to_string(Port()));
    }

    Options opts = std::get<Options>(Options::Parse(fake::TagParser(), flags));

    fake::Dialer dialer(mpd);
    dialer.check = Want();

    std::function<std::string()> pass_f = FakePasswordProvider();
    FakePasswordProvider *pp = pass_f.target<FakePasswordProvider>();

    std::unique_ptr<mpd::MPD> result = Connect(dialer, opts, pass_f);

    EXPECT_THAT(result.get(), WhenDynamicCastTo<fake::MPD *>(Pointee(Eq(mpd))));
    EXPECT_EQ(pp->call_count, 0);
}

std::vector<ConnectTestCase> connect_cases = {
    // by default, connect to localhost:6600
    {
        .want = {"localhost", 6600},
    },
    {
        .want = {"localhost", 6600},
        .password = "foo",
        .input = {.host = "foo@localhost"},
    },
    {
        .want = {"something.random.com", 123},
        .input = {"something.random.com", 123},
    },
    {
        // port is Needed for test, unused by libmpdclient
        .want = {"/test/mpd.socket", 6600},
        .input = {"/test/mpd.socket", 0},
    },
    // MPD_HOST is a unix socket, with a password.
    {
        // port is Needed for test, unused by libmpdclient
        .want = {"/another/mpd.socket", 6600},
        .password = "with_pass",
        .input = {.host = "with_pass@/another/mpd.socket"},
    },
    {
        .want = {"example.com", 6600},
        .input = {.host = "example.com"},
    },
    {
        .want = {"some.host.com", 5512},
        .input = {"some.host.com", 5512},
    },
    {
        .want = {"yet.another.host", 7781},
        .password = "secret_password",
        .input = {"secret_password@yet.another.host", 7781},
    },
};

INSTANTIATE_TEST_SUITE_P(Connect, ConnectParamTest, ValuesIn(connect_cases));

TEST(ConnectTest, NoPassword) {
    // Make sure the environment doesn't influence the test.
    xclearenv();

    fake::MPD mpd;
    fake::Dialer dialer(mpd);
    // by default we should try and connect to localhost on the default port.
    dialer.check = mpd::Address{"localhost", 6600};

    std::function<std::string()> pass_f = FakePasswordProvider();
    FakePasswordProvider *pp = pass_f.target<FakePasswordProvider>();

    std::unique_ptr<mpd::MPD> result = Connect(dialer, Options(), pass_f);

    EXPECT_EQ(pp->call_count, 0) << "getpass func should not have been called.";
    EXPECT_THAT(result.get(), WhenDynamicCastTo<fake::MPD *>(Pointee(Eq(mpd))));
}

TEST(ConnectTest, FlagOverridesEnv) {
    xclearenv();

    xsetenv("MPD_HOST", "default.host");
    xsetenv("MPD_PORT", std::to_string(6600));

    // Flags should override MPD_HOST and MPD_PORT environment variables.
    Options opts =
        std::get<Options>(Options::Parse(fake::TagParser(), {
                                                                "--host",
                                                                "real.host",
                                                                "--port",
                                                                "1234",
                                                            }));

    fake::MPD mpd;
    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"real.host", 1234};

    std::function<std::string()> pass_f = FakePasswordProvider();
    FakePasswordProvider *pp = pass_f.target<FakePasswordProvider>();

    std::unique_ptr<mpd::MPD> result = Connect(dialer, opts, pass_f);

    EXPECT_EQ(pp->call_count, 0) << "getpass func should not be called";
    EXPECT_THAT(result.get(), WhenDynamicCastTo<fake::MPD *>(Pointee(Eq(mpd))));
}

TEST(ConnectDeathTest, BadEnvPassword) {
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

    std::function<std::string()> pass_f = FakePasswordProvider();

    EXPECT_EXIT((void)Connect(dialer, Options(), pass_f), ExitedWithCode(1),
                HasSubstr("required command still not allowed"));
}

TEST(ConnectDeathTest, EnvPasswordValidWithNoPermissions) {
    xclearenv();

    fake::MPD mpd;
    mpd.users = {
        {"zero-privileges", {}},
        // The "test_password" has an extended set of privileges, but should
        // still be missing some required commands.
        {"test_password", {"add"}},
        // Has all permissions. The test should fail if this is used.
        {"good_password", {"add", "status", "play", "pause", "idle"}},
    };
    mpd.active_user = "zero-privileges";

    fake::Dialer dialer(mpd);
    dialer.check = mpd::Address{"localhost", 6600};

    xsetenv("MPD_HOST", "test_password@localhost");

    std::function<std::string()> pass_f = FakePasswordProvider("good_password");

    // We should terminate after seeing the bad permissions. If we end up
    // re-prompting (and getting a good password), we should succeed, and fail
    // the test.
    EXPECT_EXIT((void)Connect(dialer, Options(), pass_f), ExitedWithCode(1),
                HasSubstr("required command still not allowed"));
}

// If no password is supplied in the environment, but we have a restricted
// command, then we should prompt for a user password. Once that password
// matches, *and* we don't have any more disallowed required commands, then
// we should be OK.
TEST(ConnectTest, BadPermsOKPrompt) {
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

    EXPECT_EQ(pp->call_count, 0);

    std::unique_ptr<mpd::MPD> result = Connect(dialer, Options(), pass_f);

    EXPECT_THAT(result.get(), WhenDynamicCastTo<fake::MPD *>(Pointee(Eq(mpd))));

    EXPECT_EQ(pp->call_count, 1) << "getpass should have been called";
}

TEST(ConnectDeathTest, BadPermsBadPrompt) {
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

    EXPECT_EXIT((void)Connect(dialer, Options(), pass_f), ExitedWithCode(1),
                HasSubstr("required command still not allowed"));
}
