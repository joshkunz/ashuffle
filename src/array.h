#ifndef ASHUFFLE_ARRAYP_H
#define ASHUFFLE_ARRAYP_H

struct auto_array {
    /* The length of the array */
    unsigned length;
    /* The number of elements we've alloc-ed */
    unsigned alloc_length;
    void ** array;
};

/* initialize the auto array */
int array_init(struct auto_array * ar);

/* append some data to the auto_array */
int array_append(struct auto_array * ar,
                 void const * value, 
                 size_t len);

/* Append the string to the array */
int array_append_s(struct auto_array *ar,
                   const char * value,
                   unsigned len);

int array_free(struct auto_array * ar);
int array_trim(struct auto_array * ar);
#endif 
