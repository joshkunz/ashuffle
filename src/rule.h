#ifndef __ASHUFFLE_RULE_H__
#define __ASHUFFLE_RULE_H__

#include <string>
#include <vector>

#include <mpd/tag.h>

#include "mpd.h"

namespace ashuffle {

// Internal API.
struct Pattern {
    enum mpd_tag_type tag;
    std::string value;

    Pattern(enum mpd_tag_type t, std::string_view v) : tag(t), value(v){};
};

// Rule represents a set of patterns (song attribute/value pairs) that should
// be matched against song values.
class Rule {
   public:
    // Type represents the type of this rule.
    enum Type {
        // kExclude is the type of "exclusion" rules. Songs are only accepted
        // by exclusion rules when no rule patterns match. All songs match
        // the empty rule.
        kExclude,
    };

    // Construct a new exclusion rule .
    Rule() : Rule(Type::kExclude){};
    Rule(Type t) : type_(t){};

    // Type returns the type of this rule.
    Type GetType() const { return type_; }

    // Empty returns true when this rule matches no patterns.
    inline bool Empty() const { return patterns_.empty(); }

    // Size returns the number of patterns in this rule.
    inline size_t Size() const { return patterns_.size(); }

    // Add the given pattern to this rule.
    void AddPattern(enum mpd_tag_type, std::string value);

    // Returns true if the given song is "accepted" by the rule. Whether or
    // not a song is accepted depends on the "type" of the rule. E.g., for an
    // exclude rule (type kExclude) if the song matched a rule pattern, the
    // song would *not* be accepted.
    bool Accepts(const mpd::Song &song) const;

   private:
    Type type_;
    std::vector<Pattern> patterns_;
};

}  // namespace ashuffle

#endif
