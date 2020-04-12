#ifndef __ASHUFFLE_UTIL_H__
#define __ASHUFFLE_UTIL_H__

#include <cstdlib>
#include <iostream>

#include <absl/strings/str_format.h>

namespace {

// Die logs the given message as if it was printed via `absl::StrFormat`,
// and then terminates the program with with an error status code.
template <typename... Args>
void Die(absl::FormatSpec<Args...> format, Args... vars) {
    std::cerr << absl::StrFormat(format, vars...) << std::endl;
    std::exit(EXIT_FAILURE);
}

}  // namespace

#endif  // __ASHUFFLE_UTIL_H__
