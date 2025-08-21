#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#include "timing.h"

#define REPETITION_TESTER_IMPLEMENTATION
#define REPETITION_TESTER_LIVE_VIEW
#include "repetition_tester.h"

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#include "arena.h"
#include "linear_arena.h"
#include "migi.h"

// #define DYNAMIC_ARRAY_USE_ARENA
// #define DYNAMIC_ARRAY_USE_LINEAR_ARENA
#include "dynamic_array.h"
#include "migi_lists.h"
#include "migi_random.h"
#include "migi_string.h"

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

int *return_array(LinearArena *arena, size_t *size) {
    int a[] = {1, 2, 3, 4, 5, 6, 7};
    *size = array_len(a);
    return lnr_arena_memdup(arena, int, a, *size);
}

char *return_string(LinearArena *arena, size_t *size) {
    const char *s = "This is a string that will be returned from the function "
                    "by an arena.\n";
    *size = strlen(s);
    return lnr_arena_strdup(arena, s, *size);
}

void test_linear_arena_dup() {
    LinearArena arena = {0};
    size_t size = 0;
    int *a = return_array(&arena, &size);
    array_print(a, size, "%d");
    char *s = return_string(&arena, &size);
    printf("%s", s);
}

void test_linear_arena_regular(LinearArena *arena) {
    // unaligned read check
    {
        uint64_t save = lnr_arena_save(arena);
        lnr_arena_new(arena, char);
        uint64_t *u = lnr_arena_new(arena, uint64_t);
        *u = 12;
        lnr_arena_pop(arena, uint64_t, 1);
        lnr_arena_rewind(arena, save);
    }

    size_t count = getpagesize() / sizeof(int);
    int *a = lnr_arena_push(arena, int, count);
    random_array(a, int, count);

    byte *x = lnr_arena_push_bytes(arena, getpagesize(), 1);
    unused(x);
    int *c = lnr_arena_realloc(arena, int, a, count, 2 * count);

    assertf(migi_mem_eq(a, c, count), "a and c are equal upto count");
    assertf(a != c, "a and c are separate allocations!");

    assertf(arena->length == (size_t)(4 * getpagesize()),
            "4 allocations are left");
    int *b = lnr_arena_pop(arena, int, count);
    unused(b);
    // b[0] = 100; // This will segfault since the memory has been decommitted

    assertf(arena->length == (size_t)(3 * getpagesize()),
            "3 allocations are left");
    lnr_arena_free(arena);
    assertf(arena->length == arena->capacity && arena->capacity == 0,
            "0 allocations are left");
}

void test_linear_arena_rewind() {
    LinearArena arena1 = {0};
    size_t size = getpagesize() * 4;

    byte *mem = lnr_arena_push_bytes(&arena1, size, 1);
    random_bytes(mem, size);

    LinearArena arena2 = {0};
    lnr_arena_memdup_bytes(&arena2, arena1.data, arena1.length, 1);
    uint64_t checkpoint = lnr_arena_save(&arena1);
    uint64_t old_capacity = arena1.capacity;

    mem = lnr_arena_push_bytes(&arena1, size, 1);
    random_bytes(mem, size);
    lnr_arena_rewind(&arena1, checkpoint);
    assertf(old_capacity == arena1.capacity &&
                migi_mem_eq(arena1.data, arena2.data, arena1.length),
            "rewinded arena is equivalent to old one");
}

void test_linear_arena() {
#ifdef ENABLE_PROFILING
    begin_profiling();
#endif

    LinearArena arena = {0};
    LinearArena small = {.total = 16 * MB};

    test_linear_arena_regular(&arena);
    test_linear_arena_regular(&small);
    test_linear_arena_rewind();
    test_linear_arena_dup();

#ifdef ENABLE_PROFILING
    end_profiling_and_print_stats();
#endif
}

