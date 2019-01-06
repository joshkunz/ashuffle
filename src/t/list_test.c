#include <stdbool.h>
#include <string.h>

#include <tap.h>

#include "list.h"

int main() {
    plan(NO_PLAN);

    struct list test_list;
    list_init(&test_list);
    cmp_ok(test_list.length, "==", 0, "new list length is zero");
    dies_ok({ list_pop(&test_list, 0); }, "pop on empty crashes");

    char * test_str = "This is a test string";
    list_push_str(&test_list, test_str);
    cmp_ok(test_list.length, "==", 1, "list length 1 after append");
    is(list_at_str(&test_list, 0), test_str,
       "data at index 0 matches apended data");

    struct list test_list2;
    list_init(&test_list2);
    list_pop_push(&test_list, &test_list2, 0);
    cmp_ok(test_list.length, "==", 0, "original list empty after pop_push");
    cmp_ok(test_list2.length, "==", 1, "dest list has an item after pop_push");
    is(list_at_str(&test_list2, 0), test_str,
       "list 2 has test_str in 0 after pop_push");

    list_free(&test_list);
    list_free(&test_list2);
    cmp_ok(test_list2.length, "==", 0, "list empty after free");

    struct list test_list3;
    list_init(&test_list3);
    char * sa = "test str A"; char * sb = "test str B"; char * sc = "test str C";
    list_push_str(&test_list3, sa);
    list_push_str(&test_list3, sb);
    list_push_str(&test_list3, sc);
    cmp_ok(test_list3.length, "==", 3, "length 3 after appending 3 strings");
    is(list_at_str(&test_list3, 0), sa,
       "list 3 has sa in 0 after push");
    is(list_at_str(&test_list3, 1), sb,
       "list 3 has sb in 1 after push");
    is(list_at_str(&test_list3, 2), sc,
       "list 3 has sc in 2 after push");
    list_pop(&test_list3, 1);
    cmp_ok(test_list3.length, "==", 2, "length 2 after pop(1)");
    is(list_at_str(&test_list3, 0), sa,
       "list 3 has sa in 0 after push");
    is(list_at_str(&test_list3, 1), sc,
       "list 3 has sc in 1 after push");
    dies_ok({ list_pop(&test_list3, 2); }, "out of bound pop non-empty");

    list_free(&test_list3);

    done_testing();
}
