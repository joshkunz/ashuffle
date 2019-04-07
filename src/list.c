#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "list.h"
#include "util.h"

struct node {
    struct datum data;
    struct node * next;
};

static void exit_oob(const struct list * l, unsigned index) {
    fprintf(stderr, "index %d is out of bounds in list %p\n", index, (void *)l);
    exit(1);
}

void list_init(struct list * list) {
    list->length = 0;
    list->_list = NULL;
}

/* add an item to the end of the list */
void list_node_push(struct list * l, struct node * n) {
    /* allocate a pointer that points to the location we'll
     * eventually store our node into */
    struct node ** next = &l->_list;
    while (*next != NULL) {
        next = &(*next)->next;
    }
    *next = n;
    l->length++;
}

/* Fetch the node at a given index. Returns NULL if there is no node at that
 * index. If `oprev` is not NULL, a pointer to the previous node's `next`
 * field (or the start of the list if this is the first node) will be stored
 * there. */
static struct node * list_node_at(const struct list * l, unsigned index, struct node * const ** oprev) {
    /* if there's no data in the list, fail */
    if (l->_list == NULL) { return NULL; }
    struct node * current = l->_list;
    struct node * const * previous = &l->_list;
    for (; index > 0; index--) {
        if (current->next == NULL) { return NULL; }
        previous = &current->next;
        current = current->next;
    }
    if (oprev != NULL) {
        *oprev = previous;
    }
    return current;
}

/* Remove and return the node at `index` from the list. If `index` is out
 * of bounds, NULL is returned. */
struct node * list_node_extract(struct list * l, unsigned index) {
    struct node ** prev;
    struct node * n = list_node_at(l, index, (struct node * const **) &prev);
    if (n == NULL) {
        return n;
    }
    /* set the previous node's 'next' value to the current
     * nodes next value */
    *prev = n->next;
    /* null out this node's next value since it's not part of
     * a list anymore */
    n->next = NULL;
    l->length--;
    return n;
}

static void datum_copy_into(struct datum * dst, const struct datum * src) {
    dst->length = src->length;
    dst->data = xmalloc(src->length);
    memcpy(dst->data, src->data, src->length);
}

void list_push(struct list * l, const struct datum * d) {
    if (d == NULL) {
        return;
    }
    struct node * node = xmalloc(sizeof(struct node));
    datum_copy_into(&node->data, d);
    list_node_push(l, node);
}

/* Push the given null-terminated string onto the list. A new datum is
 * created automatically. */
void list_push_str(struct list * l, const char * s) {
    struct node * node = xmalloc(sizeof(struct node));
    node->data.length = strlen(s) + 1;
    node->data.data = xmalloc(node->data.length);
    memcpy(node->data.data, s, node->data.length);
    list_node_push(l, node);
}

const struct datum * list_at(const struct list * l, unsigned index) {
    struct node * found = list_node_at(l, index, NULL);
    if (found == NULL) {
        exit_oob(l, index);
    }
    return &found->data;
}

const char * list_at_str(const struct list * l, unsigned index) {
    return (const char *)(list_at(l, index)->data);
}

/* extract the item from the source list and push it onto the
 * destination list */
void list_pop_push(struct list * from, struct list * to, unsigned index) {
    struct node * extracted = list_node_extract(from, index);
    if (extracted == NULL) { 
        exit_oob(from, index);
    }
    list_node_push(to, extracted);
}

/* Remove the item at 'index' from the list, free-ing its contents */
void list_pop(struct list * l, unsigned index) {
    struct node * extracted = list_node_extract(l, index);
    if (extracted == NULL) {
        exit_oob(l, index);
    }
    free(extracted->data.data);
    free(extracted);
}

/* free all elements of the list */
void list_free(struct list * l) {
    while (l->_list != NULL) {
        list_pop(l, 0);
    }
    assert(l->length == 0 && "free list has length != 0");
}