void test_arena() {
    Arena arena = {0};

    ArenaCheckpoint save = arena_save(&arena);
    {
        // unaligned read check
        arena_new(&arena, char);
        uint64_t *u = arena_new(&arena, uint64_t);
        *u = 12;
        arena_pop_current(&arena, uint64_t, 1);
        arena_rewind(&arena, save);
    }

    char *a = arena_push(&arena, char, ARENA_DEFAULT_CAP);
    a[0] = 1;

    char *b = arena_push(&arena, char, ARENA_DEFAULT_CAP);
    b[256] = 124;
    printf("%d %d\n", a[0], b[256]);

    arena_reset(&arena);
    char *c = arena_push(&arena, char, ARENA_DEFAULT_CAP * 1.25);
    c[26] = 14;
    int *d = arena_realloc(&arena, int, NULL, 0, ARENA_DEFAULT_CAP);
    d[30] = 14;
    printf("%d %d\n", c[26], d[30]);

    ArenaZone *saved_tail = arena.tail;
    size_t saved_tail_length = arena.tail->length;
    ArenaCheckpoint checkpoint = arena_save(&arena);

    int *e =
        arena_realloc(&arena, int, d, ARENA_DEFAULT_CAP, ARENA_DEFAULT_CAP * 2);
    assertf(e != d, "new zone created since size of e was greater than the "
                    "default arena capacity");

    double *f = arena_push(&arena, double, 100);
    random_array(f, double, 100);
    double *g = arena_realloc(&arena, double, f, 100, 500);
    assertf(f == g && migi_mem_eq(f, g, 100), "previous allocation was reused");

    arena_rewind(&arena, checkpoint);
    assertf(arena.tail == saved_tail && arena.tail->length == saved_tail_length,
            "rewind goes to the correct checkpoint");

    arena_free(&arena);
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

        array_foreach(&sb.arena, byte, elem) { printf("%c ", *elem); }
        printf("len: %zu\n", sb.arena.length);
    }
    printf("len: %zu\n", sb.arena.length);
}

void test_string_builder_formatted() {
    StringBuilder sb = {0};
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99),
             "what is this even doing????");
    assert(sb.arena.length == 67);
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99),
             "what is this even doing????");
    assert(sb.arena.length == 67 + 67);

    StringBuilder new_sb = {0};
    read_file(&new_sb, SV("string.h"));
    const char *str = sb_to_cstr(&new_sb);

    sb_pushf(&sb, "%s\n", str);
    printf("%s", sb_to_cstr(&sb));
    assert(sb.arena.length == 67 + 67 + new_sb.arena.length + 1);
}

void test_random() {
    size_t size = 1 * MB;
    time_t seed = time(NULL);
    byte *buf1 = malloc(size);
    byte *buf2 = malloc(size);

    migi_seed(seed);
    random_bytes(buf1, size);

    migi_seed(seed);
    random_bytes(buf2, size);

    int a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    array_shuffle(a, int, array_len(a));
    array_print(a, array_len(a), "%d");

    typedef struct {
        int a, b;
        char *foo;
    } Foo;
    Foo b[] = {
        (Foo){1, 2, "12"}, (Foo){2, 3, "23"}, (Foo){3, 4, "34"},
        (Foo){4, 5, "45"}, (Foo){5, 6, "56"},
    };
    array_shuffle(b, Foo, array_len(b));
    for (size_t i = 0; i < array_len(b); i++) {
        printf("%d %d %s\n", b[i].a, b[i].b, b[i].foo);
    }

    assertf(migi_mem_eq(buf1, buf2, size),
            "random with same seed must have same data");

    for (size_t i = 0; i < 10; i++) {
        assert(random_range_exclusive(-1, 0) != 0);
        assert(random_range_exclusive(0, 1) != 1);
    }

#define COUNT 5
    int arr[COUNT] = {0, 1, 2, 3, 4};
    int64_t weights[COUNT] = {25, 50, 75, 50, 25};
    int frequencies[COUNT] = {0};

    int sample_size = 1000000;
    int total = 0;
    for (int i = 0; i < sample_size; i++) {
        int a = random_choose_fuzzy(arr, int, weights);
        frequencies[a] += 1;
        total += 1;
    }

    for (size_t i = 0; i < COUNT; i++) {
        double percentage = ((double)frequencies[i] / (double)total) * 100.0;
        printf("[%zu] => %.2f%%\n", i, percentage);
    }
#undef COUNT
}

