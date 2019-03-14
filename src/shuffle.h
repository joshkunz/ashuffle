#include "list.h"

#ifndef ASHUFFLE_SHUFFLE_H
#define ASHUFFLE_SHUFFLE_H

struct shuffle_chain {
    unsigned max_window;
    struct list window;
    struct list pool;
};

/* initialize this shuffle chain */
void shuffle_init(struct shuffle_chain *, unsigned window_size);

/* Add an the item pointed to by 'data' of size 'size' to
 * the given chain */
void shuffle_add(struct shuffle_chain *, const char * data);

/* return the number of songs in the shuffle chain */
unsigned shuffle_length(struct shuffle_chain *);

/* Randomly pick an element added via 'shuffle_add' and return
 * a pointer to it. */
const char * shuffle_pick(struct shuffle_chain *);

/* Free memory associated with the shuffle chain. */
void shuffle_free(struct shuffle_chain *);

#endif
