#ifndef __ASHUFFLE_ARGS_H__
#define __ASHUFFLE_ARGS_H__

#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "rule.h"

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
    std::vector<Rule> ruleset = {};
    unsigned queue_only = 0;
    FILE *file_in = nullptr;
    bool check_uris = true;
    unsigned queue_buffer = 0;
    std::optional<std::string> host = {};
    unsigned port = 0;
    // Special test-only options.
    struct {
        bool print_all_songs_and_exit = false;
    } test = {};

    // Parse parses the arguments in the given vector and returns ParseResult
    // based on the success/failure of the parse.
    static std::variant<Options, ParseError> Parse(
        const std::vector<std::string> &);

    // ParseFromC parses the arguments in the given c-style arguments list,
    // like would be given in `main`.
    static std::variant<Options, ParseError> ParseFromC(const char **argv,
                                                        int argc) {
        std::vector<std::string> args;
        for (int i = 0; i < argc; i++) {
            args.push_back(argv[i]);
        }
        return Options::Parse(args);
    }
};

// Print the help message on the given output stream.
void PrintHelp(FILE *output_stream);

#endif
