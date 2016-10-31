#define _GNU_SOURCE
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "getpass.h"

#define DEFAULT_GETLINE_BUFSIZE 100

#define set_flag(field, flag, state) \
    do { \
        if ((state)) { \
            (field) |= (flag); \
        } else { \
            (field) &= ~(flag); \
        } \
    } while (0) 

static void set_echo(FILE *stream, bool echo_state, bool echo_nl_state) {
    struct termios flags;
    int res = tcgetattr(fileno(stream), &flags);
    if (res != 0) {
        perror("set_echo (tcgetattr)");
        exit(1);
    }
    set_flag(flags.c_lflag, ECHO, echo_state);
    set_flag(flags.c_lflag, ECHONL, echo_nl_state);
    res = tcsetattr(fileno(stream), TCSANOW, &flags);
    if (res != 0) {
        perror("set_echo (tcsetattr)");
        exit(1);
    }
}

char * as_getpass(FILE * in_stream, FILE * out_stream, const char *prompt) {
    if (fwrite(prompt, strlen(prompt), 1, out_stream) != 1) {
        perror("getpass (fwrite)");
        exit(1);
    }

    set_echo(out_stream, false, true);

    char * result = NULL;
    size_t result_size = 0;
    ssize_t result_len = getline(&result, &result_size, in_stream);
    if (result_len < 0) {
        perror("getline (getpass)");
        exit(1);
    }

    set_echo(out_stream, true, true);

    return result;
}
