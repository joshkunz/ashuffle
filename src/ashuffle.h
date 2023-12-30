#ifndef __ASHUFFLE_ASHUFFLE_H__
#define __ASHUFFLE_ASHUFFLE_H__

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <absl/status/statusor.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <mpd/client.h>

#include "args.h"
#include "load.h"
#include "mpd.h"
#include "rule.h"
#include "shuffle.h"

namespace ashuffle {

namespace {

// A getpass_f value that can be used in non-interactive mode.
[[maybe_unused]] std::function<std::string()>* kNonInteractiveGetpass = nullptr;

}  // namespace

// `MPD_PORT` environment variables. If a password is needed, no password can
// be found in MPD_HOST, then `getpass_f' will be used to prompt the user
// for a password. If `getpass_f' is NULL, then a password will not be
// prompted.
absl::StatusOr<std::unique_ptr<mpd::MPD>> Connect(
    const mpd::Dialer& d, const Options& options,
    std::function<std::string()>* getpass_f);

struct TestDelegate {
    bool (*until_f)() = nullptr;
    std::function<void(absl::Duration)> sleep_f = absl::SleepFor;
};

// Use the MPD `idle` command to queue songs random songs when the current
// queue finishes playing. This is the core loop of `ashuffle`. The tests
// delegate is used during tests to observe loop effects. It should be set to
// NULL during normal operations.
absl::Status Loop(mpd::MPD* mpd, ShuffleChain* songs, const Options& options,
                  TestDelegate d = TestDelegate());

// Return a loader capable of re-loading the current shuffle chain given
// a particular set of options. If it's not possible to create such a
// loader, returns an empty option.
std::optional<std::unique_ptr<Loader>> Reloader(mpd::MPD* mpd,
                                                const Options& options);
// Print the size of the database to the given stream, accounting for
// grouping.
void PrintChainLength(std::ostream& stream, const ShuffleChain& chain);

}  // namespace ashuffle

#endif
