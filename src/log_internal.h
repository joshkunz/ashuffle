#ifndef __ASHUFFLE_LOG_INTERNAL_H__
#define __ASHUFFLE_LOG_INTERNAL_H__

#include <cassert>
#include <iostream>
#include <fstream>

#include <absl/strings/str_format.h>

namespace ashuffle {
namespace log {

struct SourceLocation final {
    SourceLocation(const std::string_view file = __builtin_FILE(),
                   const std::string_view function = __builtin_FUNCTION(),
                   unsigned line = __builtin_LINE())
        : file(file), function(function), line(line) {}

    const std::string_view file;
    const std::string_view function;
    const unsigned line;
};

std::ostream& operator<<(std::ostream&, const SourceLocation&);

// Logger is an object that owns the output file and standard formatting
// for logs.
class Logger final {
 public:
     Logger(){};

     void SetOutput(std::ostream& output) {
         output_ = &output;
     }

     std::ostream& Stream() {
        static std::ofstream devnull("/dev/null");
         if (output_ != nullptr) {
             return *output_;
         }
         return devnull;
     }

 private:
     std::ostream* output_;
};

// Fetch the default logger.
Logger& DefaultLogger();

} // namespace log
} // namespace ashuffle


#endif // __ASHUFFLE_LOG_INTERNAL_H__
