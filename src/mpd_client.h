#ifndef __ASHUFFLE_MPD_CLIENT_H__
#define __ASHUFFLE_MPD_CLIENT_H__

#include <memory>
#include <optional>

#include <mpd/song.h>

#include "mpd.h"

namespace ashuffle {
namespace mpd {
namespace client {

// Returns the default TagParser, backed by libmpdclient.
std::unique_ptr<TagParser> Parser();

// Returns a new MPD dialer that uses libmpdclient to dial MPD.
std::unique_ptr<Dialer> Dialer();

}  // namespace client
}  // namespace mpd
}  // namespace ashuffle

#endif  // __ASHUFFLE_MPD_CLIENT_H__
