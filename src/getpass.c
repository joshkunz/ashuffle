#define _POSIX_SOURCE
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "getpass.h"

#define DEFAULT_GETLINE_BUFSIZE 100

static void * xmalloc(size_t size) {
    void * mem = malloc(size);
    if (mem == NULL) {
        perror("xmalloc");
        exit(1);
    }
    return mem;
}

static void getline(char ** lineptr, size_t *n, FILE *stream) {
    char * buf = *lineptr;
    size_t bufsize = *n;
    if (bufsize == 0) { 
        assert(buf == NULL && "got zero bufsize, but non-null buffer");
    }

    size_t read = 0;
    char c;
    do {
        if (read == bufsize) {
            char * tmp_buf = buf;
            size_t tmp_bufsize = bufsize;
            /* +1 so I don't have to special-case when bufsize = 0 */
            bufsize = (bufsize + 1) * 2;
            buf = xmalloc(bufsize);
            memcpy(buf, tmp_buf, tmp_bufsize);
        }
        if (fread(&c, 1, 1, stream) != 1) {
            perror("getline");
            exit(1);
        }
        if (c == '\n') { break; }
        buf[read] = c;
        read += 1;
    } while (true);


    buf = realloc(buf, read);
    if (buf == NULL) { perror("getline"); exit(1); }
    buf[read] = '\0';
    *lineptr = buf;
    *n = read;
}

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

char * getpass(FILE * in_stream, FILE * out_stream, const char *prompt) {
    if (fwrite(prompt, strlen(prompt), 1, out_stream) != 1) {
        perror("getpass (fwrite)");
        exit(1);
    }

    set_echo(out_stream, false, true);

    char * result = NULL;
    size_t result_size = 0;
    getline(&result, &result_size, in_stream);

    set_echo(out_stream, true, true);

    return result;
}
