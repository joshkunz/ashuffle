#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "shuffle.h"

void shuffle_init(struct shuffle_chain *s, unsigned window_size) {
    list_init(&s->pool);
    list_init(&s->window);
    s->max_window = window_size;
}

/* get then number of songs in the shuffle chain */
unsigned shuffle_length(struct shuffle_chain *s) {
    return s->pool.length + s->window.length;
}

/* add an item to the chain by pushing it into the pool */
void shuffle_add(struct shuffle_chain *s, const char *item) {
    list_push_str(&s->pool, item);
}

/* ensure that our window is as full as it can possibly be. */
static void fill_window(struct shuffle_chain *s) {
    /* while our window isn't full and there's songs in the pool */
    while (s->window.length <= s->max_window && s->pool.length > 0) {
        /* push a random song from the pool onto the end of the window */
        list_pop_push(&s->pool, &s->window, rand() % s->pool.length);
    }
}

/* Randomly pick an element added via 'shuffle_add' and return
 * a pointer to it. */
const char *shuffle_pick(struct shuffle_chain *s) {
    const void *data = NULL;
    if (shuffle_length(s) == 0) {
        fprintf(stderr, "shuffle_pick: cannot pick from empty chain.");
        abort();
    }
    fill_window(s);
    /* get the first element off the window */
    data = list_at_str(&s->window, 0);
    /* push the retrived element back into the pool */
    list_pop_push(&s->window, &s->pool, 0);
    return (const char *) data;
}

void shuffle_items(const struct shuffle_chain *s, struct list *out) {
    assert(out != NULL && "output list must not be null");
    assert(out->length == 0 && "output list must be empty");

    for (unsigned i = 0; i < s->window.length; i++) {
        list_push_str(out, list_at_str(&s->window, i));
    }
    for (unsigned i = 0; i < s->pool.length; i++) {
        list_push_str(out, list_at_str(&s->pool, i));
    }
}

/* Free memory associated with the shuffle chain. */
void shuffle_free(struct shuffle_chain *s) {
    list_free(&s->pool);
    list_free(&s->window);
}
