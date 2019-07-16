#define _GNU_SOURCE
#define _POSIX_C_SOURCE 201904L
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t size) {
    void *res = calloc(1, size);
    if (res == NULL) {
        perror("xmalloc");
        abort();
    }
    return res;
}

char *xstrdup(const char *to_dup) {
    char *res = strdup(to_dup);
    if (res == NULL) {
        perror("xstrdup");
        abort();
    }
    return res;
}

char *xsprintf(const char *fmt, ...) {
    va_list rest;
    va_start(rest, fmt);

    char *res = NULL;

    if (vasprintf(&res, fmt, rest) < 0) {
        perror("xsprintf");
        abort();
    }

    va_end(rest);

    return res;
}

void die(const char *fmt, ...) {
    va_list rest;
    va_start(rest, fmt);

    // +2 = 1 (newline) + 1 (null)
    char *fmt_nl = xmalloc(strlen(fmt) + 2);
    strcpy(fmt_nl, fmt);
    strcat(fmt_nl, "\n");

    vfprintf(stderr, fmt_nl, rest);

    va_end(rest);
    exit(1);
}
