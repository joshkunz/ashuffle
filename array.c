#include <stdlib.h>
#include <string.h>
#include "array.h"

/* initialize the auto array */
int array_init(struct auto_array * ar) {
    ar->length = 0;
    ar->alloc_length = 0;
    ar->array = NULL;
    return 0;
}

int array_append(struct auto_array * ar, 
                 const void * data, 
                 size_t size) {
    /* if this array is full, reallocate it larger */
    if (ar->length == ar->alloc_length) {
        ar->alloc_length = ar->length + 1 + (ar->length / 3);
        ar->array = realloc(ar->array, sizeof(void *) * ar->alloc_length);
    }
    ar->array[ar->length] = malloc(size);
    /* Copy the data into the new memory block */
    memcpy(ar->array[ar->length], data, size);
    /* Increment the length of the array */
    ar->length++;
    return 0;
}

int array_append_s(struct auto_array * ar,
                   const char * astr,
                   unsigned length) {
    return array_append(ar, astr, sizeof(char) * length);
}

/* Free the entire array */
int array_free(struct auto_array * ar) {
    for (unsigned i = 0; i < ar->length; i++) {
        free(ar->array[i]);
    }
    free(ar->array);
    array_init(ar);
    return 0;
}

/* Trim the auto_allocated array to the minumim size */
int array_trim(struct auto_array * ar) {
    if (ar->length == ar->alloc_length) { return 0; }
    /* save a reference to the array */
    void ** tmp = ar->array;
    /* Allocate a minimum sized array */
    ar->array = malloc(ar->length * sizeof(void *));
    /* Copy the old contents */
    memcpy(ar->array, tmp, ar->length * sizeof(void *));
    /* re-set the allocated length */
    ar->alloc_length = ar->length;
    /* free the old array */
    free(tmp);
    return 0;
}
