#include "mpd_client.h"

#include <iostream>

#include <absl/strings/str_format.h>
#include <mpd/capabilities.h>
#include <mpd/connection.h>
#include <mpd/database.h>
#include <mpd/error.h>
#include <mpd/idle.h>
#include <mpd/pair.h>
#include <mpd/password.h>
#include <mpd/player.h>
#include <mpd/protocol.h>
#include <mpd/queue.h>
#include <mpd/recv.h>
#include <mpd/search.h>
#include <mpd/song.h>
#include <mpd/status.h>

#include "mpd.h"
#include "util.h"

namespace ashuffle {
namespace mpd {
namespace client {

namespace {

using Authorization = mpd::MPD::Authorization;

class TagParserImpl : public TagParser {
   public:
    // Parse parses the given tag, and returns the appropriate tag type.
    // If no matching tag is found, then an empty optional is returned.
    std::optional<enum mpd_tag_type> Parse(
        const std::string_view tag) const override;
};

std::optional<enum mpd_tag_type> TagParserImpl::Parse(
    const std::string_view tag_name) const {
    std::string name_with_null(tag_name);
    enum mpd_tag_type tag = mpd_tag_name_iparse(name_with_null.data());
    if (tag == MPD_TAG_UNKNOWN) {
        return std::nullopt;
    }
    return tag;
}

class SongImpl : public Song {
   public:
    // Create a new song based on the given mpd_song.
    SongImpl(struct mpd_song* song) : song_(song){};

    // Song is pointer owning. We do not support copies.
    SongImpl(Song&) = delete;
    SongImpl& operator=(SongImpl&) = delete;

    // However, moves are OK, because the "old" owner no longer exists.
    SongImpl(SongImpl&&) = default;

    // Free the wrapped struct mpd_song;
    ~SongImpl() override;

    std::optional<std::string> Tag(enum mpd_tag_type tag) const override;
    std::string URI() const override;

   private:
    // The wrapped song.
    struct mpd_song* song_;
};

SongImpl::~SongImpl() { mpd_song_free(song_); }

std::optional<std::string> SongImpl::Tag(enum mpd_tag_type tag) const {
    const char* raw_value = mpd_song_get_tag(song_, tag, 0);
    if (raw_value == nullptr) {
        return std::nullopt;
    }
    return std::string(raw_value);
}

std::string SongImpl::URI() const { return mpd_song_get_uri(song_); }

class StatusImpl : public Status {
   public:
    // Wrap the given mpd_status.
    StatusImpl(struct mpd_status* status) : status_(status){};

    // StatusImpl is pointer owning. We do not support copies.
    StatusImpl(StatusImpl&) = delete;
    StatusImpl& operator=(StatusImpl&) = delete;

    // Moves are OK, because the "old" owner no longer exists.
    StatusImpl(StatusImpl&&) = default;
    StatusImpl& operator=(StatusImpl&&) = default;

    // Free the wrapped status.
    ~StatusImpl() override;

    unsigned QueueLength() const override;
    bool Single() const override;
    std::optional<int> SongPosition() const override;
    bool IsPlaying() const override;

   private:
    struct mpd_status* status_;
};

StatusImpl::~StatusImpl() { mpd_status_free(status_); }

unsigned StatusImpl::QueueLength() const {
    return mpd_status_get_queue_length(status_);
}

bool StatusImpl::Single() const { return mpd_status_get_single(status_); }

std::optional<int> StatusImpl::SongPosition() const {
    int pos = mpd_status_get_song_pos(status_);
    if (pos == -1) {
        return std::nullopt;
    }
    return pos;
}

bool StatusImpl::IsPlaying() const {
    return mpd_status_get_state(status_) == MPD_STATE_PLAY;
}

// Forward declare SongReaderImpl for MPDImpl;
class SongReaderImpl;

class MPDImpl : public MPD {
   public:
    MPDImpl(struct mpd_connection* conn) : mpd_(conn){};

    // MPDImpl owns the connection pointer, no copies possible.
    MPDImpl(MPDImpl&) = delete;
    MPDImpl& operator=(MPDImpl&) = delete;

    MPDImpl(MPDImpl&&) = default;
    MPDImpl& operator=(MPDImpl&&) = default;

