#include "migi.h"
#include "random.h"
#include "search.h"

void test_search() {
    Temp tmp = arena_temp();
    {
        int s = 10;
        int *arr = arena_push(tmp.arena, int, s);
        for (int i = 0; i < s; i++) {
            arr[i] = i;
        }
        for (int i = 0; i <= s; i++) {
            size_t n = search(arr, s, i);
            assert((int)n == i);
        }
    }

    {
        Str arr[] = { S("bar"), S("baz"), S("foo"), S("hello"), S("world") };
        for (size_t i = 0; i < array_len(arr); i++) {
            size_t n = search(arr, array_len(arr), arr[i]);
            assert(n == i);
        }
        assert(search(arr, array_len(arr), S("different string")) == array_len(arr));
    }

    {
        typedef struct {
            int num1, num2;
            char ch;
        } Foo;

        int s = 10;
        Foo *arr = arena_push(tmp.arena, Foo, s);
        for (int i = 0; i < s; i++) {
            arr[i] = (Foo){
                .num1 = rand_range(0, 10),
                .num2 = i,
                .ch = rand_range('a', 'z'),
            };
        }

        for (int i = 0; i < s; i++) {
            size_t n = search_key(arr, s, num2, i);
            assert((int)n == i);
        }
    }


    arena_temp_release(tmp);
}

void test_binary_search() {
    Temp tmp = arena_temp();
    {
        int s = 10;
        int *arr = arena_push(tmp.arena, int, s);
        for (int i = 0; i < s; i++) {
            arr[i] = i;
        }
        for (int i = 0; i <= s; i++) {
            size_t n = binary_search(arr, s, i);
            assert((int)n == i);
        }
    }

    {
        Str arr[] = { S("bar"), S("baz"), S("foo"), S("hello"), S("world") };
        for (size_t i = 0; i < array_len(arr); i++) {
            size_t n = binary_search(arr, array_len(arr), arr[i]);
            assert(n == i);
        }
        assert(binary_search(arr, array_len(arr), S("different string")) == array_len(arr));
    }

    {
        typedef struct {
            int num1, num2;
            char ch;
        } Foo;

        int s = 10;
        Foo *arr = arena_push(tmp.arena, Foo, s);
        for (int i = 0; i < s; i++) {
            arr[i] = (Foo){
                .num1 = rand_range(0, 10),
                .num2 = i,
                .ch = rand_range('a', 'z'),
            };
        }

        for (int i = 0; i < s; i++) {
            size_t n = binary_search_key(arr, s, num2, i);
            assert((int)n == i);
        }
    }


    arena_temp_release(tmp);
}

int main() {
    test_search();
    test_binary_search();
    printf("\nExiting Successfully\n");
    return 0;
}
