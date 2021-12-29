#ifndef __ASHUFFLE_LOG_H__
#define __ASHUFFLE_LOG_H__

#include <string_view>

#include <absl/strings/str_format.h>

#include "log_internal.h"

namespace ashuffle {

class Log final {
    public:
        Log(log::SourceLocation loc = log::SourceLocation())
            : loc_(loc){}

        template <typename... Args>
        void Info(const absl::FormatSpec<Args...>& fmt, Args... args) {
            WriteLog(Level::kInfo, fmt, args...);
        }

        template <typename... Args>
        void Error(const absl::FormatSpec<Args...>& fmt, Args... args) {
            WriteLog(Level::kError, fmt, args...);
        }

    private:
        enum class Level {
            kInfo,
            kError,
        };

        template <typename... Args>
        void WriteLog(Level level, const absl::FormatSpec<Args...>& fmt, Args... args) {
            const char* level_str = nullptr;
            switch (level) {
            case Level::kInfo:
                level_str = "INFO";
                break;
            case Level::kError:
                level_str = "ERROR";
                break;
            }
            log::DefaultLogger().Stream() << level_str << " " << loc_ << ": " << absl::StrFormat(fmt, args...) << std::endl;
        }

        log::SourceLocation loc_;
};

namespace log {

// Set the output of the default logger to the given ostream. The ostream must
// have program lifetime.
void SetOutput(std::ostream& output);

} // namespace log
} // namespace ashuffle

#endif // __ASHUFFLE_LOG_H__

