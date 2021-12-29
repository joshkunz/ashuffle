#include <log.h>

#include <iostream>

namespace ashuffle {
namespace log {

std::ostream& operator<<(std::ostream& out, const SourceLocation& loc) {
    out << loc.file << ":" << loc.line << " in " << loc.function;
    return out;
}

void SetOutput(std::ostream& output) {
    DefaultLogger().SetOutput(output);
}

Logger& DefaultLogger() {
    // static-lifetime logger.
    static Logger logger;
    return logger;
}

} // namespace log
} // namespace ashuffle