    ~MPDImpl() override;
    void Pause() override;
    void Play() override;
    void PlayAt(unsigned position) override;
    std::unique_ptr<Status> CurrentStatus() override;
    std::unique_ptr<SongReader> ListAll() override;
    std::optional<std::unique_ptr<Song>> Search(std::string_view uri) override;
    IdleEventSet Idle(const IdleEventSet&) override;
    void Add(const std::string& uri) override;
    MPD::PasswordStatus ApplyPassword(const std::string& password) override;
    Authorization CheckCommands(
        const std::vector<std::string_view>& cmds) override;

   private:
    friend SongReaderImpl;
    struct mpd_connection* mpd_;

    // Exits the program, printing the current MPD connection error message.
    void Fail();

    // Checks to see if the MPD connection has an error. If it does, it
    // calls Fail.
    void CheckFail();
};

class SongReaderImpl : public SongReader {
   public:
    SongReaderImpl(MPDImpl& mpd)
        : mpd_(mpd), song_(std::nullopt), has_song_(false){};

    // SongReaderImpl is also pointer owning (the pointer to the next song_).
    SongReaderImpl(SongReaderImpl&) = delete;
    SongReaderImpl& operator=(SongReaderImpl&) = delete;

    // As with the other types, moves are OK.
    SongReaderImpl(SongReaderImpl&&) = default;

    // Default destructor should work fine, since the std::optional owns
    // a unique_ptr to an actual Song. The generated destructor will destruct
    // that type correctly.
    ~SongReaderImpl() override = default;

    std::optional<std::unique_ptr<Song>> Next() override;
    bool Done() override;

   private:
    // Fetch the next song, and if there is a song store it in song_. If a song
    // has already been fetched (even if no song was returned), take no action.
    void FetchNext();

