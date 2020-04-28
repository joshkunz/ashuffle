#include "load.h"

#include <iostream>

#include <absl/strings/str_format.h>

namespace ashuffle {

/* build the list of songs to shuffle from using MPD */
void MPDLoader::Load(ShuffleChain *songs) {
    std::unique_ptr<mpd::SongReader> reader = mpd_->ListAll();
    while (!reader->Done()) {
        std::unique_ptr<mpd::Song> song = *reader->Next();
        if (!Verify(*song)) {
            continue;
        }
        songs->Add(song->URI());
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
                             std::istream *file)
    : MPDLoader(mpd, ruleset), file_(file) {
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