void test_dynamic_array() {
    Ints ints = {0};
    Ints ints_new = {0};

#if defined(DYNAMIC_ARRAY_USE_LINEAR_ARENA)
    LinearArena a = {0};
#elif defined(DYNAMIC_ARRAY_USE_ARENA)
    Arena a = {0};
#endif

#if defined(DYNAMIC_ARRAY_USE_ARENA) || defined(DYNAMIC_ARRAY_USE_LINEAR_ARENA)
    for (size_t i = 0; i < 100; i++) {
        array_add(&a, &ints, i);
    }

    array_reserve(&a, &ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_add(&a, &ints_new, 2 * i);
    }
    array_extend(&a, &ints_new, &ints);
#else
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, i);
    }

    array_reserve(&ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints_new, 2 * i);
    }
    array_extend(&ints_new, &ints);
#endif

    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);


    array_foreach(&ints_new, int, i) {
        printf("%d ", *i);
    }
}

void test_repetition_tester() {
    size_t size = 1 * MB;
    int time = 10;

    byte *buf = malloc(1 * MB);
    Tester tester = tester_init_with_name("random_bytes", time,
                                          estimate_cpu_timer_freq(), size);
    while (!tester.finished) {
        tester_begin(&tester);
        random_bytes(buf, size);
        tester_end(&tester);
    }
    tester_print_stats(&tester);
    free(buf);
}

void profile_linear_arena() {
    begin_profiling();
    test_linear_arena();
    end_profiling_and_print_stats();
}


typedef struct {
    StringSlice expected;
    StringList actual;
} StringSplitTest;

static void assert_string_split(StringSplitTest t) {
    size_t count = 0;
    size_t char_count = 0;
    if (t.actual.size != 0) {
        list_foreach(t.actual.head, StringNode, node) {
            assert(count < t.expected.length);
            assertf(string_eq(node->string, t.expected.data[count]),
                    "expected: `%.*s,` got: `%.*s`",
                    SV_FMT(t.expected.data[count]), SV_FMT(node->string));
            count++;
            char_count += node->string.length;
        }
    }
    assertf(count == t.expected.length, "expected length: %zu, actual length: %zu", t.expected.length, count);
    assert(char_count == t.actual.size);
}


void test_string_split_and_join() {
    Arena a = {0};
    StringSplitTest splits[] = {
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb") }),
            .actual = string_split(&a, SV("Mary had a little lamb"), SV(" "))
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb") }),
            .actual =  string_split_ex(&a, SV(" Mary    had   a   little   lamb "), SV(" "), Split_SkipEmpty)
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV(""), SV("Mary"), SV(""), SV(""), SV(""), SV("had"), SV(""), SV(""), 
                    SV("a"), SV(""), SV(""), SV("little"), SV(""), SV(""), SV("lamb") }),
            .actual =  string_split(&a, SV(" Mary    had   a   little   lamb"), SV(" "))
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb"), SV("") }),
            .actual = string_split(&a, SV("Mary--had--a--little--lamb--"), SV("--"))
        },
        {
            .expected = (StringSlice){0},
            .actual = string_split(&a, SV("Mary had a little lamb"), SV(""))
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV(""), SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb") }),
            .actual = string_split(&a, SV(" Mary had a little lamb"), SV(" ")),
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV(""), SV("1"), SV("") }),
            .actual = string_split(&a, SV("010"), SV("0"))
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("2020"), SV("11"), SV("03"), SV("23"), SV("59"), SV("") }),
            .actual = string_split_ex(&a, SV("2020-11-03 23:59@"), SV("- :@"), Split_AsChars)
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("2020"), SV("11"), SV("03"), SV("23"), SV("59") }),
            .actual = string_split_ex(&a, SV("2020-11--03 23:59@"), SV("- :@"), Split_SkipEmpty|Split_AsChars)
        },
        {
            .expected = migi_slice(StringSlice, (String[]){ SV("2020"), SV("11"), SV(""), SV("03"), SV("23"), SV("59"), SV("") }),
            .actual = string_split_ex(&a, SV("2020-11--03 23:59@"), SV("- :@"), Split_AsChars)
        },
    };

    for (size_t i = 0; i < array_len(splits); i++) {
        assert_string_split(splits[i]);
    }

    StringList list = string_split_ex(&a, SV("2020-11--03 23:59@"), SV("- :@"), Split_AsChars|Split_SkipEmpty);
    String expected = strlist_join(&a, &list, SV("/"));
    assert(string_eq(expected, SV("2020/11/03/23/59")));

    list = string_split(&a, SV("--foo--bar--baz--"), SV("--"));
    expected = strlist_join(&a, &list, SV("=="));
    assert(string_eq(expected, SV("==foo==bar==baz==")));
}



