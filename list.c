#include <stdlib.h>
#include <string.h>
#include "list.h"

struct node {
    void * data;
    struct node * next;
};

/* create a new node from the given data (can be used
 * in conjunction with list_push to add an element to
 * the list) */
struct node * node_from(const void * data, size_t size) {
    struct node * node = malloc(sizeof(struct node));
    node->data = malloc(size);
    memcpy(node->data, data, size);
    node->next = NULL;
    return node;
}

/* initialize the received list structure */
int list_init(struct list * list) {
    list->length = 0;
    list->list = NULL;
    return 0;
}

/* get the low-level node at a given index */
struct node * list_node_at(const struct list * l, unsigned index) {
    /* if there's no data in the list, fail */
    if (l->list == NULL) { return NULL; }
    struct node * current = l->list;
    for (; index > 0; index--) {
        if (current->next == NULL) { return NULL; }
        current = current->next;
    }
    return current;
}

/* remove the current node from the list, but don't free its
 * contents. */
struct node * list_node_extract(struct list * l, unsigned index) {
    if (l->list == NULL) { return NULL; }
    struct node * current = l->list, ** previous = &l->list;
    for (; index > 0; index--) {
        if (current->next == NULL) { return NULL; }
        previous = &current->next;
        current = current->next;
    }
    /* set the previous node's 'next' value to the current
     * nodes next value */
    *previous = current->next;
    /* null out this node's next value since it's not part of
     * a list anymore */
    current->next = NULL;
    l->length--;
    return current;
}

/* Return a pointer to the data at 'index'. Returns NULL
 * if there's not data at that index */
void * list_at(const struct list * l, unsigned index) {
    struct node * found = list_node_at(l, index);
    if (found == NULL) { return NULL; }
    return found->data;
}

/* extract the item from the source list and push it onto the
 * destination list */
int list_pop_push(struct list * from, struct list * to, unsigned index) {
    struct node * extracted = list_node_extract(from, index);
    if (extracted == NULL) { return -1; }
    return list_push(to, extracted);
}

/* Remove the item at 'index' from the list, free-ing its contents */
int list_pop(struct list * l, unsigned index) {
    struct node * extracted = list_node_extract(l, index);
    if (extracted == NULL) { return -1; }
    free(extracted->data);
    free(extracted);
    return 0;
}

/* add an item to the end of the list */
int list_push(struct list * l, struct node * n) {
    /* allocate a pointer that points to the location we'll
     * eventually store our node into */
    struct node ** next = &l->list;
    while (*next != NULL) {
        next = &(*next)->next;
    }
    *next = n;
    l->length++;
    return 0;
}

/* free all elements of the list */
int list_free(struct list * l) {
    struct node * current = l->list, * tmp = NULL;
    while (current != NULL) {
        free(current->data);
        tmp = current;
        current = current->next;
        free(tmp);
    }
    list_init(l);
    return 0;
}
