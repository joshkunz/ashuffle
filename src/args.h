#ifndef __ASHUFFLE_ARGS_H__
#define __ASHUFFLE_ARGS_H__

#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "mpd.h"
#include "rule.h"

namespace ashuffle {

struct ParseError {
    enum Type {
        kUnknown,  // Initial error type, unknown error.
        kGeneric,  // Generic failure. Described by 'msg'.
        kHelp,     // The user requested the help to be printed.
    };
    Type type;
    std::string msg;

    ParseError() : type(kUnknown){};
    ParseError(std::string_view m) : ParseError(kGeneric, m){};
    ParseError(Type t, std::string_view m) : type(t), msg(m){};
};

class Options {
   public:
    std::vector<Rule> ruleset;
    unsigned queue_only = 0;
    std::istream *file_in = nullptr;
    bool check_uris = true;
    unsigned queue_buffer = 0;
    std::optional<std::string> host = {};
    unsigned port = 0;
    // Special test-only options.
    struct {
        bool print_all_songs_and_exit = false;
    } test = {};
    std::vector<enum mpd_tag_type> group_by = {};

    // Parse parses the arguments in the given vector and returns ParseResult
    // based on the success/failure of the parse.
    static std::variant<Options, ParseError> Parse(
        const mpd::TagParser &, const std::vector<std::string> &);

    // ParseFromC parses the arguments in the given c-style arguments list,
    // like would be given in `main`.
    static std::variant<Options, ParseError> ParseFromC(
        const mpd::TagParser &tag_parser, const char **argv, int argc) {
        std::vector<std::string> args;
        // Start from '1' to skip the program name itself.
        for (int i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }
        return Options::Parse(tag_parser, args);
    }

    // Take ownership fo the given istream, and set the file_in member to
    // point to the referenced istream. This should only be used while the
    // Options are being constructed.
    void InternalTakeIstream(std::unique_ptr<std::istream> &&is) {
        file_in = is.get();
        owned_file_ = std::move(is);
    };

   private:
    // The owned_file is set if this Options class owns the file_in ptr.
    // The file_in ptr is only *sometimes* owned. For example, the file_in ptr
    // may point to std::cin, which has static lifetime, and is not owned by
    // this object.
    std::unique_ptr<std::istream> owned_file_;
};

// Print the help message on the given output stream, and return the input
// ostream.
std::ostream &DisplayHelp(std::ostream &);

}  // namespace ashuffle

#endif