void linear_arena_stress_test() {
    LinearArena arenas[100] = {0};
    for (size_t i = 0; i < 100; i++) {
        lnr_arena_push_bytes(&arenas[i], 10 * MB, 1);
    }
}

void test_string_list() {
    Arena a = {0};
    StringList sl = {0};

    strlist_push_string(&a, &sl, SV("This is a "));
    strlist_push_string(&a, &sl, SV("string being built "));
    strlist_push_cstr(&a, &sl, "over time");
    strlist_push(&a, &sl, '!');

    char *s = "\nMore Stuff Here\n";
    size_t len = strlen(s);
    strlist_push_buffer(&a, &sl, s, len);
    strlist_pushf(&a, &sl,
                  "%s:%d:%s: %.15f ... and more stuff... blah blah blah",
                  __FILE__, __LINE__, __func__, M_PI);
    String final_str = strlist_to_string(&a, &sl);
    printf("%.*s", SV_FMT(final_str));
}

void profile_arenas() {
    {
        LinearArena a = {0};
        begin_profiling();
        for (int i = 0; i < 10000; i++) {
            lnr_arena_new(&a, char);
            lnr_arena_pop(&a, char, 1);
        }
        end_profiling_and_print_stats();
    }

    {
        Arena a = {0};
        begin_profiling();
        for (int i = 0; i < 10000; i++) {
            arena_new(&a, char);
            arena_pop_current(&a, char, 1);
        }
        end_profiling_and_print_stats();
    }
}

