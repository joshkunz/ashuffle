#include "rule.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>

namespace ashuffle {

void Rule::AddPattern(enum mpd_tag_type tag, std::string value) {
    assert(tag != MPD_TAG_UNKNOWN && "cannot add unknown tag to pattern");
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    patterns_.push_back(Pattern(tag, value));
}

bool Rule::Accepts(const mpd::Song &song) const {
    assert(type_ == Rule::Type::kExclude &&
           "only exclusion rules are supported");
    for (const Pattern &p : patterns_) {
        std::optional<std::string> tag_value = song.Tag(p.tag);
        if (!tag_value) {
            // If the tag doesn't exist, we can't match on it. Accept this
            // song because it can't possible match this tag.
            return true;
        }

        // Lowercase the tag value, to make sure our comparison is not
        // case sensitive.
        std::transform(tag_value->begin(), tag_value->end(), tag_value->begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (tag_value->find(p.value) == std::string::npos) {
            // No substring match, this pattern does not match.
            return true;
        }
    }
    // All tags existed and matched. Reject this song.
    return false;
}

}  // namespace ashuffle
