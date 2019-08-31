#include <stdbool.h>
#include <string.h>

#include <tap.h>

#include "list.h"

void test_basic() {
    struct list test_list;
    list_init(&test_list);
    cmp_ok(test_list.length, "==", 0, "new list length is zero");
    dies_ok({ list_pop(&test_list, 0); }, "pop on empty crashes");

    char* test_str = "This is a test string";
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
}

void test_multi_push_pop() {
    struct list test_list;
    list_init(&test_list);
    char* sa = "test str A";
    char* sb = "test str B";
    char* sc = "test str C";
    list_push_str(&test_list, sa);
    list_push_str(&test_list, sb);
    list_push_str(&test_list, sc);
    cmp_ok(test_list.length, "==", 3, "length 3 after appending 3 strings");
    is(list_at_str(&test_list, 0), sa, "list 3 has sa in 0 after push");
    is(list_at_str(&test_list, 1), sb, "list 3 has sb in 1 after push");
    is(list_at_str(&test_list, 2), sc, "list 3 has sc in 2 after push");
    list_pop(&test_list, 1);
    cmp_ok(test_list.length, "==", 2, "length 2 after pop(1)");
    is(list_at_str(&test_list, 0), sa, "list 3 has sa in 0 after push");
    is(list_at_str(&test_list, 1), sc, "list 3 has sc in 1 after push");
    dies_ok({ list_pop(&test_list, 2); }, "out of bound pop non-empty");

    list_free(&test_list);
}

void test_leak() {
    struct list t;
    list_init(&t);
    char* a = "test str A";
    char* b = "test str B";
    char* c = "test str C";
    list_push_str(&t, a);
    list_push_str(&t, b);
    list_push_str(&t, c);

    cmp_ok(t.length, "==", 3, "leak: length 3 after appending 3 strings");
    dies_ok({ list_leak(&t, 1, NULL); }, "leak: fail to leak to NULL datum");
    dies_ok(
        {
            struct datum _bad;
            list_leak(&t, 3, &_bad);
        },
        "leak: fail to leak out of bounds");
    struct datum outa;
    list_leak(&t, 1, &outa);
    is(outa.data, b, "leak: popped item 1 matches string 2");
    cmp_ok((size_t)outa.data, "!=", (size_t)a,
           "leak: popped value is owned by the caller");
    free(outa.data);

    cmp_ok(t.length, "==", 2, "leak: list length 2 after list_leak");
    is(list_at_str(&t, 0), a,
       "leak: remaing strings match original strings (a)");
    is(list_at_str(&t, 1), c,
       "leak: remaing strings match original strings (c)");
}

void test_empty() {
    struct list t;
    list_init(&t);

    struct datum d = {
        .length = 0,
        .data = NULL,
    };
    list_push(&t, &d);

    cmp_ok(t.length, "==", 1, "empty: length 1 after appending empty element");

    ok(list_at(&t, 0)->data == NULL, "empty: item 1 is NULL pointer");
    ok(list_at(&t, 0)->length == 0, "empty: item 1 is 0 length");

    list_pop(&t, 0);

    cmp_ok(t.length, "==", 0, "empty: length 0 after popping empty element");

    list_free(&t);
}

int main() {
    plan(NO_PLAN);

    test_basic();
    test_multi_push_pop();
    test_leak();
    test_empty();

    done_testing();
}
