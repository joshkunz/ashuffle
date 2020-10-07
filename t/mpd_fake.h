#ifndef __ASHUFFLE_T_MPD_FAKE_H__
#define __ASHUFFLE_T_MPD_FAKE_H__

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>

#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <mpd/tag.h>

#include "mpd.h"

namespace ashuffle {
namespace fake {

namespace {

// For debugging: Set to true to make the fake echo all MPD calls made
// by the tests.
constexpr bool kMPDEcho = false;
std::ostream& dbg() {
    static std::ofstream devnull("/dev/null");
    return kMPDEcho ? std::cerr : devnull;
}
}  // namespace

class Song : public mpd::Song {
   public:
    using tag_map = std::unordered_map<enum mpd_tag_type, std::string>;
    std::string uri;
    tag_map tags;

    Song() : Song("", {}){};
    Song(std::string_view u) : Song(u, {}){};
    Song(tag_map t) : Song("", t){};
    Song(std::string_view u, tag_map t) : uri(u), tags(t){};

    std::optional<std::string> Tag(enum mpd_tag_type tag) const override {
        if (tags.find(tag) == tags.end()) {
            return std::nullopt;
        }
        return tags.at(tag);
    }

    std::string URI() const override { return uri; }

    bool operator==(const Song& other) const {
        return uri == other.uri && tags == other.tags;
    }

    friend std::ostream& operator<<(std::ostream& os, const Song& s) {
        os << "Song(\"" << s.uri << "\"";

        if (s.tags.empty()) {
            return os << ")";
        }

        os << ", {";
        bool first = true;
        for (auto& [tag, val] : s.tags) {
            if (!first) {
                os << ", ";
            }
            first = false;
            std::string tag_name;
            switch (tag) {
                case MPD_TAG_ARTIST:
                    tag_name = "artist";
                    break;
                case MPD_TAG_ALBUM:
                    tag_name = "album";
                    break;
                default:
                    tag_name = "<unknown>";
            }
            os << tag_name << ": " << val;
        }
        return os << "})";
    }
};

class TagParser : public mpd::TagParser {
   public:
    using tagname_map = std::unordered_map<std::string, enum mpd_tag_type>;
    tagname_map tags;

    TagParser() : tags({}){};
    TagParser(tagname_map t) : tags(t){};

    ~TagParser() override = default;

    std::optional<enum mpd_tag_type> Parse(
        const std::string_view tag) const override {
        std::string tag_copy(tag);
        if (tags.find(tag_copy) == tags.end()) {
            return std::nullopt;
        }
        return tags.at(tag_copy);
    }
};

// State stores the "state" of the MPD player. It is also used by the
// status fake to implement the status interface.
struct State {
    bool single_mode = false;
    bool playing = false;
    std::optional<unsigned> song_position = std::nullopt;
    unsigned queue_length = 0;
};

inline bool operator==(const State& lhs, const State& rhs) {
    return (lhs.single_mode == rhs.single_mode && lhs.playing == rhs.playing &&
            lhs.song_position == rhs.song_position &&
            lhs.queue_length == rhs.queue_length);
}

class Status : public mpd::Status {
   public:
    Status(State state) : state_(state){};
    ~Status() override = default;

    unsigned QueueLength() const override { return state_.queue_length; };

    bool Single() const override { return state_.single_mode; };

    std::optional<int> SongPosition() const override {
        return state_.song_position;
    };

    bool IsPlaying() const override { return state_.playing; };

   private:
    const State state_;
};

class SongReader;

class MPD : public mpd::MPD {
   public:
    MPD() = default;
    MPD(const MPD& other) = default;
    ~MPD() override = default;

    // user_map is a map of password -> vector<allowed commands>.
    typedef std::unordered_map<std::string, std::vector<std::string>> user_map;

    std::vector<Song> db;
    std::vector<Song> queue;
    State state;
    mpd::IdleEventSet (*idle_f)() = [] { return mpd::IdleEventSet(); };
    std::string active_user;
    user_map users;

    std::unique_ptr<mpd::SongReader> ListAll() override;

