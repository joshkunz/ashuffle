#ifndef __ASHUFFLE_MPD_H__
#define __ASHUFFLE_MPD_H__

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
    virtual enum mpd_state State() const = 0;
};

// SongReader is a helper for iterating over a list of songs fetched from
// MPD.
class SongReader {
   public:
    virtual ~SongReader(){};

    // Next returns the next song from the iterator, or an empty option if
    // all songs have been consumed.
    virtual std::optional<std::unique_ptr<Song>> Next() = 0;

    // Done returns true when there are no more songs to get. After Done
    // returns true, future calls to `Next` will return an empty option.
    virtual bool Done() = 0;
};

// IdleEventSet contains a set of MPD "Idle" events. These events are used to
// signal to MPD what conditions trigger the end of an "idle" command.
struct IdleEventSet {
    // Events is an integer representation of the bit-set.
    int events;

    // Add adds the given event to the set.
    void Add(enum mpd_idle event) { events |= event; }

    // Has returns true if the given event is in the set.
    bool Has(enum mpd_idle event) const { return !!(events & event); }

    // Enum is a helper, that returns an enum representation of `events`.
    enum mpd_idle Enum() const { return static_cast<enum mpd_idle>(events); }
};

// Address represents the dial address of a given MPD instance.
struct Address {
    // host is the hostname of the MPD instance.
    std::string host = "";
    // Port is the TCP port the MPD instance is listening on.
    unsigned port = 0;
};

// MPD represents a connection to an MPD instance.
class MPD {
   public:
    virtual ~MPD(){};

    // Pauses the player.
    virtual void Pause() = 0;

    // Resumes playing.
    virtual void Play() = 0;

    // Play the song at the given queue position.
    virtual void PlayAt(unsigned position) = 0;

    // Gets the current player/MPD status.
    virtual std::unique_ptr<Status> GetStatus() = 0;

    // Returns a song reader that can be used to list all songs stored in MPD's
    // database.
    virtual std::unique_ptr<SongReader> ListAll() = 0;

    // Searches MPD's DB for a particular song URI, and returns that song.
    // Returns an empty optional if the song could not be found.
    virtual std::optional<std::unique_ptr<Song>> Search(
        const std::string& uri) = 0;

    // Blocks until one of the enum mpd_idle events in the event set happens.
    // A new event set is returned, containing all events that occured during
    // the idle period.
    virtual IdleEventSet Idle(const IdleEventSet&) = 0;

    // Add, adds the song wit the given URI to the MPD queue.
    virtual void Add(const std::string& uri) = 0;

    enum PasswordStatus {
        kAccepted,
        kRejected,
    };
    // ApplyPassword applies the given password to the MPD connection. If
    // the password was received by MPD successfully, a PasswordStatus is
    // returned.
    virtual PasswordStatus ApplyPassword(const std::string& password) = 0;

    // CheckCommandsAllowed checks that the given commands are allowed on
    // the MPD connection.
    virtual bool CheckCommandsAllowed(
        const std::vector<std::string_view>& cmds) = 0;
};

}  // namespace mpd
}  // namespace ashuffle

#endif  // __ASHUFFLE_MPD_H__
