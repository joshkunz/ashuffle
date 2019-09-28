#ifndef __ASHUFFLE_UTIL_H__
#define __ASHUFFLE_UTIL_H__

/* Equivalent to malloc, but initializes allocated memory to 0 (using calloc),
 * and crashes when the requested memory cannot be allocated. xmalloc cannot
 * return NULL. */
void *xmalloc(size_t size);

/* Equivalent to strdup, but it crashes the program when it fails to allocate
 * memory. */
char *xstrdup(const char *);

/* Equivalent to asprintf, but it cannot fail. */
char *xsprintf(const char *fmt, ...);

/* Exit the program after displaying the given error. */
void die(const char *fmt, ...);

/* Do an in-place qsort on the (const char *) array `arr`. `len` is the length
 * of the array `arr`. */
void qsort_str(const char *arr[], unsigned len);

#endif
