#include "load.h"

#include <iostream>
#include <unordered_map>
#include <vector>

#include <absl/hash/hash.h>
#include <absl/strings/str_format.h>

namespace ashuffle {

namespace {

// A Group is a vector of field values, present or not.
typedef std::vector<std::optional<std::string>> Group;

// A GroupMap is a mapping from Groups to song URI vectors of the URIs in the
// given group.
typedef std::unordered_map<Group, std::vector<std::string>, absl::Hash<Group>>
    GroupMap;

}  // namespace

/* build the list of songs to shuffle from using MPD */
void MPDLoader::Load(ShuffleChain *songs) {
    GroupMap groups;

    mpd::MPD::MetadataOption metadata = mpd::MPD::MetadataOption::kInclude;
    if (rules_.empty() && group_by_.empty()) {
        // If we don't need to process any rules, or group tracks, then we
        // can omit metadata from the query. This is an optimization,
        // mainly to avoid
        // https://github.com/MusicPlayerDaemon/libmpdclient/issues/69
        metadata = mpd::MPD::MetadataOption::kOmit;
    }

    auto reader_or = mpd_->ListAll(metadata);
    if (!reader_or.ok()) {
        Die("Failed to get reader: %s", reader_or.status().ToString());
    }
    std::unique_ptr<mpd::SongReader> reader = std::move(*reader_or);

    while (!reader->Done()) {
        std::unique_ptr<mpd::Song> song = *reader->Next();
        if (!Verify(*song)) {
            continue;
        }

        if (group_by_.empty()) {
            songs->Add(song->URI());
            continue;
        }
        Group group;
        for (auto &field : group_by_) {
            group.emplace_back(song->Tag(field));
        }
        groups[group].push_back(song->URI());
    }

    if (group_by_.empty()) {
        return;
    }

    for (auto &&[_, group] : groups) {
        songs->Add(group);
    }
}

bool MPDLoader::Verify(const mpd::Song &song) {
    for (const Rule &rule : rules_) {
        if (!rule.Accepts(song)) {
            return false;
        }
    }
    return true;
}

FileMPDLoader::FileMPDLoader(mpd::MPD *mpd, const std::vector<Rule> &ruleset,
                             const std::vector<enum mpd_tag_type> &group_by,
                             std::istream *file)
    : MPDLoader(mpd, ruleset, group_by), file_(file) {
    for (std::string uri; std::getline(*file_, uri);) {
        valid_uris_.emplace_back(uri);
    }
    std::sort(valid_uris_.begin(), valid_uris_.end());
}

bool FileMPDLoader::Verify(const mpd::Song &song) {
    if (!std::binary_search(valid_uris_.begin(), valid_uris_.end(),
                            song.URI())) {
        // If the URI for this song is not in the list of valid_uris_, then
        // it shouldn't be loaded by this loader.
        return false;
    }

    // Otherwise, just check against the normal rules.
    return MPDLoader::Verify(song);
}

void FileLoader::Load(ShuffleChain *songs) {
    for (std::string uri; std::getline(*file_, uri);) {
        songs->Add(uri);
    }
}

}  // namespace ashuffle
