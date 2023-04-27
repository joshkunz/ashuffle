#ifndef __ASHUFFLE_LOG_H__
#define __ASHUFFLE_LOG_H__

#include <string_view>

#include <absl/strings/str_format.h>

#include "log_internal.h"

namespace ashuffle {

class Log final {
   public:
    Log(log::SourceLocation loc = log::SourceLocation()) : loc_(loc) {}

    template <typename... Args>
    void Info(const absl::FormatSpec<Args...>& fmt, Args... args) {
        WriteLog(Level::kInfo, fmt, args...);
    }

    void InfoStr(std::string_view message) {
        WriteLogStr(Level::kInfo, message);
    }

    template <typename... Args>
    void Error(const absl::FormatSpec<Args...>& fmt, Args... args) {
        WriteLog(Level::kError, fmt, args...);
    }

    void ErrorStr(std::string_view message) {
        WriteLogStr(Level::kError, message);
    }

   private:
    enum class Level {
        kInfo,
        kError,
    };

    friend std::ostream& operator<<(std::ostream&, const Level&);

    void WriteLogStr(Level level, std::string_view message) {
        log::DefaultLogger().Stream()
            << level << " " << loc_ << ": " << message << std::endl;
    }

    template <typename... Args>
    void WriteLog(Level level, const absl::FormatSpec<Args...>& fmt,
                  Args... args) {
        log::DefaultLogger().Stream()
            << level << " " << loc_ << ": " << absl::StrFormat(fmt, args...)
            << std::endl;
    }

    log::SourceLocation loc_;
};

namespace log {

// Set the output of the default logger to the given ostream. The ostream must
// have program lifetime.
void SetOutput(std::ostream& output);

}  // namespace log
}  // namespace ashuffle

#endif  // __ASHUFFLE_LOG_H__
