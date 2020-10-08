#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <string_view>

#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "args.h"
#include "rule.h"

namespace ashuffle {
namespace {

constexpr char kHelpMessage[] =
    "usage: ashuffle [-h] [-n] [[-e PATTERN ...] ...] [-o NUMBER] "
    "[-f FILENAME] [-q NUMBER]\n"
    "    [-g TAG ...] [[-t TWEAK] ...]\n"
    "\n"
    "Optional Arguments:\n"
    "   -h,-?,--help      Display this help message.\n"
    "   -e,--exclude      Specify things to remove from shuffle (think\n"
    "                     blacklist).\n"
    "   -f,--file         Use MPD URI's found in 'file' instead of using the\n"
    "                     entire MPD library. You can supply `-` instead of a\n"
    "                     filename to retrive URI's from standard in. This\n"
    "                     can be used to pipe song URI's from another program\n"
    "                     into ashuffle.\n"
    "   --by-album        Same as '--group-by album date'.\n"
    "   -g,--group-by     Shuffle songs grouped by the given tags. For\n"
    "                     example 'album' could be used as the tag, and an\n"
    "                     entire album's worth of songs would be queued\n"
    "                     instead of one song at a time.\n"
    "   --host            Specify a hostname or IP address to connect to.\n"
    "                     Defaults to `localhost`.\n"
    "   -n,--no-check     When reading URIs from a file, don't check to\n"
    "                     ensure that the URIs match the given exclude rules.\n"
    "                     This option is most helpful when shuffling songs\n"
    "                     with -f, that aren't in the MPD library.\n"
    "   -o,--only         Instead of continuously adding songs, just add\n"
    "                     'NUMBER' songs and then exit.\n"
    "   -p,--port         Specify a port number to connect to. Defaults to\n"
    "                     `6600`.\n"
    "   -q,--queue-buffer Specify to keep a buffer of `n` songs queued after\n"
    "                     the currently playing song. This is to support MPD\n"
    "                     features like crossfade that don't work if there\n"
    "                     are no more songs in the queue.\n"
    "   -t,--tweak        Tweak an infrequently used ashuffle option. See\n"
    "                     `readme.md` for a list of available options.\n"
    "See included `readme.md` file for PATTERN syntax.\n";

// Parse the given string as a boolean. Produces an empty option if no value
// can be parsed.
std::optional<bool> ParseBool(std::string val) {
    std::transform(val.begin(), val.end(), val.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (val == "yes" || val == "true" || val == "on" || val == "1") {
        return true;
    }
    if (val == "no" || val == "false" || val == "off" || val == "0") {
        return false;
    }
    return std::nullopt;
}

class Parser {
   public:
    enum Status {
        kInProgress,
        kDone,
    };
    // Consume consumes the given argument and updates the state of the parser.
    // Consume returns the status of the parser, either "In Progress" or
    // Final. Once a final status is reached, future calls to Consume will
    // ignore the given argument. Note: Finish can be called before the final
    // state is reached, so waiting for the parser to reach a final state
    // is not a reasonable approach. Just feed in arguments, and then call
    // Finish once all arguments have been consumed.
    Status Consume(std::string_view);

    // Finish completes the parse and returns the parsed Options struct, or
    // a parser error.
    std::variant<Options, ParseError> Finish();

    // Constructs an empty parser. The given tagger is used to resolve
    // exclusion rule field names.
    Parser(const mpd::TagParser& tag_parser)
        : state_(kNone), tag_parser_(tag_parser){};

   private:
    enum State {
        kFile,         // Expecting file path
        kFinal,        // (final) Final state
        kError,        // (final) Error state
        kGroup,        // (generic) Expecting tag for group.
        kGroupBegin,   // Expecting first tag for group.
        kHost,         // Expecting hostname
        kNone,         // (generic) Default state, and initial state.
        kPort,         // Expecting port
        kQueue,        // Expecting --only value
        kQueueBuffer,  // Expecting queue buffer size
        kRule,         // (generic) Expecting rule tag
        kRuleBegin,    // Expecting first rule tag (not generic)
        kRuleValue,    // Expecting rule matcher for previous tag
        kTest,         // Expecting test-only flag name
        kTweak,        // Expecting a tweak
    };
    State state_;
    // opts_ is modified as tokens are `Consume`d.
    Options opts_;
    // err_ is set if a parse error occurs.
    ParseError err_;
    // prev_ is the previous token that was passed to consume. Initially the
    // empty token.
    std::string prev_;

    const mpd::TagParser& tag_parser_;
    Rule pending_rule_;
    enum mpd_tag_type rule_tag_;

    // Returns true if we are in a "Generic" state, where we can transfer
    // to any other option.
    bool InGenericState();

    // Returns true if the parser is in a "Final" state, where no future tokens
    // will be accepted.
    bool InFinalState();

    // Store the currently pending rule in `opts_`, and clear the pending rule.
    void FlushRule();

    // Actual consume logic is here. It maps an argument to a state update or
    // parse error as appropriate.
    std::variant<State, ParseError> ConsumeInternal(std::string_view arg);

    // Parse a tweak argument specifically (anything when we're in kTweak
    // state). Return value has the same semantics as ConsumeInternal.
    std::variant<State, ParseError> ParseTweak(std::string_view arg);
};

bool Parser::InGenericState() {
    return state_ == kNone || state_ == kRule || state_ == kGroup;
}

bool Parser::InFinalState() { return state_ == kFinal || state_ == kError; }

void Parser::FlushRule() {
    assert(!pending_rule_.Empty() &&
           "should not be possible to construct empty rule");
    opts_.ruleset.emplace_back(std::move(pending_rule_));
    pending_rule_ = Rule();
}

Parser::Status Parser::Consume(std::string_view arg) {
    if (InFinalState()) {
        // Fail if we receive any more tokens after we reach a final
        // state.
        return kDone;
    }
    std::variant<Parser::State, ParseError> next_or_err = ConsumeInternal(arg);
    if (ParseError* err = std::get_if<ParseError>(&next_or_err);
        err != nullptr) {
        err_ = *err;
        state_ = kError;
        return kDone;
    }
    prev_ = arg;
    Parser::State next = std::get<Parser::State>(next_or_err);
    // If we're transitioning out of a rule...
    if (state_ == kRule && next != kRule) {
        FlushRule();
    }
    state_ = next;
    return InFinalState() ? kDone : kInProgress;
}

std::variant<Options, ParseError> Parser::Finish() {
    if (state_ == kRule) {
        // If we're in the middle of parsing a rule, then flush the current
        // rule before finishing.
        FlushRule();
    }
    if (!InFinalState()) {
        // Finalize the parser if parsing is still in-progress. In a generic
        // state (ready to start a new arg), everything is peachy. However, if
        // we are expecting some specific value (non-generic), then we should
        // fail.
        if (InGenericState()) {
            state_ = kFinal;
        } else if (state_ == kRuleValue) {
            err_ = ParseError(
                absl::StrFormat("no value supplied for match '%s'", prev_));
            state_ = kError;
        } else {
            err_ = ParseError(
                absl::StrFormat("no argument supplied for '%s'", prev_));
            state_ = kError;
        }
    }
    if (state_ == kError) {
        return err_;
    }
    return std::exchange(opts_, Options());
}

std::variant<Parser::State, ParseError> Parser::ParseTweak(
    std::string_view arg) {
    std::vector<std::string> assignment = absl::StrSplit(arg, "=");
    // Match on things like 'window-size=' as well.
    if (assignment.size() < 2 || assignment[1].empty()) {
        return ParseError("tweak must be of the form <name>=<value>");
    }
    auto& key = assignment[0];
    std::vector<std::string> value_parts(assignment.begin() + 1,
                                         assignment.end());
    auto value = absl::StrJoin(value_parts, "=");

    if (key == "window-size") {
        if (!absl::SimpleAtoi(value, &opts_.tweak.window_size)) {
            return ParseError(absl::StrFormat(
                "couldn't convert window-size value '%s'", value));
        }
        if (opts_.tweak.window_size < 1) {
            return ParseError(absl::StrFormat(
                "tweak window-size must be >= 1 (%s given)", value));
        }
        return kNone;
    }

    if (key == "play-on-startup") {
        auto v = ParseBool(value);
        if (!v) {
            return ParseError(absl::StrFormat(
                "play-on-startup must be a boolean value ('%s' given)", value));
        }
        opts_.tweak.play_on_startup = *v;
        return kNone;
    }

    if (key == "suspend-timeout") {
        if (!absl::ParseDuration(value, &opts_.tweak.suspend_timeout)) {
            return ParseError(
                absl::StrFormat("suspend-timeout must be a duration with units "
                                "e.g., 250ms ('%s' given)",
                                value));
        }
        if (opts_.tweak.suspend_timeout < absl::ZeroDuration()) {
            return ParseError(absl::StrFormat(
                "suspend-timeout must be a positive duration ('%s' given)",
                value));
        }
        return kNone;
    }

    return ParseError(absl::StrFormat("unrecognized tweak '%s'", arg));
}

std::variant<Parser::State, ParseError> Parser::ConsumeInternal(
    std::string_view arg) {
    if (arg == "--help" || arg == "-h" || arg == "-?") {
        return ParseError(ParseError::Type::kHelp,
                          "the user requested help to be displayed");
    }
    if (InGenericState()) {
        if (arg == "--exclude" || arg == "-e") {
            return kRuleBegin;
        }
        if (arg == "--no-check" || arg == "-n") {
            opts_.check_uris = false;
            return kNone;
        }
        if (arg == "--queue-buffer" || arg == "-q") {
            return kQueueBuffer;
        }
        if (arg == "--only" || arg == "-o") {
            return kQueue;
        }
        if (arg == "--file" || arg == "-f") {
            return kFile;
        }
        if (arg == "--host") {
            return kHost;
        }
        if (arg == "--port" || arg == "-p") {
            return kPort;
        }
        if (arg == "--test_enable_option_do_not_use") {
            return kTest;
        }
        if (arg == "--group-by" || arg == "-g") {
            if (!opts_.group_by.empty()) {
                return ParseError(
                    absl::StrFormat("'%s' can only be provided once", arg));
            }
            return kGroupBegin;
        }
        if (arg == "--by-album") {
            if (!opts_.group_by.empty()) {
                return ParseError(
                    absl::StrFormat("'%s' can only be provided once", arg));
            }
            opts_.group_by.push_back(MPD_TAG_ALBUM);
            opts_.group_by.push_back(MPD_TAG_DATE);
            return kNone;
        }
        if (arg == "--tweak" || arg == "-t") {
            return kTweak;
        }
    }
    switch (state_) {
        case kTweak:
            return ParseTweak(arg);
        case kFile:
            if (arg == "-") {
                opts_.file_in = &std::cin;
            } else {
                std::string filepath(arg);
                opts_.InternalTakeIstream(
                    std::make_unique<std::ifstream>(filepath));
            }
            return kNone;
        case kHost:
            opts_.host = arg;
            return kNone;
        case kPort:
            if (!absl::SimpleAtoi(arg, &opts_.port)) {
                return ParseError(
                    absl::StrFormat("couldn't convert port value '%s'", arg));
            }
            return kNone;
        case kQueue:
            if (!absl::SimpleAtoi(arg, &opts_.queue_only)) {
                return ParseError(
                    absl::StrFormat("couldn't convert only value '%s'", arg));
            }
            return kNone;
        case kQueueBuffer:
            if (!absl::SimpleAtoi(arg, &opts_.queue_buffer)) {
                return ParseError(absl::StrFormat(
                    "couldn't convert queue_buffer value '%s'", arg));
            }
            return kNone;
        case kRule:
        case kRuleBegin: {
            std::optional<enum mpd_tag_type> tag = tag_parser_.Parse(arg);
            if (!tag) {
                return ParseError(
                    absl::StrFormat("invalid song tag name '%s'", arg));
            }
            rule_tag_ = *tag;
            return kRuleValue;
        }
        case kRuleValue:
            pending_rule_.AddPattern(rule_tag_, std::string(arg));
            return kRule;
        case kTest:
            if (arg == "print_all_songs_and_exit") {
                opts_.test.print_all_songs_and_exit = true;
                return kNone;
            }
            return ParseError(absl::StrFormat("bad test option '%s'", arg));
        case kGroup:
        case kGroupBegin: {
            std::optional<enum mpd_tag_type> tag = tag_parser_.Parse(arg);
            if (!tag) {
                return ParseError(
                    absl::StrFormat("invalid tag name '%s'", arg));
            }
            opts_.group_by.push_back(*tag);
            return kGroup;
        }
        case kFinal:
        case kError:
            assert(false && "unreachable, should not be possible");
        case kNone:
            return ParseError(absl::StrFormat("bad option '%s'", arg));
    }
    assert(false && "unreachable, invalid state");
    __builtin_unreachable();
}

}  // namespace

std::variant<Options, ParseError> Options::Parse(
    const mpd::TagParser& tag_parser, const std::vector<std::string>& args) {
    Parser p(tag_parser);
    for (std::string_view arg : args) {
        if (p.Consume(arg) == Parser::Status::kDone) {
            break;
        }
    }
    return p.Finish();
}

std::ostream& DisplayHelp(std::ostream& output) {
    output << kHelpMessage;
    return output;
}

std::ostream& operator<<(std::ostream& out, const ParseError& e) {
    std::string type;
    switch (e.type) {
        case ParseError::Type::kGeneric:
            type = "generic";
            break;
        case ParseError::Type::kHelp:
            type = "help";
            break;
        case ParseError::Type::kUnknown:
            type = "unknown";
            break;
        default:
            assert(false && "unreachable");
            __builtin_unreachable();
    }
    out << "ParseError(" << type << ", \"" << e.msg << "\")" << std::endl;
    return out;
}

}  // namespace ashuffle
