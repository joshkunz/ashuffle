#include "load.h"

#include <iostream>

#include <absl/strings/str_format.h>

namespace ashuffle {

namespace {

/* check wheter a song is allowed by the given ruleset */
bool RulesetAcceptsSong(const std::vector<Rule> &ruleset,
                        const mpd::Song &song) {
    for (const Rule &rule : ruleset) {
        if (!rule.Accepts(song)) {
            return false;
        }
    }
    return true;
}

}  // namespace

/* build the list of songs to shuffle from using MPD */
void MPDLoader::Load(ShuffleChain *songs) {
    std::unique_ptr<mpd::SongReader> reader = mpd_->ListAll();
    while (!reader->Done()) {
        std::unique_ptr<mpd::Song> song = *reader->Next();
        if (RulesetAcceptsSong(rules_, *song)) {
            songs->Add(song->URI());
        }
    }
}

FileLoader::~FileLoader() { fclose(file_); }

bool FileLoader::Verify(std::string_view) {
    // The base file loader verifies all songs. It's the "no-check" version.
    return true;
}

void FileLoader::Load(ShuffleChain *songs) {
    char *uri = NULL;
    ssize_t length = 0;
    size_t ignored = 0;

    length = getline(&uri, &ignored, file_);
    while (!feof(file_) && !ferror(file_)) {
        if (length < 1) {
            Die("invalid URI in input stream");
        }

        // If this line has terminating newline attached, set it to null
        // (effectively removing the newline).
        if (uri[length - 1] == '\n') {
            length -= 1;
            uri[length] = '\0';
        }

        if (Verify(uri)) {
            songs->Add(std::string(uri));
        }

        free(uri);
        uri = NULL;

        // Get the next URI line.
        length = getline(&uri, &ignored, file_);
    }

    // Free any memory allocated by our final getline.
    if (uri != NULL) {
        free(uri);
    }
}

CheckFileLoader::CheckFileLoader(mpd::MPD *mpd,
                                 const std::vector<Rule> &ruleset, FILE *file)
    : FileLoader(file), mpd_(mpd), rules_(ruleset) {
    std::unique_ptr<mpd::SongReader> reader = mpd->ListAll();
    while (!reader->Done()) {
        all_uris_.push_back((*reader->Next())->URI());
    }
    std::sort(all_uris_.begin(), all_uris_.end());
}

bool CheckFileLoader::Verify(std::string_view uri) {
    if (all_uris_.empty()) {
        // No URIs in MPD, so the song can't possibly exist.
        return false;
    }

    if (!std::binary_search(all_uris_.begin(), all_uris_.end(), uri)) {
        // We have some uris in `all_uris', but the given URI
        // is not in there. Skip this URI.
        return false;
    }

    if (rules_.empty()) {
        // If the song does exist in MPD's library, and we don't have any
        // rules to check, then we're done \o/.
        return true;
    }

    // If we think the song actually exists, and we have rules to verify,
    // then we need to fetch the song from MPD. This is quite expensive
    // unfortunately :(.

    std::optional<std::unique_ptr<mpd::Song>> song = mpd_->Search(uri);
    // The song doesn't exist in MPD's database, can't match ruleset.
    if (!song) {
        std::cerr << absl::StrFormat("Song URI '%s' not found", uri)
                  << std::endl;
        return false;
    }

    return RulesetAcceptsSong(rules_, **song);
}

}  // namespace ashuffle
