#define _GNU_SOURCE
#define _POSIX_C_SOURCE 201904L
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void * xmalloc(size_t size) {
    void * res = calloc(1, size);
    if (res == NULL) {
        perror("xmalloc");
        abort();
    }
    return res;
}

char * xstrdup(const char * to_dup) {
    char * res = strdup(to_dup);
    if (res == NULL) {
        perror("xstrdup");
        abort();
    }
    return res;
}

char * xsprintf(const char * fmt, ...) {
    va_list rest;
    va_start(rest, fmt);

    char * res = NULL;

    if (vasprintf(&res, fmt, rest) < 0) {
        perror("xsprintf");
        abort();
    }

    return res;
}
