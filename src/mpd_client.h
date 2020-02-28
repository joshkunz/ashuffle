#ifndef __ASHUFFLE_MPD_CLIENT_H__
#define __ASHUFFLE_MPD_CLIENT_H__

#include <memory>
#include <optional>

#include <mpd/song.h>

#include "mpd.h"

namespace ashuffle {
namespace mpd {
namespace client {

constexpr unsigned kDefaultTimeout = 25000;  // 25 seconds.

// LiftLegacySong is a temporary work-around that allows constructing
// mpd::Song types from regular libmpdclient song structures.
std::unique_ptr<Song> LiftLegacySong(struct mpd_song*);

// Returns the default TagParser, backed by libmpdclient.
std::unique_ptr<TagParser> Parser();

// Dial MPD at the given address, with the given timeout. On success a
// pointer to an MPD instance is returned, on error an empty optional is
// returned.
std::optional<std::unique_ptr<MPD>> Dial(const Address& addr,
                                         unsigned timeout_ms = kDefaultTimeout);

}  // namespace client
}  // namespace mpd
}  // namespace ashuffle

#endif  // __ASHUFFLE_MPD_CLIENT_H__
