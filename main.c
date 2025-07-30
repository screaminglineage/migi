#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#include "timing.h"

#define REPETITION_TESTER_IMPLEMENTATION
#define REPETITION_TESTER_LIVE_VIEW
#include "repetition_tester.h"

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#include "migi_random.h"
#include "migi_string.h"
#include "migi.h"
#include "linear_arena.h"

#pragma GCC diagnostic pop

typedef struct {
    int *data;
    size_t length;
    size_t capacity;
} Ints;


bool baz_error(int x) {
    return_if_false(x != 0, printf("%s: failed\n", __func__));
    return true;
}

bool bar_error() {
    return_if_false(baz_error(0));
    return true;
}

bool foo_error() {
    return_if_false(bar_error());
    return true;
}

int test_error_propagation() {
    return_val_if_false(foo_error(), 1, printf("failed to do something\n"));
    printf("No errors!\n");
    return 0;
}

void test_linear_arena() {
    LinearArena arena = {0};

    size_t count = getpagesize()/sizeof(int);
    int *a = lnr_arena_push(&arena, int, count);
    random_array(a, int, count);

    byte *x = lnr_arena_push_bytes(&arena, getpagesize());
    (void)x;
    int *c = lnr_arena_realloc(&arena, int, a, count, 2*count);

    assertf(migi_mem_eq(a, c, count), "a and c are equal upto count");
    assertf(a != c, "a and c are separate allocations!");

    assertf(arena.length == arena.capacity && arena.capacity == (size_t)(4*getpagesize()), "4 allocations are left");
    int *b = lnr_arena_pop(&arena, int, count);
    unused(b);
    // b[0] = 100; // This will segfault since the memory has been decommitted
    assertf(arena.length == arena.capacity && arena.capacity == (size_t)(3*getpagesize()), "3 allocations are left");
    lnr_arena_free(&arena);
    assertf(arena.length == arena.capacity && arena.capacity == 0, "0 allocations are left");

    LinearArena arena1 = {0};
    size_t size = getpagesize()*4;

    byte *mem = lnr_arena_push_bytes(&arena1, size);
    random_bytes(mem, size);

    LinearArena arena2 = {0};
    lnr_arena_memdup_bytes(&arena2, arena1.data, arena1.length);
    uint64_t checkpoint = lnr_arena_save(&arena1);
    uint64_t old_capacity = arena1.capacity;

    mem = lnr_arena_push_bytes(&arena1, size);
    random_bytes(mem, size);
    lnr_arena_rewind(&arena1, checkpoint);
    assertf(old_capacity == arena1.capacity && migi_mem_eq(arena1.data, arena2.data, arena1.length), "rewinded arena is equivalent to old one");
}

int *return_array(LinearArena *arena, size_t *size) {
    int a[] = {1,2,3,4,5,6,7};
    *size = array_len(a);
    return lnr_arena_memdup(arena, int, a, *size);
}

char *return_string(LinearArena *arena, size_t *size) {
    const char *s = "This is a string that will be returned from the function by an arena.\n";
    *size = strlen(s);
    return lnr_arena_strdup(arena, s, *size);
}

void test_dup() {
    LinearArena arena = {0};
    size_t size = 0;
    int *a = return_array(&arena, &size);
    array_print(a, size, "%d");
    char *s = return_string(&arena, &size);
    printf("%s", s);
}

void test_string_builder() {
    StringBuilder sb = {0};
    assert(read_file(&sb, SV("main.c")));
    assert(read_file(&sb, SV("string.h")));
    assert(write_file(&sb, SV("main-string.c")));

    sb.arena.length = 0;
    defer_block(sb.arena.length = 0) {
        sb_push_string(&sb, SV("hello"));
        sb_push_string(&sb, SV("foo"));
        sb_push_string(&sb, SV("bar"));
        sb_push_string(&sb, SV("baz"));

        array_foreach(&sb.arena, byte, elem) {
            printf("%c ", *elem);
        }
        printf("len: %zu\n", sb.arena.length);
    }
    printf("len: %zu\n", sb.arena.length);
}

