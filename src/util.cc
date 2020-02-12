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
    char *fmt_nl = (char *)xmalloc(strlen(fmt) + 2);
    strcpy(fmt_nl, fmt);
    strcat(fmt_nl, "\n");

    vfprintf(stderr, fmt_nl, rest);

    va_end(rest);
    exit(1);
}

// "a" and "b" *point* to the objects (strings) being compared, so we
// dereference and adapt the types.
static int compare_str(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
}

void qsort_str(const char *arr[], unsigned len) {
    qsort(arr, len, sizeof(arr[0]), compare_str);
}

const char *bsearch_str(const char *haystack[], unsigned len,
                        const char *needle) {
    // Note: we take &needle here because we want to be able to use the same
    // "compare_str" routine across qsort and compare_str. The extra
    // dereference is no big deal.
    return (const char *)bsearch(&needle, haystack, len, sizeof(haystack[0]),
                                 compare_str);
}
