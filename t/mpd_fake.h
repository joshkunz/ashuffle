#ifndef __ASHUFFLE_T_MPD_FAKE_H__
#define __ASHUFFLE_T_MPD_FAKE_H__

#include <string>
#include <unordered_map>

#include <mpd/tag.h>

#include "mpd.h"

namespace ashuffle {
namespace fake {

class Song : public mpd::Song {
   public:
    using tag_map = std::unordered_map<enum mpd_tag_type, std::string>;
    std::string uri;
    tag_map tags;

    Song() : Song("", {}){};
    Song(tag_map t) : Song("", t){};
    Song(std::string_view u, tag_map t) : uri(u), tags(t){};

    std::optional<std::string> Tag(enum mpd_tag_type tag) const override {
        if (tags.find(tag) == tags.end()) {
            return std::nullopt;
        }
        return tags.at(tag);
    }

    std::string URI() const override { return uri; }
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

}  // namespace fake
}  // namespace ashuffle

#endif  // __ASHUFFLE_T_MPD_FAKE_H__
