#include <termios.h>
#include <unistd.h>
#include <cstdlib>
#include <string>

#include "getpass.h"

namespace {

template <typename FieldT, typename FlagT>
void SetFlag(FieldT &field, FlagT flag, bool state) {
    if (state) {
        field |= flag;
    } else {
        field &= ~flag;
    }
}

void SetEcho(FILE *stream, bool echo_state, bool echo_nl_state) {
    struct termios flags;
    int res = tcgetattr(fileno(stream), &flags);
    if (res != 0) {
        perror("SetEcho (tcgetattr)");
        std::exit(1);
    }
    SetFlag(flags.c_lflag, ECHO, echo_state);
    SetFlag(flags.c_lflag, ECHONL, echo_nl_state);
    res = tcsetattr(fileno(stream), TCSANOW, &flags);
    if (res != 0) {
        perror("SetEcho (tcsetattr)");
        std::exit(1);
    }
}

}  // namespace

namespace ashuffle {

std::string GetPass(FILE *in_stream, FILE *out_stream,
                    std::string_view prompt) {
    if (fwrite(prompt.data(), prompt.size(), 1, out_stream) != 1) {
        perror("getpass (fwrite)");
        std::exit(1);
    }
    if (fflush(out_stream) != 0) {
        perror("getpass (fflush)");
        std::exit(1);
    }

    SetEcho(out_stream, false, true);

    char *result = NULL;
    size_t result_size = 0;
    ssize_t result_len = getline(&result, &result_size, in_stream);
    if (result_len < 0) {
        perror("getline (getpass)");
        exit(1);
    }
    // Trim off the trailing newline, if it exists
    if (result[result_len - 1] == '\n') {
        result[result_len - 1] = '\0';
    }

    SetEcho(out_stream, true, true);

    return result;
}

}  // namespace ashuffle