    MPDImpl& mpd_;
    std::optional<std::unique_ptr<Song>> song_;
    bool has_song_;
};

void SongReaderImpl::FetchNext() {
    if (has_song_) {
        return;
    }
    struct mpd_song* raw_song = mpd_recv_song(mpd_.mpd_);
    const enum mpd_error err = mpd_connection_get_error(mpd_.mpd_);
    if (err == MPD_ERROR_CLOSED) {
        std::cerr
            << "MPD server closed the connection while getting the list of\n"
            << "all songs. If MPD error logs say \"Output buffer is full\",\n"
            << "consider setting max_output_buffer_size to a higher value\n"
            << "(e.g. 32768) in your MPD config." << std::endl;
    }
    mpd_.CheckFail();
    if (raw_song == nullptr) {
        song_ = std::nullopt;
        return;
    }
    has_song_ = true;
    song_ = std::unique_ptr<Song>(new SongImpl(raw_song));
}

std::optional<std::unique_ptr<Song>> SongReaderImpl::Next() {
    FetchNext();
    has_song_ = false;
    return std::exchange(song_, std::nullopt);
}

bool SongReaderImpl::Done() {
    FetchNext();
    return !song_.has_value();
}

MPDImpl::~MPDImpl() { mpd_connection_free(mpd_); }

void MPDImpl::Fail() {
    assert(mpd_connection_get_error(mpd_) != MPD_ERROR_SUCCESS &&
           "must be an error present");
    Die("MPD error: %s", mpd_connection_get_error_message(mpd_));
}

void MPDImpl::CheckFail() {
    if (mpd_connection_get_error(mpd_) != MPD_ERROR_SUCCESS) {
        Fail();
    }
}

void MPDImpl::Pause() {
    if (!mpd_run_pause(mpd_, true)) {
        Fail();
    }
}

void MPDImpl::Play() {
    if (!mpd_run_pause(mpd_, false)) {
        Fail();
    }
}

void MPDImpl::PlayAt(unsigned position) {
    if (!mpd_run_play_pos(mpd_, position)) {
        Fail();
    }
}

std::unique_ptr<SongReader> MPDImpl::ListAll() {
    if (!mpd_send_list_all_meta(mpd_, NULL)) {
        Fail();
    }
    return std::unique_ptr<SongReader>(new SongReaderImpl(*this));
}

std::optional<std::unique_ptr<Song>> MPDImpl::Search(std::string_view uri) {
    // Copy to ensure URI buffer is null-terminated.
    std::string uri_copy(uri);
    mpd_search_db_songs(mpd_, true);
    mpd_search_add_uri_constraint(mpd_, MPD_OPERATOR_DEFAULT, uri_copy.data());
    if (!mpd_search_commit(mpd_)) {
        Fail();
    }

    struct mpd_song* raw_song = mpd_recv_song(mpd_);
    CheckFail();
    if (raw_song == nullptr) {
        return std::nullopt;
    }
    std::unique_ptr<Song> song = std::unique_ptr<Song>(new SongImpl(raw_song));

    /* even though we're searching for a single song, libmpdclient
     * still acts like we're reading a song list. We read an aditional
     * element to convince MPD this is the end of the song list. */
    raw_song = mpd_recv_song(mpd_);
    assert(raw_song == nullptr &&
           "search by URI should only ever find  one song");
    return std::optional<std::unique_ptr<Song>>(std::move(song));
}

IdleEventSet MPDImpl::Idle(const IdleEventSet& events) {
    enum mpd_idle occured = mpd_run_idle_mask(mpd_, events.Enum());
    CheckFail();
    return {static_cast<int>(occured)};
}

void MPDImpl::Add(const std::string& uri) {
    if (!mpd_run_add(mpd_, uri.data())) {
        Fail();
    }
}

std::unique_ptr<Status> MPDImpl::CurrentStatus() {
    struct mpd_status* status = mpd_run_status(mpd_);
    if (status == nullptr) {
        Fail();
    }
    return std::unique_ptr<Status>(new StatusImpl(status));
}

MPD::PasswordStatus MPDImpl::ApplyPassword(const std::string& password) {
    mpd_run_password(mpd_, password.data());
    const enum mpd_error err = mpd_connection_get_error(mpd_);
    if (err == MPD_ERROR_SUCCESS) {
        return MPD::PasswordStatus::kAccepted;
    }
    if (err != MPD_ERROR_SERVER) {
        Fail();
    }
    enum mpd_server_error serr = mpd_connection_get_server_error(mpd_);
    if (serr != MPD_SERVER_ERROR_PASSWORD) {
        Fail();
    }
    mpd_connection_clear_error(mpd_);
    return MPD::PasswordStatus::kRejected;
}

Authorization MPDImpl::CheckCommands(
    const std::vector<std::string_view>& cmds) {
    Authorization result;
    if (cmds.size() < 1) {
        // Empty command list is always allowed, and we don't need a round
        // trip to the server.
        result.authorized = true;
        return result;
    }

    // Fetch a list of the commands we're not allowed to run. In most
    // installs, this should be empty.
    if (!mpd_send_disallowed_commands(mpd_)) {
        CheckFail();
    }

    std::vector<std::string> disallowed;
    struct mpd_pair* command_pair = mpd_recv_command_pair(mpd_);
    while (command_pair != nullptr) {
        disallowed.push_back(command_pair->value);
        mpd_return_pair(mpd_, command_pair);
        command_pair = mpd_recv_command_pair(mpd_);
    }
    CheckFail();

    for (std::string_view cmd : cmds) {
        if (std::find(disallowed.begin(), disallowed.end(), cmd) !=
            disallowed.end()) {
            result.missing.emplace_back(cmd);
        }
    }
    // We're authorized as long as we are not missing any required commands.
    result.authorized = result.missing.size() == 0;
    return result;
}

class DialerImpl : public Dialer {
   public:
    ~DialerImpl() override = default;

    // Dial connects to the MPD instance at the given Address, optionally,
    // with the given timeout. On success a variant with a unique_ptr to
    // an MPD instance is returned. On failure, a string is returned with
    // a human-readable description of the error.
    Dialer::result Dial(
        const Address&,
        unsigned timeout_ms = Dialer::kDefaultTimeout) const override;
};

Dialer::result DialerImpl::Dial(const Address& addr,
                                unsigned timeout_ms) const {
    /* Create a new connection to mpd */
    struct mpd_connection* mpd =
        mpd_connection_new(addr.host.data(), addr.port, timeout_ms);
    if (mpd == nullptr) {
        return "could not connect to mpd: out of memory";
    }
    if (mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        return absl::StrFormat("could not connect to mpd at %s:%u: %s",
                               addr.host, addr.port,
                               mpd_connection_get_error_message(mpd));
    }
    return std::unique_ptr<MPD>(new MPDImpl(mpd));
}

}  // namespace

std::unique_ptr<mpd::TagParser> Parser() {
    return std::unique_ptr<mpd::TagParser>(new TagParserImpl());
}

std::unique_ptr<mpd::Dialer> Dialer() {
    return std::unique_ptr<mpd::Dialer>(new DialerImpl());
}

}  // namespace client
}  // namespace mpd
}  // namespace ashuffle
