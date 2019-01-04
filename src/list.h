#ifndef ASHUFFLE_LIST_H
#define ASHUFFLE_LIST_H

struct node;

struct list {
    unsigned length;
    struct node * list;
};

/* create a new node from the given data (can be used
 * in conjunction with list_push to add an element to
 * the list) */
struct node * node_from(const void * data, size_t size);

/* initialize the received list structure */
void list_init(struct list *);

/* Return a pointer to the data at 'index'. Returns NULL
 * if there's not data at that index */
void * list_at(const struct list *, unsigned index);

/* Pop item at index 'index' in list 'from' and push
 * it onto the end of list 'to' */
int list_pop_push(struct list * from, struct list * to, unsigned index);

/* Remove the item at 'index' from the list */
int list_pop(struct list *, unsigned index);

/* add an item to the end of the list */
void list_push(struct list *, struct node *);

/* free all elements of the list */
void list_free(struct list *);

int print_list(struct list *);

#endif 