void test_string_builder_formatted() {
    StringBuilder sb = {0};
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99), "what is this even doing????");
    assert(sb.arena.length == 67);
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99), "what is this even doing????");
    assert(sb.arena.length == 67 + 67);

    StringBuilder new_sb = {0};
    read_file(&new_sb, SV("string.h"));
    const char *str = sb_to_cstr(&new_sb);

    sb_pushf(&sb, "%s\n", str);
    printf("%s", sb_to_cstr(&sb));
    assert(sb.arena.length == 67 + 67 + new_sb.arena.length + 1);
}

void test_random() {
    size_t size = 1*MB;
    time_t seed = time(NULL);
    byte *buf1 = malloc(size);
    byte *buf2 = malloc(size);

    migi_seed(seed);
    random_bytes(buf1, size);

    migi_seed(seed);
    random_bytes(buf2, size);

    int a[] = {1,2,3,4,5,6,7,8,9,0};
    array_shuffle(a, int, array_len(a));
    array_print(a, array_len(a), "%d");

    typedef struct {
        int a, b;
        char *foo;
    } Foo;
    Foo b[] = {
        (Foo){1, 2, "12"},
        (Foo){2, 3, "23"},
        (Foo){3, 4, "34"},
        (Foo){4, 5, "45"},
        (Foo){5, 6, "56"},
    };
    array_shuffle(b, Foo, array_len(b));
    for (size_t i = 0; i < array_len(b); i++) {
        printf("%d %d %s\n", b[i].a, b[i].b, b[i].foo);
    }

    assertf(migi_mem_eq(buf1, buf2, size), "random with same seed must have same data");
}

void test_dynamic_array() {
    Ints ints = {0};
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, i);
    }

    Ints ints_new = {0};
    array_reserve(&ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, 2*i);
    }

    array_extend(&ints_new, &ints);
    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);

    for (size_t i = 0; i < ints_new.length; i++) {
        printf("%d ", ints_new.data[i]);
    }
}

void test_repetition_tester() {
    size_t size = 1*MB;
    int time = 10;

    byte *buf = malloc(1*MB);
    Tester tester = tester_init_with_name("random_bytes", time, estimate_cpu_timer_freq(), size);
    while (!tester.finished) {
        tester_begin(&tester);
        random_bytes(buf, size);
        tester_end(&tester);
    }
    tester_print_stats(&tester);
    free(buf);
}

void profile_linear_array() {
    begin_profiling();
    test_linear_arena();
    end_profiling_and_print_stats();
}

void test_string_split_print(Strings s) {
    printf("Length = %zu\n", s.length);
    array_foreach(&s, String, str) {
        printf("`%.*s` ", SV_FMT(*str));
    }
    printf("\n");
}

void test_string_split() {
    Strings s1[] = {
        string_split(SV("Mary had a little lamb"), SV(" ")),
        string_split_ex(SV(" Mary    had   a   little   lamb "), SV(" "), SPLIT_SKIP_EMPTY),
        string_split(SV(" Mary    had   a   little   lamb"), SV(" ")),
        string_split(SV("Mary--had--a--little--lamb--"), SV("--")),
        string_split(SV("Mary had a little lamb"), SV("")),
        string_split(SV(" Mary had a little lamb"), SV(" ")),
        string_split(SV("010"), SV("0")),
    };

    for (size_t i = 0; i < array_len(s1); i++) {
        test_string_split_print(s1[i]);
    }

    char delims[] = {'-', ' ', ':', '@'};
    Strings s2 = string_split_chars(SV("2020-11-03 23:59@"), delims, array_len(delims));
    test_string_split_print(s2);
    s2 = string_split_chars_ex(SV("2020-11-03 23:59@"), delims, array_len(delims), SPLIT_SKIP_EMPTY);
    test_string_split_print(s2);
}

void linear_arena_stress_test() {
    LinearArena arenas[100] = {0};
    for (size_t i = 0; i < 100; i++) {
        arenas[i] = (LinearArena){0};
        lnr_arena_push_bytes(&arenas[i], 1*GB);
    }
}

int main() {
    printf("\nExiting successfully\n");
    return 0;
}