    void Pause() override {
        dbg() << "call:Play" << std::endl;
        state.playing = false;
    };
    void Play() override {
        dbg() << "call:Pause" << std::endl;
        state.playing = true;
    };
    void PlayAt(unsigned position) override {
        dbg() << "call:PlayAt(" << position << ")" << std::endl;
        assert(position < queue.size() && "can't play song outside of queue");
        state.song_position = position;
        state.playing = true;
    };
    std::unique_ptr<mpd::Status> CurrentStatus() override {
        dbg() << "call:Status" << std::endl;
        State snapshot(state);
        snapshot.queue_length = queue.size();
        return std::unique_ptr<mpd::Status>(new Status(snapshot));
    };
    std::optional<std::unique_ptr<mpd::Song>> Search(
        std::string_view uri) override {
        dbg() << "call:Search(" << uri << ")" << std::endl;
        std::optional<Song> found = SearchInternal(uri);
        if (!found) {
            return std::nullopt;
        }
        return std::unique_ptr<mpd::Song>(new Song(*found));
    };
    mpd::IdleEventSet Idle(__attribute__((unused))
                           const mpd::IdleEventSet&) override {
        dbg() << "call:Idle" << std::endl;
        return idle_f();
    };
    void Add(const std::string& uri) override {
        dbg() << "call:Add(" << uri << ")" << std::endl;
        std::optional<Song> found = SearchInternal(uri);
        assert(found && "cannot add URI not in DB");
        queue.push_back(*found);
    };
    mpd::MPD::PasswordStatus ApplyPassword(
        const std::string& password) override {
        dbg() << "call:Password(" << password << ")" << std::endl;
        using status = mpd::MPD::PasswordStatus;
        if (users.find(password) == users.end()) {
            return status::kRejected;
        }
        active_user = password;
        return status::kAccepted;
    };
    mpd::MPD::Authorization CheckCommands(
        const std::vector<std::string_view>& cmds) override {
        dbg() << "call:CheckCommandsAllowed(" << absl::StrJoin(cmds, ", ")
              << ")" << std::endl;
        std::vector<std::string> allowed;
        if (auto user = users.find(active_user);
            !active_user.empty() && user != users.end()) {
            allowed = user->second;
        }
        // If there is no active user, by default, allow these commands. This
        // makes it so we don't have to constantly add these in the mocks.
        if (active_user.empty()) {
            allowed = {"add", "status", "play", "pause", "idle"};
        }
        std::vector<std::string> missing;
        for (auto& cmd : cmds) {
            if (std::find(allowed.begin(), allowed.end(), cmd) ==
                allowed.end()) {
                missing.emplace_back(cmd);
            }
        }
        mpd::MPD::Authorization auth;
        auth.authorized = missing.empty();
        auth.missing = std::move(missing);
        return auth;
    };

    // Playing is a special test-only API to get the currently playing song
    // from the queue. If there is no currently playing song, and empty
    // option is returned.
    std::optional<fake::Song> Playing() {
        if (!state.playing || !state.song_position) {
            return std::nullopt;
        }
        return queue[*state.song_position];
    };

   private:
    std::optional<Song> SearchInternal(std::string_view uri) {
        for (Song& song : db) {
            if (song.URI() == uri) {
                return song;
            }
        }
        return std::nullopt;
    };
};

inline bool operator==(const MPD& lhs, const MPD& rhs) {
    return (lhs.db == rhs.db && lhs.queue == rhs.queue &&
            lhs.state == rhs.state && lhs.idle_f == rhs.idle_f &&
            lhs.users == rhs.users);
}

std::ostream& operator<<(std::ostream& st, const MPD& mpd) {
    std::vector<std::string> db;
    for (auto& song : mpd.db) {
        db.push_back(song.uri);
    }
    std::vector<std::string> queue;
    for (auto& song : mpd.queue) {
        queue.push_back(song.uri);
    }
    std::vector<std::string> users;
    for (auto& user : mpd.users) {
        users.push_back(
            absl::StrCat(user.first, "=", absl::StrJoin(user.second, ",")));
    }
    st << "MPD<\n"
       << "  DB: " << absl::StrJoin(db, ",") << std::endl
       << "  Queue: " << absl::StrJoin(queue, ",") << std::endl
       << "  State: "
       << absl::StrFormat(
              "State(%d, %d, %u, %u)", mpd.state.single_mode, mpd.state.playing,
              mpd.state.song_position ? *mpd.state.song_position : 0,
              mpd.state.queue_length)
       << std::endl
       << "  Idlef: " << absl::StrFormat("%p", mpd.idle_f) << std::endl
       << "  Active: " << mpd.active_user << std::endl
       << "  Users: "
       << "\n    " << absl::StrJoin(users, "\n    ") << std::endl
       << ">";
    return st;
}

class SongReader : public mpd::SongReader {
   public:
    ~SongReader() override = default;

    SongReader(const MPD& mpd) : cur_(mpd.db.begin()), end_(mpd.db.end()){};

    std::optional<std::unique_ptr<mpd::Song>> Next() override {
        if (Done()) {
            return std::nullopt;
        }
        return std::unique_ptr<mpd::Song>(new Song(*cur_++));
    };

    // Done returns true when there are no more songs to get. After Done
    // returns true, future calls to `Next` will return an empty option.
    bool Done() override { return cur_ == end_; }

   private:
    std::vector<Song>::const_iterator cur_;
    std::vector<Song>::const_iterator end_;
};

std::unique_ptr<mpd::SongReader> MPD::ListAll() {
    dbg() << "call:ListAll" << std::endl;
    return std::unique_ptr<mpd::SongReader>(new SongReader(*this));
}

class Dialer : public mpd::Dialer {
   public:
    ~Dialer() override = default;

    Dialer(MPD& m) : mpd_(m){};

    // Check is the address to check the dialed address against.
    mpd::Address check;

    mpd::Dialer::result Dial(const mpd::Address& addr,
                             __attribute__((unused)) unsigned timeout_ms =
                                 mpd::Dialer::kDefaultTimeout) const override {
        std::string got = absl::StrFormat("%s:%d", addr.host, addr.port);
        std::string want = absl::StrFormat("%s:%d", check.host, check.port);
        if (got != want) {
            return absl::StrFormat("host '%s' does not match check host '%s'",
                                   got, want);
        }
        return std::unique_ptr<mpd::MPD>(new MPD(mpd_));
    }

   private:
    // mpd_ is the MPD instance to return if the user dials the check address.
    MPD& mpd_;
};

}  // namespace fake
}  // namespace ashuffle

#endif  // __ASHUFFLE_T_MPD_FAKE_H__
