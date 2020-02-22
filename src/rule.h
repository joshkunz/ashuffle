#ifndef __ASHUFFLE_RULE_H__
#define __ASHUFFLE_RULE_H__

#include <string>
#include <vector>

#include <mpd/tag.h>

// Internal API.
struct Pattern {
    enum mpd_tag_type tag;
    std::string value;
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

    // Construct a new exclusion rule.
    Rule() : Rule(Type::kExclude){};

    // Construct a new rule with the specific type.
    Rule(Type t) : type_(t){};

    // Type returns the type of this rule.
    Type GetType() const { return type_; }

    // Empty returns true when this rule matches no patterns.
    inline bool Empty() const { return patterns_.empty(); }

    // Status is the type returned from AddPattern. If the pattern was
    // successfully added, kOK is returned. If we failed to add the pattern,
    // we return kFail.
    enum Status {
        kOK,
        kFail,
    };

    // Add the given pattern to this rule.
    Status AddPattern(const std::string &field, std::string value);

    // Returns true if the given song is "accepted" by the rule. Whether or
    // not a song is accepted depends on the "type" of the rule. E.g., for an
    // exclude rule (type kExclude) if the song matched a rule pattern, the
    // song would *not* be accepted.
    bool Accepts(const struct mpd_song *song) const;

   private:
    std::vector<Pattern> patterns_;
    Type type_;
};

#endif
