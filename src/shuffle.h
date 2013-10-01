#include "list.h"

#ifndef ASHUFFLE_SHUFFLE_H
#define ASHUFFLE_SHUFFLE_H

struct shuffle_chain {
    struct list song_list;
    unsigned length;
};

/* initialize this shuffle chain */
int shuffle_init(struct shuffle_chain *, double start_chance);

/* Add an the item pointed to by 'data' of size 'size' to
 * the given chain */
int shuffle_add(struct shuffle_chain *, const void * data, size_t size);

/* Randomly pick an element added via 'shuffle_add' and return
 * a pointer to it. */
const void * shuffle_pick(struct shuffle_chain *);

/* Free memory associated with the shuffle chain. */
int shuffle_free(struct shuffle_chain *);

#endif
