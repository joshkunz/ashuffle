#ifndef __ASHUFFLE_ASHUFFLE_H__
#define __ASHUFFLE_ASHUFFLE_H__

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <mpd/client.h>

#include "args.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

namespace ashuffle {

extern const int WINDOW_SIZE;

// `MPD_PORT` environment variables. If a password is needed, no password can
// be found in MPD_HOST, then `getpass_f' will be used to prompt the user
// for a password. If `getpass_f' is NULL, the a default password prompt
// (based on getpass) will be used.
std::unique_ptr<mpd::MPD> ashuffle_connect(
    const mpd::Dialer& d, const Options& options,
    std::function<std::string()>& getpass_f);

// Build a `shuffle_chain` of songs from URIs in the given file.
void build_songs_file(mpd::MPD* mpd, const std::vector<Rule>& ruleset,
                      FILE* input, ShuffleChain* songs, bool check);

// Build a `shuffle_chain` of songs, by querying the given MPD instance.
void build_songs_mpd(mpd::MPD* mpd, const std::vector<Rule>& ruleset,
                     ShuffleChain* songs);

struct TestDelegate {
    bool skip_init = false;
    bool (*until_f)() = nullptr;
};

// Use the MPD `idle` command to queue songs random songs when the current
// queue finishes playing. This is the core loop of `ashuffle`. The tests
// delegate is used during tests to observe loop effects. It should be set to
// NULL during normal operations.
void shuffle_loop(mpd::MPD* mpd, ShuffleChain* songs, const Options& options,
                  TestDelegate d = TestDelegate());

}  // namespace ashuffle

#endif
