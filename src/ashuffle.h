#ifndef __ASHUFFLE_ASHUFFLE_H__
#define __ASHUFFLE_ASHUFFLE_H__

#include <cstdio>
#include <string>
#include <vector>

#include <mpd/client.h>

#include "args.h"
#include "rule.h"
#include "shuffle.h"

extern const int WINDOW_SIZE;

// `MPD_PORT` environment variables. If a password is needed, no password can
// be found in MPD_HOST, then `getpass_f' will be used to prompt the user
// for a password. If `getpass_f' is NULL, the a default password prompt
// (based on getpass) will be used.
struct mpd_connection* ashuffle_connect(const Options& options,
                                        std::string (*getpass_f)());

// Build a `shuffle_chain` of songs from URIs in the given file.
int build_songs_file(struct mpd_connection* mpd,
                     const std::vector<Rule>& ruleset, FILE* input,
                     ShuffleChain* songs, bool check);

// Build a `shuffle_chain` of songs, by querying the given MPD instance.
int build_songs_mpd(struct mpd_connection* mpd,
                    const std::vector<Rule>& ruleset, ShuffleChain* songs);

// Add a single random song from the given shuffle chain to the given MPD
// instance.
void shuffle_single(struct mpd_connection* mpd, ShuffleChain* songs);

struct shuffle_test_delegate {
    bool skip_init;
    bool (*until_f)();
};

// Use the MPD `idle` command to queue songs random songs when the current
// queue finishes playing. This is the core loop of `ashuffle`. The tests
// delegate is used during tests to observe loop effects. It should be set to
// NULL during normal operations.
int shuffle_loop(struct mpd_connection* mpd, ShuffleChain* songs,
                 const Options& options, struct shuffle_test_delegate*);
#endif
