#include <stdlib.h>
#include <string.h>

#include "helpers.h"

// "a" and "b" *point* to the objects (strings) being compared, so we
// dereference and adapt the types.
static int compare_str(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
}

void qsort_str(const char *arr[], unsigned len) {
    qsort(arr, len, sizeof(arr[0]), compare_str);
}
