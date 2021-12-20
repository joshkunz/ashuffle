#ifndef __ASHUFFLE_T_HELPER_H__
#define __ASHUFFLE_T_HELPER_H__

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <string_view>

#include "util.h"

namespace fs = ::std::filesystem;

namespace ashuffle {
namespace test_helper {

class TemporaryFile final {
   public:
    // Construct a new temporary file with the given contents.
    explicit TemporaryFile(std::string_view contents) {
        tmp_ = std::tmpfile();
        if (tmp_ == nullptr) {
            Die("Failed to open temporary file errno=%d", errno);
        }
        if (contents.size() != 0) {
            if (std::fwrite(contents.data(), contents.size(), 1, tmp_) != 1) {
                Die("Failed to write test contents into temporary file "
                    "errno=%d",
                    errno);
            }
        }
        // Make sure others can read our writes.
        if (std::fflush(tmp_)) {
            Die("Failed to flush test contents to temporary file errno=%d",
                errno);
        }
    }

    ~TemporaryFile() {
        // On destruction make sure we close the temporary file we own.
        std::fclose(tmp_);
    }

    // Path returns the path to the temporary file.
    std::string Path() const {
        return fs::path("/proc/self/fd") / std::to_string(fileno(tmp_));
    }

    // Move-only type.
    TemporaryFile(TemporaryFile&) = delete;
    TemporaryFile& operator=(TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&) = default;

   private:
    // The underlying temporary file.
    FILE* tmp_;
};

}  // namespace test_helper
}  // namespace ashuffle

#endif  // __ASHUFFLE_T_HELPER_H__
