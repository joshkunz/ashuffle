#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>

#include <mpd/song.h>

#include "rule.h"

namespace ashuffle {

Rule::Status Rule::AddPattern(const std::string &field, std::string value) {
    Pattern p;
    p.tag = mpd_tag_name_iparse(field.data());
    if (p.tag == MPD_TAG_UNKNOWN) {
        return Status::kFail;
    }
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    p.value = value;
    patterns_.push_back(p);
    return Status::kOK;
}

bool Rule::Accepts(const struct ::mpd_song *song) const {
    assert(type_ == Rule::Type::kExclude &&
           "only exclusion rules are supported");
    for (const Pattern &p : patterns_) {
        const char *raw_tag_value = mpd_song_get_tag(song, p.tag, 0);
        if (raw_tag_value == nullptr) {
            // If the tag doesn't exist, we can't match on it. Just skip this
            // pattern.
            continue;
        }

        std::string tag_value(raw_tag_value);
        // Lowercase the tag value, to make sure our comparison is not
        // case sensitive.
        std::transform(tag_value.begin(), tag_value.end(), tag_value.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (tag_value.find(p.value) == std::string::npos) {
            // No substring match, this pattern does not match.
            continue;
        }

        return false;
    }
    return true;
}

}  // namespace ashuffle
