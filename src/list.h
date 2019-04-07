#ifndef ASHUFFLE_LIST_H
#define ASHUFFLE_LIST_H

struct node;

struct datum {
    size_t length;
    void * data;
};

struct list {
    unsigned length;
    struct node * _list;
};

/* initialize the received list structure */
void list_init(struct list *);

/* Push the given datum onto the end of the given list. The given datum
 * (and its contained data) is copied and appended to the list. The given
 * datum is still owned by the caller.
 *
 * The copied datum and its contents will be free'd upon pop, or when the list
 * is free'd. If a NULL datum is given, then no data will be pushed onto
 * the list. */
void list_push(struct list *, const struct datum *);

/* Push the given null-terminated string onto the list. A new datum is
 * created automatically. */
void list_push_str(struct list *, const char *);

/* Return a pointer to the datum at 'index'. Panics if `index` is outside
 * of the range [0, list->length). */
const struct datum * list_at(const struct list *, unsigned index);

/* Same as list_at, but assumes the datum contains a null-terminated string. */
const char * list_at_str(const struct list *, unsigned index);

/* Pop item at index 'index' in list 'from' and push it onto the end of
 * list 'to'. If 'index' is out of bounds, the program will crash. */
void list_pop_push(struct list * from, struct list * to, unsigned index);

/* Remove the datum at 'index' from the list. If index is out of bounds,
 * the program will crash. */
void list_pop(struct list *, unsigned index);

/* Free all elements of the list, and their datums. */
void list_free(struct list *);

#endif 
