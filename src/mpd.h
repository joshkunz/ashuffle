#ifndef __ASHUFFLE_MPD_H__
#define __ASHUFFLE_MPD_H__

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

#include <mpd/idle.h>
#include <mpd/status.h>
#include <mpd/tag.h>

namespace ashuffle {
namespace mpd {

class TagParser {
   public:
    // Parse parses the given tag, and returns the appropriate tag type.
    // If no matching tag is found, then an empty optional is returned.
    virtual std::optional<enum mpd_tag_type> Parse(
        const std::string_view tag) const = 0;
    virtual ~TagParser(){};
};

class Song {
   public:
    virtual ~Song(){};

    // Get the given tag for this song.
    virtual std::optional<std::string> Tag(enum mpd_tag_type tag) const = 0;

    // Returns the URI of this song.
    virtual std::string URI() const = 0;
};

class Status {
   public:
    virtual ~Status(){};

    // Return the current queue length. Returns 0 if the queue is empty.
    virtual unsigned QueueLength() const = 0;

    // Single returns true if "single mode" is toggled in MPD.
    virtual bool Single() const = 0;

    // SongPosition returns the position of the "current" song in the queue.
    // If there is no current song (e.g., all songs in the queue have been
    // played, or the queue is empty) then an empty option is returned;
    virtual std::optional<int> SongPosition() const = 0;

    // Returns the current play state of the player.
    virtual bool IsPlaying() const = 0;
};

// SongReader is a helper for iterating over a list of songs fetched from
// MPD.
class SongReader {
   public:
    virtual ~SongReader(){};

    // Next returns the next song from the iterator, or a NOT_FOUND error
    // if there are no more songs to iterate.
    virtual absl::StatusOr<std::unique_ptr<Song>> Next() = 0;

    // Done returns true when there are no more songs to get. After Done
    // returns true, future calls to `Next` will return an empty option.
    virtual bool Done() = 0;
};

// IdleEventSet contains a set of MPD "Idle" events. These events are used to
// signal to MPD what conditions trigger the end of an "idle" command.
struct IdleEventSet {
    // Events is an integer representation of the bit-set.
    int events = 0;

    template <typename... Events>
    IdleEventSet(Events... set_events) {
        std::initializer_list<int> es = {set_events...};
        for (int event : es) {
            Add(static_cast<enum mpd_idle>(event));
        }
    }

    // Add adds the given event to the set.
    void Add(enum mpd_idle event) { events |= event; }

    // Has returns true if the given event is in the set.
    bool Has(enum mpd_idle event) const { return !!(events & event); }

    // Enum is a helper, that returns an enum representation of `events`.
    enum mpd_idle Enum() const { return static_cast<enum mpd_idle>(events); }
};

// MPD represents a connection to an MPD instance.
class MPD {
   public:
    virtual ~MPD(){};

    // Pauses the player.
    virtual absl::Status Pause() = 0;

    // Resumes playing.
    virtual absl::Status Play() = 0;

    // Play the song at the given queue position.
    virtual absl::Status PlayAt(unsigned position) = 0;

    // Gets the current player/MPD status.
    virtual absl::StatusOr<std::unique_ptr<Status>> CurrentStatus() = 0;

    // Options for controlling whether or not song metadata is included in
    // a ListAll call.
    enum class MetadataOption {
        // All metadata sourced from MPD is included and queryable on the songs.
        kInclude,
        // No metadata is included on the songs. Only song URI.
        kOmit,
    };

    // Returns a song reader that can be used to list all songs stored in MPD's
    // database.
    virtual absl::StatusOr<std::unique_ptr<SongReader>> ListAll(
        MetadataOption metadata = MetadataOption::kInclude) = 0;

    // Searches MPD's DB for a particular song URI, and returns that song.
    // Returns a NOT_FOUND status if the song could not be found.
    virtual absl::StatusOr<std::unique_ptr<Song>> Search(
        std::string_view uri) = 0;

    // Blocks until one of the enum mpd_idle events in the event set happens.
    // A new event set is returned, containing all events that occured during
    // the idle period.
    virtual absl::StatusOr<IdleEventSet> Idle(const IdleEventSet&) = 0;

    // Add, adds the song wit the given URI to the MPD queue.
    virtual absl::Status Add(const std::string& uri) = 0;

    // Add also works on vectors of URIs, by repeatedly invoking Add for each
    // element.
    absl::Status Add(const std::vector<std::string>& uris) {
        for (auto& u : uris) {
            absl::Status status = Add(u);
            if (!status.ok()) {
                return status;
            }
        }
        return absl::OkStatus();
    };

    enum PasswordStatus {
        kAccepted,
        kRejected,
    };
    // ApplyPassword applies the given password to the MPD connection. If
    // the password was received by MPD successfully, a PasswordStatus is
    // returned.
    virtual absl::StatusOr<PasswordStatus> ApplyPassword(
        const std::string& password) = 0;

    struct Authorization {
        // Set to true if this connection is authorized to execute all
        // requested commands.
        bool authorized = false;
        // If authorized is false, this will be filled with the missing
        // commands.
        std::vector<std::string> missing = {};
    };

    // CheckCommandsAllowed checks that the given commands are allowed on
    // the MPD connection.
    virtual absl::StatusOr<Authorization> CheckCommands(
        const std::vector<std::string_view>& cmds) = 0;
};

// Address represents the dial address of a given MPD instance.
struct Address {
    // host is the hostname of the MPD instance.
    std::string host = "";
    // Port is the TCP port the MPD instance is listening on.
    unsigned port = 0;
};

class Dialer {
   public:
    virtual ~Dialer(){};

    constexpr static absl::Duration kDefaultTimeout = absl::Seconds(25);

    // Dial connects to the MPD instance at the given Address, optionally,
    // with the given timeout. On success a variant with a unique_ptr to
    // an MPD instance is returned. On failure, a string is returned with
    // a human-readable description of the error.
    virtual absl::StatusOr<std::unique_ptr<MPD>> Dial(
        const Address&, absl::Duration timeout = kDefaultTimeout) const = 0;
};

}  // namespace mpd
}  // namespace ashuffle

#endif  // __ASHUFFLE_MPD_H__
