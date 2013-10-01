#include <stdlib.h>
#include "list.h"
#include "shuffle.h"

#include <stdio.h>

#define RAND_REAL() ((double) rand() / (double)RAND_MAX)

struct song_entry {
    const char * song;
    double prob;
};

/* initialize this shuffle chain */
int shuffle_init(struct shuffle_chain * s, double start_chance) {
    list_init

    if (start_chance < 0 || start_chance > 1) { return -1; }
    s->base_chance = start_chance;
    s->length = 0;

    /* initalize our chain of lists */
    list_init(&s->chain_list);

    /* create an initalize our temporary list */
    struct list tmp_list;
    list_init(&tmp_list);

    /* push this temporary struct onto the chain */
    list_push(&s->chain_list, node_from(&tmp_list, sizeof(struct list)));

    return 0;
}


/* To add an item to the list we just push it onto the first list in
 * the chain */
int shuffle_add(struct shuffle_chain * s, const void * data, size_t size) {
    struct list * first_list = list_at(&s->chain_list, 0);
    list_push(first_list, node_from(data, size));
    s->length++;
    return 0;
}

/* given a node and the index in the chain of the current
 * list it's in, push the item to the next list, cleaning up
 * empty lists and provisioning new lists on the way. */
int shuffle_push_next(struct shuffle_chain * s, unsigned chain, unsigned index) {
    struct list * from = list_at(&s->chain_list, chain);
    /* if this is the last chain in the list, add a new chain */
    if (chain == (s->chain_list.length - 1)) {
        struct list tmp;
        list_init(&tmp);
        list_push(&s->chain_list, node_from(&tmp, sizeof(struct list)));
    }

    struct list * to = list_at(&s->chain_list, chain + 1);
    /* pop the item off this list and add it to the next one */
    list_pop_push(from, to, index);

    /* if the list we popped from is empty, delete it.
     * The next chain will move into this position automatically */
    if (from->length == 0) {
        list_pop(&s->chain_list, chain);
    }

    return 0;
}

/* Randomly pick an element added via 'shuffle_add' and return
 * a pointer to it. */
const void * shuffle_pick(struct shuffle_chain * s) {
    double prob = RAND_REAL();
    double current_chance = s->base_chance;
    unsigned chain = 0;
    /* find the matched list */
    for (; chain < (s->chain_list.length - 1); chain++) {
        if (current_chance > prob) { break; }
        current_chance += s->base_chance * (1 - current_chance);
    }

    printf("[rolled %.4f Using chain: %u\n", prob, chain);

    struct list * list = list_at(&s->chain_list, chain);
    unsigned index = rand() % list->length;
    void * data = list_at(list, index);
    shuffle_push_next(s, chain, index);
    return data;
}

/* Free memory associated with the shuffle chain. */
int shuffle_free(struct shuffle_chain * s) {
    /* free all chain lists */
    for (unsigned i = 0; i < s->chain_list.length; i++) {
        list_free(list_at(&s->chain_list, i));
    }
    /* free the current list */
    list_free(&s->chain_list);
    return 0;
}
