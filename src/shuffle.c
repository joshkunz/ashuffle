#include <stdlib.h>
#include "list.h"
#include "shuffle.h"

#define RAND_REAL() ((double) rand() / (double)RAND_MAX)

int shuffle_init(struct shuffle_chain * s, unsigned window_size) {
    list_init(&s->pool);
    list_init(&s->window);
    s->max_window = window_size;

    return 0;
}

/* get then number of songs in the shuffle chain */
int shuffle_length(struct shuffle_chain *s) {
    return s->pool.length + s->window.length;
}

/* add an item to the chain by simply pushing it into the pool */
int shuffle_add(struct shuffle_chain * s, const void * data, size_t size) {
    list_push(&s->pool, node_from(data, size));
    return 0;
}

/* ensure that our window is as full as it can possibly be. */
static int fill_window(struct shuffle_chain *s) {
    /* while our window isn't full and there's songs in the pool */
    while (s->window.length <= s->max_window && s->pool.length > 0) {
        /* push a random song from the pool onto the end of the window */
        list_pop_push(&s->pool, &s->window, rand() % s->pool.length);
    }
    return 0;
}

/* Randomly pick an element added via 'shuffle_add' and return
 * a pointer to it. */
const void * shuffle_pick(struct shuffle_chain * s) {
    const void * data = NULL;
    fill_window(s);
    /* get the first element off the window */
    data = list_at(&s->window, 0);
    /* push the retrived element back into the pool */
    list_pop_push(&s->window, &s->pool, 0);
    return data;
}

/* Free memory associated with the shuffle chain. */
int shuffle_free(struct shuffle_chain * s) {
    list_free(&s->pool);
    list_free(&s->window);
    return 0;
}