void test_string() {
    Arena a = {0};
    {
        assert(string_eq(string_to_lower(&a, SV("HELLO world!!!")),
                         SV("hello world!!!")));
        assert(string_eq(string_to_upper(&a, SV("FOO bar baz!")),
                         SV("FOO BAR BAZ!")));
    }

    {
        String str = SV("\n    hello       \n");
        assert(string_eq(string_trim_right(str),       SV("\n    hello")));
        assert(string_eq(string_trim_left(str),        SV("hello       \n")));
        assert(string_eq(string_trim(str),             SV("hello")));
        assert(string_eq(string_trim(SV("foo")),       SV("foo")));
        assert(string_eq(string_trim(SV("\t\r\nfoo")), SV("foo")));
        assert(string_eq(string_trim(SV("foo\r\n\t")), SV("foo")));
        assert(string_eq(string_trim(SV(" \r\n\t")),   SV("")));
        assert(string_eq(string_trim(SV("")),          SV("")));
    }

    // string_find
    {
        assert(string_find(SV("hello"),  SV("he"))     == 0);
        assert(string_find(SV("hello"),  SV("llo"))    == 2);
        assert(string_find(SV("hello"),  SV("o"))      == 4);
        assert(string_find(SV("banana"), SV("ana"))    == 1);
        assert(string_find(SV("abcabc"), SV("cab"))    == 2);
        assert(string_find(SV("hello"),  SV("world"))  == -1);
        assert(string_find(SV("short"),  SV("longer")) == -1);
        assert(string_find(SV("abc"),    SV("abcd"))   == -1);
        assert(string_find(SV("abc"),    SV("z"))      == -1);
        assert(string_find(SV(""),       SV(""))       == 0);
        assert(string_find(SV("abc"),    SV(""))       == 0);
        assert(string_find(SV(""),       SV("a"))      == -1);
        assert(string_find(SV("aaaaa"),  SV("aa"))     == 0);
    }

    // string_find_rev
    {
        assert(string_find_rev(SV("hello"),  SV("he"))     == 0);
        assert(string_find_rev(SV("hello"),  SV("llo"))    == 2);
        assert(string_find_rev(SV("hello"),  SV("o"))      == 4);
        assert(string_find_rev(SV("banana"), SV("ana"))    == 3);
        assert(string_find_rev(SV("abcabc"), SV("cab"))    == 2);
        assert(string_find_rev(SV("hello"),  SV("world"))  == -1);
        assert(string_find_rev(SV("short"),  SV("longer")) == -1);
        assert(string_find_rev(SV("abc"),    SV("abcd"))   == -1);
        assert(string_find_rev(SV("abc"),    SV("z"))      == -1);
        assert(string_find_rev(SV(""),       SV(""))       == 0);
        assert(string_find_rev(SV("abc"),    SV(""))       == 3);
        assert(string_find_rev(SV(""),       SV("a"))      == -1);
        assert(string_find_rev(SV("aaaaa"),  SV("aa"))     == 3);
    }

    todof("Add tests for other string functions");
}

void test_swap() {
    int a = 1, b = 2;
    migi_swap(a, b);
    assertf(b == 1 && a == 2, "swapping things work");

    typedef struct {
        int a, b;
        char c;
    } Foo;

    Foo f1 = {1, 2, 'a'}, f2 = {3, 4, 'b'};
    migi_swap(f1, f2);
    assertf(f1.a == 3 && f1.b == 4 && f1.c == 'b' && f2.a == 1 && f2.b == 2 &&
                f2.c == 'a',
            "swapping things work");
}

typedef struct {
    int *data;
    size_t length;
} IntSlice;


IntSlice return_slice(Arena *a) {
    return migi_slice_dup(a, IntSlice, (int[]){1,2,3,4,5});
}

void test_return_slice() {
    Arena a = {0};
    IntSlice slice = return_slice(&a);
    int arr[] = {1,2,3,4,5};
    assert(slice.length == array_len(arr) && migi_mem_eq(slice.data, arr, slice.length));
}

void test_string_split_first() {
    String a = SV("2020-11--03 23:59@");
    string_split_chars_foreach(a, SV("- :@"), ch) {
        printf("=> `%.*s`\n", SV_FMT(ch));
    }
    assertf(string_eq(a, SV("2020-11--03 23:59@")), "original string remains intact");

    String b = SV("a,b,c,");
    string_split_foreach(b, SV(","), it) {
        printf("=> `%.*s`\n", SV_FMT(it));
    }
    assertf(string_eq(b, SV("a,b,c,")), "original string remains intact");

    {
        String c = SV("a+-b");
        String delims = SV("-+");
        SplitIterator iter = string_split_chars_first(&c, delims);
        assert(!iter.is_over);
        assert(string_eq(iter.string, SV("a")));

        iter = string_split_chars_first(&c, delims);
        assert(!iter.is_over);
        assert(string_eq(iter.string, SV("")));

        iter = string_split_chars_first(&c, delims);
        assert(iter.is_over);
        assert(string_eq(iter.string, SV("b")));
    }
}

int main() {
<<<<<<< HEAD
    test_string_split_first();

=======
>>>>>>> hashmap-generic-key
    printf("\nExiting successfully\n");
    return 0;
}
