#include "migi.h"

// #define DYNAMIC_ARRAY_USE_ARENA
#include "dynamic_array.h"

void test_dynamic_array() {
    Array(int) ints = {0};
    Array(int) ints_new = {0};

    for (size_t i = 0; i < 100; i++) {
        array_push(&ints, i);
    }

    array_reserve(&ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_push(&ints_new, 2 * i);
    }
    array_extend(&ints_new, &ints);

    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);

    assert(ints.length == 100);
    assert(ints_new.length == 199);
    assert(ints_new.data[50] == 99);


    array_foreach(&ints_new, i) {
        printf("%d ", *i);
    }

    array_free(&ints);
    array_free(&ints_new);
}

void test_dynamic_array_arena() {
    // These tests make no sense if not using an arena
#ifdef DYNAMIC_ARRAY_USE_ARENA
    {
        Array(int) ints1 = {0};

        array_push(&ints1, 1);
        int *old_ints1_data = ints1.data;

        for (int i = 1; i < 500; i++) {
            array_push(&ints1, i);
        }
        assertf(old_ints1_data == ints1.data, "since a linear arena is used and no other allocations take place, "
                                              "the dynamic array contents are never moved in this case");

        Array(int) ints2 = {0};
        array_push(&ints2, 0);
        int *old_ints2_data = ints2.data;

        for (int i = 1; i < 500; i++) {
            array_push(&ints2, 10 * i);
        }

        array_extend(&ints2, &ints1);
        assertf(old_ints2_data == ints2.data, "since a linear arena is used and no other allocations take place, "
                                              "the dynamic array contents are never moved in this case");

        array_free(&ints1);
        array_free(&ints2);
    }

    {
        // All the allocations are done on the custom arena passed in
        // This is great for both maintaining the lifetimes as well as
        // cache locality
        Temp tmp = arena_temp();
        Array(Str) a = {.arena = tmp.arena};

        array_push(&a, S("some"));
        array_push(&a, S("interesting"));
        array_push(&a, S("stuff"));
        array_push(&a, strf(tmp.arena, "pretty f%c%cking cool!", 'u', 'c'));

        assertf((byte *)a.data         < tmp.arena->data + tmp.arena->position, "all allocations live on the same arena");
        assertf((byte *)a.data[3].data < tmp.arena->data + tmp.arena->position, "all allocations live on the same arena");

        arena_temp_release(tmp);
    }


#endif // ifdef DYNAMIC_ARRAY_USE_ARENA
}

int main() {
    test_dynamic_array();
    test_dynamic_array_arena();
    return 0;
}
