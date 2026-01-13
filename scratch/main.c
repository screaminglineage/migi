#define _CRT_SECURE_NO_WARNINGS

// MSVC needs this macro to define math constants (M_PI, etc.)
#ifdef _MSC_VER
    #define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "migi_memory.h"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
#endif // ifdef _GNU_C

#include "timing.h"

#define REPETITION_TESTER_IMPLEMENTATION
#define REPETITION_TESTER_LIVE_VIEW
#include "repetition_tester.h"

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#include "arena.h"
#include "migi.h"

// #define DYNAMIC_ARRAY_USE_ARENA
#include "dynamic_array.h"
#include "migi_lists.h"
#include "migi_random.h"
#include "migi_string.h"
#include "dynamic_string.h"
#include "string_builder.h"

#define POOL_ALLOC_COUNT_ALLOCATIONS
#include "pool_allocator.h"

#include "migi_temp.h"
#include "dynamic_deque.h"

#include "smol_map.h"

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif


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

int *return_array(Arena *arena, size_t *size) {
    int a[] = {1, 2, 3, 4, 5, 6, 7};
    *size = array_len(a);
    return arena_copy(arena, int, a, *size);
}

char *return_string(Arena *arena, size_t *size) {
    const char *s = "This is a string that will be returned from the function "
                    "by an arena.\n";
    *size = strlen(s);
    return arena_copy(arena, char, s, *size);
}

void test_linear_arena_dup() {
    Arena *arena = arena_init();
    size_t size = 0;
    int *a = return_array(arena, &size);
    array_print(a, size, "%d");
    char *s = return_string(arena, &size);
    printf("%s", s);
}

void test_linear_arena_regular(Arena *arena) {
    // unaligned read check
    {
        Checkpoint save = arena_save(arena);
        arena_push(arena, char, 1);
        *arena_new(arena, uint64_t) = 12;
        arena_pop(arena, uint64_t, 1);
        arena_rewind(save);
    }

    size_t count = memory_page_size() / sizeof(int);
    int *a = arena_push(arena, int, count);
    random_array(a, int, count);

    byte *x = arena_push_bytes(arena, memory_page_size(), 1, true);
    unused(x);
    int *c = arena_realloc(arena, int, a, count, 2 * count);

    assertf(mem_eq_array(a, c, count), "a and c are equal upto count");
    assertf(a != c, "a and c are separate allocations!");

    assertf(arena->current->position == sizeof(Arena) + (size_t)(4 * memory_page_size()),
            "4 allocations are left");
    arena_pop(arena, int, count);

    assertf(arena->current->position == sizeof(Arena) + (size_t)(3 * memory_page_size()),
            "3 allocations are left");
    arena_free(arena);
}

void test_linear_arena_rewind() {
    Arena *arena1 = arena_init(.type = Arena_Linear);
    size_t size = memory_page_size() * 4;

    byte *mem = arena_push_bytes(arena1, size, 1, false);
    random_bytes(mem, size);

    Arena *arena2 = arena_init(.type = Arena_Linear);
    arena_copy_bytes(arena2, arena1->current->data, arena1->current->position - sizeof(Arena), 1);
    Checkpoint checkpoint = arena_save(arena1);
    uint64_t old_capacity = arena1->current->reserved;

    mem = arena_push_bytes(arena1, size, 1, true);
    random_bytes(mem, size);
    arena_rewind(checkpoint);
    assertf(old_capacity == arena1->current->reserved &&
                mem_eq_array(arena1->current->data, arena2->current->data, arena1->current->position - sizeof(Arena)),
            "rewinded arena is equivalent to old one");
}

void test_linear_arena() {
#ifdef ENABLE_PROFILING
    begin_profiling();
#endif

    Arena *arena = arena_init(.type = Arena_Linear);

#define BUF_SIZE 16*MB
    static byte buf[BUF_SIZE];

// FUCK YOU MICROSOFT
// More Info: https://stackoverflow.com/questions/27793470/why-does-small-give-an-error-about-char
// (TODO: fix this by not including whatever fucking header it auto-includes by default)
#undef small

    Arena *small = arena_init_static(buf, BUF_SIZE);
#undef BUF_SIZE

    test_linear_arena_regular(arena);
    test_linear_arena_regular(small);
    test_linear_arena_rewind();
    test_linear_arena_dup();

#ifdef ENABLE_PROFILING
    end_profiling_and_print_stats();
#endif
}

void test_chained_arena() {
    size_t reserved = 16*KB;
    Arena *arena = arena_init(.type = Arena_Chained, .reserve_size = reserved);

    Checkpoint save = arena_save(arena);
    {
        // unaligned read check
        arena_push(arena, char, 1);
        *arena_new(arena, uint64_t) = 12;
        arena_pop(arena, uint64_t, 1);
        arena_rewind(save);
    }

    char *a = arena_push(arena, char, reserved);
    a[0] = 1;

    char *b = arena_push(arena, char, reserved);
    b[256] = 124;
    printf("%d %d\n", a[0], b[256]);

    arena_reset(arena);
    char *c = arena_push(arena, char, (size_t)(reserved * 1.25));
    c[26] = 14;
    int *d = arena_realloc(arena, int, NULL, 0, reserved);
    d[30] = 14;
    printf("%d %d\n", c[26], d[30]);

    Arena *saved_tail = arena->current;
    size_t saved_tail_length = arena->current->position;
    Checkpoint checkpoint = arena_save(arena);

    int *e =
        arena_realloc(arena, int, d, reserved, reserved * 2);
    assertf(e != d, "new zone created since size of e was greater than the "
                    "default arena capacity");

    double *f = arena_push(arena, double, 100);
    random_array(f, double, 100);
    double *g = arena_realloc(arena, double, f, 100, 500);
    assertf(f == g && mem_eq_array(f, g, 100), "previous allocation was reused");

    arena_rewind(checkpoint);
    assertf(arena->current == saved_tail && arena->current->position == saved_tail_length,
            "rewind goes to the correct checkpoint");

    arena_free(arena);
}

void test_string_builder() {
    StringBuilder sb = sb_init();
    defer_block(sb_reset(&sb)) {
        sb_push_string(&sb, SV("hello"));
        sb_push_string(&sb, SV("foo"));
        sb_push_string(&sb, SV("bar"));
        sb_push_string(&sb, SV("baz"));

        printf("%s\n", sb_to_cstr(&sb));
        printf("len: %zu\n", sb_length(&sb));
    }
    printf("len: %zu\n", sb_length(&sb));
}

void test_string_builder_formatted() {
    StringBuilder sb = sb_init();
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99),
             "what is this even doing????");
    assert(sb_length(&sb) == 67);
    sb_pushf(&sb, "Hello world, %d, %.10f - %s\n\n", -3723473, sin(25.6212e99),
             "what is this even doing????");
    assert(sb_length(&sb) == 67 + 67);

    {
        StringBuilder sb1 = sb_init();
        sb_push_string(&sb1, SV("foo"));
        sb_push_string(&sb1, SV("bar"));
        sb_push_string(&sb1, SV("baz"));
        sb_pushf(&sb1, "\nhello world! %d, %.*s, %f\n", 12, SV_FMT(SV("more stuff")), 3.14);
        sb_pushf(&sb1, "abcd efgh 12345678 %x\n", 0xdeadbeef);

        String str = sb_to_string(&sb1);
        printf("%.*s\n", SV_FMT(str));
        sb_free(&sb1);
    }

    static char buf[1*MB];
    Arena *a = arena_init_static(buf, sizeof(buf));
    String str = read_file(a, SV("./src/string_builder.h")).string;
    const char *cstr = string_to_cstr(a, str);

    sb_pushf(&sb, "%s\n", cstr);
    printf("%s", sb_to_cstr(&sb));
    assert(sb_length(&sb) == 67 + 67 + str.length + 1);

    {

        char buffer[2048] = {0};
        StringBuilder sb_static = sb_init_static(buffer, sizeof(buffer));
        sb_pushf(&sb_static, "%.*s/%s:%d\n", SV_FMT(SV("FILE PATH")), __FILE__, __LINE__);
        printf("%.*s", SV_FMT(sb_to_string(&sb_static)));
    }
}

void test_random() {
    size_t size = 1 * MB;
    Arena *arena = arena_init();
    time_t seed = time(NULL);
    byte *buf1 = arena_push_nonzero(arena, byte, size);
    byte *buf2 = arena_push_nonzero(arena, byte, size);

    migi_seed(seed);
    random_bytes(buf1, size);

    migi_seed(seed);
    random_bytes(buf2, size);

    int a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    array_shuffle(arena, a, int, array_len(a));
    array_print(a, array_len(a), "%d");

    typedef struct {
        int a, b;
        char *foo;
    } Foo;
    Foo b[] = {
        (Foo){1, 2, "12"}, (Foo){2, 3, "23"}, (Foo){3, 4, "34"},
        (Foo){4, 5, "45"}, (Foo){5, 6, "56"},
    };
    array_shuffle(arena, b, Foo, array_len(b));
    for (size_t i = 0; i < array_len(b); i++) {
        printf("%d %d %s\n", b[i].a, b[i].b, b[i].foo);
    }

    assertf(mem_eq_array(buf1, buf2, size),
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
        int chosen = random_choose_fuzzy(arena, arr, int, weights);
        frequencies[chosen] += 1;
        total += 1;
    }

    for (size_t i = 0; i < COUNT; i++) {
        double percentage = ((double)frequencies[i] / (double)total) * 100.0;
        printf("[%zu] => %.2f%%\n", i, percentage);
    }
#undef COUNT
}

void test_dynamic_array() {
    typedef struct {
        int *data;
        size_t length;
        size_t capacity;
    } Ints;

    Ints ints = {0};
    Ints ints_new = {0};

#ifdef DYNAMIC_ARRAY_USE_ARENA
    Arena *a = arena_init();
#endif

#ifdef DYNAMIC_ARRAY_USE_ARENA
    for (size_t i = 0; i < 100; i++) {
        array_push(a, &ints, i);
    }

    array_reserve(a, &ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_push(a, &ints_new, 2 * i);
    }
    array_extend(a, &ints_new, &ints);
#else
    for (int i = 0; i < 100; i++) {
        array_push(&ints, i);
    }

    array_reserve(&ints_new, 100);
    for (int i = 0; i < 100; i++) {
        array_push(&ints_new, 2 * i);
    }
    array_extend(&ints_new, &ints);
#endif

    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);


    array_foreach(&ints_new, int, i) {
        printf("%d ", *i);
    }
#ifdef DYNAMIC_ARRAY_USE_ARENA
    arena_free(a);
#else
    free(ints.data);
    free(ints_new.data);
#endif
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
    if (t.actual.total_size != 0) {
        list_foreach(t.actual.head, StringNode, node) {
            // strlist stores in reverse order
            String expected = t.expected.data[t.expected.length - count - 1];
            String actual = node->string;

            assert(count < t.expected.length);
            assertf(string_eq(actual, expected), "expected: `%.*s,` got: `%.*s`",
                    SV_FMT(expected), SV_FMT(actual));
            count++;
            char_count += actual.length;
        }
    }
    assertf(count == t.expected.length, "expected length: %zu, actual length: %zu", t.expected.length, count);
    assert(char_count == t.actual.total_size);
}


void test_string_split_and_join() {
    Arena *a = arena_init();
    StringSplitTest splits[] = {
        {
            .expected = slice_from(String, StringSlice, SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb")),
            .actual = string_split(a, SV("Mary had a little lamb"), SV(" "))
        },
        {
            .expected = slice_from(String, StringSlice, SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb")),
            .actual =  string_split_ex(a, SV(" Mary    had   a   little   lamb "), SV(" "), Split_SkipEmpty)
        },
        {
            .expected = slice_from(String, StringSlice, SV(""), SV("Mary"), SV(""), SV(""), SV(""), SV("had"), SV(""), SV(""), 
                    SV("a"), SV(""), SV(""), SV("little"), SV(""), SV(""), SV("lamb")),
            .actual =  string_split(a, SV(" Mary    had   a   little   lamb"), SV(" "))
        },
        {
            .expected = slice_from(String, StringSlice, SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb"), SV("")),
            .actual = string_split(a, SV("Mary--had--a--little--lamb--"), SV("--"))
        },
        {
            .expected = (StringSlice){0},
            .actual = string_split(a, SV("Mary had a little lamb"), SV(""))
        },
        {
            .expected = slice_from(String, StringSlice, SV(""), SV("Mary"), SV("had"), SV("a"), SV("little"), SV("lamb")),
            .actual = string_split(a, SV(" Mary had a little lamb"), SV(" ")),
        },
        {
            .expected = slice_from(String, StringSlice, SV(""), SV("1"), SV("")),
            .actual = string_split(a, SV("010"), SV("0"))
        },
        {
            .expected = slice_from(String, StringSlice, SV("2020"), SV("11"), SV("03"), SV("23"), SV("59"), SV("")),
            .actual = string_split_ex(a, SV("2020-11-03 23:59@"), SV("- :@"), Split_AsChars)
        },
        {
            .expected = slice_from(String, StringSlice, SV("2020"), SV("11"), SV("03"), SV("23"), SV("59")),
            .actual = string_split_ex(a, SV("2020-11--03 23:59@"), SV("- :@"), Split_SkipEmpty|Split_AsChars)
        },
        {
            .expected = slice_from(String, StringSlice, SV("2020"), SV("11"), SV(""), SV("03"), SV("23"), SV("59"), SV("")),
            .actual = string_split_ex(a, SV("2020-11--03 23:59@"), SV("- :@"), Split_AsChars)
        },
    };

    for (size_t i = 0; i < array_len(splits); i++) {
        assert_string_split(splits[i]);
    }

    StringList list = string_split_ex(a, SV("2020-11--03 23:59@"), SV("- :@"), Split_AsChars|Split_SkipEmpty);
    String expected = strlist_join(a, &list, SV("-"));
    assert(string_eq(expected, SV("2020-11-03-23-59")));

    list = string_split(a, SV("--foo--bar--baz--"), SV("--"));
    expected = strlist_join(a, &list, SV("=="));
    assert(string_eq(expected, SV("==foo==bar==baz==")));

    expected = strlist_join(a, &list, SV(""));
    assert(string_eq(expected, SV("foobarbaz")));
}



void linear_arena_stress_test() {
    Arena *arenas[100] = {0};
    for (size_t i = 0; i < 100; i++) {
        arenas[i] = arena_init();
    }
    for (size_t i = 0; i < 100; i++) {
        arena_push_bytes(arenas[i], 10 * MB, 1, true);
    }
}

void test_string_list() {
    test_string_split_and_join();

    Arena *a = arena_init();
    StringList sl = {0};

    strlist_push_string(a, &sl, SV("This is a "));
    strlist_push_string(a, &sl, SV("string being built "));
    strlist_push_cstr(a, &sl, "over time");
    strlist_push(a, &sl, '!');

    char *s = "\nMore Stuff Here\n";
    size_t len = strlen(s);
    strlist_push_buffer(a, &sl, s, len);
    strlist_pushf(a, &sl,
                  "%s:%d:%s: %.15f ... and more stuff... blah blah blah",
                  __FILE__, __LINE__, __func__, M_PI);
    String final_str = strlist_to_string(a, &sl);
    printf("%.*s", SV_FMT(final_str));
}

void profile_arenas() {
    {
        Arena *a = arena_init(.type = Arena_Linear);
        begin_profiling();
        for (int i = 0; i < 10000; i++) {
            arena_push(a, char, 1);
            arena_pop(a, char, 1);
        }
        end_profiling_and_print_stats();
    }

    {
        Arena *a = arena_init(.type = Arena_Chained);
        begin_profiling();
        for (int i = 0; i < 10000; i++) {
            arena_push(a, char, 1);
            arena_pop(a, char, 1);
        }
        end_profiling_and_print_stats();
    }
}

bool skip_nums(char ch, void *data) {
    unused(data);
    return between(ch, '0', '9');
}

void test_string() {
    Arena *a = arena_init();
    {
        assert(string_eq(string_to_lower(a, SV("HELLO world!!!")), SV("hello world!!!")));
        assert(string_eq(string_to_upper(a, SV("FOO bar baz!")),   SV("FOO BAR BAZ!")));
    }

    // string_skip_while
    {
        assert(string_eq(string_skip_while(    SV("1234abcd"),  skip_nums, NULL), SV("abcd")));
        assert(string_eq(string_skip_while_rev(SV("1234abcd"),  skip_nums, NULL), SV("1234abcd")));
        assert(string_eq(string_skip_while_rev(SV("foo90"),     skip_nums, NULL), SV("foo")));
        assert(string_eq(string_skip_while(    SV("foo90"),     skip_nums, NULL), SV("foo90")));
        assert(string_eq(string_skip_while(    SV(""),          skip_nums, NULL), SV("")));

        assert(string_eq(string_skip_chars(    SV("abcd"),      SV("abd")),       SV("cd")));
        assert(string_eq(string_skip_chars_rev(SV("abcd"),      SV("da")),        SV("abc")));
    }

    // string_trim
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

    // string_reverse
    {
        assert(string_eq(string_reverse(a, SV("")), SV("")));
        assert(string_eq(string_reverse(a, SV("hello world")), SV("dlrow olleh")));
    }

    // string_replace
    {
        assert(string_eq(string_replace(a, SV(""),              SV(""),      SV("")),     SV("")));
        assert(string_eq(string_replace(a, SV("foo"),           SV(""),      SV("bar")),  SV("foo")));
        assert(string_eq(string_replace(a, SV("foo"),           SV("bar"),   SV("")),     SV("foo")));
        assert(string_eq(string_replace(a, SV("foo"),           SV("foo"),   SV("")),     SV("")));
        assert(string_eq(string_replace(a, SV("hello world!!"), SV("ll"),    SV("yy")),   SV("heyyo world!!")));
        assert(string_eq(string_replace(a, SV("aaa"),           SV("a"),     SV("bar")),  SV("barbarbar")));
        assert(string_eq(string_replace(a, SV("hello world"),   SV("l"),     SV("x")),    SV("hexxo worxd")));
        assert(string_eq(string_replace(a, SV("start starry starred restart started"),
                                                                 SV("start"), SV("part")),
                                                                                           SV("part starry starred repart parted")));
    }

    // string_cut
    {
        StringCut cut = {0};

        cut = string_cut(SV("hello world"), SV(" "));
        assert(cut.valid == true
           && string_eq(cut.head, SV("hello"))
           && string_eq(cut.tail, SV("world")));

        cut = string_cut(SV("hello==++==world"), SV("==++=="));
        assert(cut.valid == true
           && string_eq(cut.head, SV("hello"))
           && string_eq(cut.tail, SV("world")));

        cut = string_cut(SV("world"), SV("world"));
        assert(cut.valid == true
           && string_eq(cut.head, SV(""))
           && string_eq(cut.tail, SV("")));

        cut = string_cut(SV("world"), SV(""));
        assert(cut.valid == true
           && string_eq(cut.head, SV(""))
           && string_eq(cut.tail, SV("world")));

        cut = string_cut(SV(""), SV(""));
        assert(cut.valid == true
           && string_eq(cut.head, SV(""))
           && string_eq(cut.tail, SV("")));

        cut = string_cut(SV("hello"), SV("llo"));
        assert(cut.valid == true
           && string_eq(cut.head, SV("he"))
           && string_eq(cut.tail, SV("")));

        cut = string_cut(SV("abcd"), SV("e"));
        assert(cut.valid == false);
    }

    todof("Add tests for other string functions");
}

void test_swap() {
    int a = 1, b = 2;
    mem_swap(int, a, b);
    assertf(b == 1 && a == 2, "swapping things work");

    typedef struct {
        int a, b;
        char c;
    } Foo;

    Foo f1 = {1, 2, 'a'}, f2 = {3, 4, 'b'};
    mem_swap(Foo, f1, f2);
    assertf(f1.a == 3 && f1.b == 4 && f1.c == 'b' && f2.a == 1 && f2.b == 2 &&
                f2.c == 'a',
            "swapping things work");
}

typedef struct {
    int *data;
    size_t length;
} IntSlice;


IntSlice return_slice(Arena *a) {
    return slice_new(a, int, IntSlice, 1,2,3,4,5);
}

void test_return_slice() {
    Arena *a = arena_init();
    IntSlice slice = return_slice(a);
    int arr[] = {1,2,3,4,5};
    assert(slice.length == array_len(arr) && mem_eq_array(slice.data, arr, slice.length));
}

void test_string_split_first() {
    String a = SV("2020-11--03 23:59");
    string_split_chars_foreach(a, SV("- :"), it) {
        printf("=> `%.*s`\n", SV_FMT(it.split));
    }
    assertf(string_eq(a, SV("2020-11--03 23:59")), "original string remains intact");

    String b = SV("a,b,c,");
    string_split_foreach(b, SV(","), it) {
        printf("=> `%.*s`\n", SV_FMT(it.split));
    }
    assertf(string_eq(b, SV("a,b,c,")), "original string remains intact");

    {
        String c = SV("a+-b");
        String delims = SV("-+");
        SplitIterator iter = string_split_chars_next(&c, delims);
        assert(!iter.is_over);
        assert(string_eq(iter.string, SV("a")));

        iter = string_split_chars_next(&c, delims);
        assert(!iter.is_over);
        assert(string_eq(iter.string, SV("")));

        iter = string_split_chars_next(&c, delims);
        assert(iter.is_over);
        assert(string_eq(iter.string, SV("b")));
    }
}


typedef struct {
    int foo[512];
    float bar[512];
    char baz[512];
} LargeStruct;

typedef PoolAllocator(LargeStruct) StructPool;

void test_pool_allocator_impl(StructPool *p) {
    LargeStruct *allocs[10] = {0};
    for (size_t i = 0; i < array_len(allocs); i++) {
        allocs[i] = pool_alloc(p);
        random_bytes(&allocs[i]->foo, sizeof(allocs[i]->foo));
        random_bytes(&allocs[i]->bar, sizeof(allocs[i]->bar));
        random_bytes(&allocs[i]->baz, sizeof(allocs[i]->baz));
    }
    assert(p->p.length == 10);

    pool_free(p, allocs[1]);
    pool_free(p, allocs[9]);
    pool_free(p, allocs[4]);
    pool_free(p, allocs[0]);
    assert(p->p.length == 6);


    LargeStruct *a1 = pool_alloc(p);
    LargeStruct *a2 = pool_alloc(p);
    LargeStruct *a3 = pool_alloc(p);
    LargeStruct *a4 = pool_alloc(p);

    assert(a4 == allocs[1]);
    assert(a3 == allocs[9]);
    assert(a2 == allocs[4]);
    assert(a1 == allocs[0]);
    assert(p->p.length == 10);
}

void test_pool_allocator() {
    StructPool p = {0};
    test_pool_allocator_impl(&p);
    assert(p.p.length == 10);
    pool_reset(&p.p);
    assert(p.p.length == 0 && p.p.free_list == NULL && p.p.arena->current->position == sizeof(Arena));

    LargeStruct *s1 = pool_alloc(&p);
    random_bytes(s1, sizeof(*s1));
    assert(p.p.length == 1);

    pool_free(&p, s1);
    assert(p.p.length == 0);

    LargeStruct *s2 = pool_alloc(&p);
    random_bytes(s2, sizeof(*s2));
    assertf(s1 == s2, "s1 reallocated as s2 from free list");
}

void test_temp_allocator() {
    temp_init();
    Checkpoint c = temp_save();
    for (size_t i = 0; i < 1000; i++) {
        assert(string_eq(SV("3-2-1 go!"), temp_format("%d-%d-%d go!", 3, 2, 1)));
        int *a = temp_alloc(int, 25);
        for (int j = 0; j < 25; j++) {
            a[j] = j;
        }
    }
    temp_rewind(c);

    assert(temp_allocator_global_arena->current == temp_allocator_global_arena->current);
    assert(temp_allocator_global_arena->current->position == sizeof(Arena));
}

void test_dynamic_deque() {
    Deque d = deque_init();

    for (int i = 0; i < 1000; i++) {
        *deque_push_head(&d, int, 1) = i;
    }
    for (int i = 0; i < 1000; i++) {
        *deque_push_tail(&d, int, 1) = i;
    }
    deque_pop_head(&d, int, 3);
    deque_pop_tail(&d, int, 1);
    deque_pop_tail(&d, int, 2);
    deque_pop_head(&d, int, 2);

    for (int i = 0; i < 1000; i++) {
        *deque_push_head(&d, int, 1) = i;
    }
    deque_free(&d);

    d = deque_init();
    for (size_t i = 0; i < 10; i++) {
        deque_push_head_bytes(&d, 64*MB, 16);
    }
    deque_pop_head_bytes(&d, 64*MB);

    for (size_t i = 0; i < 10; i++) {
        deque_push_tail_bytes(&d, 64*MB, 16);
    }
    deque_pop_tail_bytes(&d, 64*MB);
    deque_free(&d);

    d = deque_init();
    deque_push_head_bytes(&d, 64*MB, 16);
    deque_push_head_bytes(&d, 64*MB, 16);
    deque_push_head_bytes(&d, 64*MB, 16);
    deque_pop_head_bytes(&d, 128*MB);

    deque_push_tail_bytes(&d, 64*MB, 16);
    deque_push_tail_bytes(&d, 64*MB, 16);
    deque_push_tail_bytes(&d, 64*MB, 16);
    deque_pop_tail_bytes(&d, 128*MB);
}

void test_smol_map() {
    typedef struct {
        int *data;
        size_t length;
        size_t capacity;
    } Ints;

    Arena *arena = arena_init();
    SmolHashmap shm = {0};

    struct {String a; int b;} data[] = {
        { SV("Foo"),    121 },
        { SV("Bar"),    124 },
        { SV("Baz"),    127 },
        { SV("Hello"),  130 },
        { SV("World"),  123 },
        { SV("abcd"),   118 },
        { SV("efgh"),   11  },
        { SV("12345"),  99  },
        { SV("557w49"), 132 },
    };

    Ints values = {0};
    array_push(&values, 0); // reserving the 0th index
    for (size_t i = 0; i < array_len(data); i++) {
        smol_lookup(arena, &shm, string_hashfnv(data[i].a, 0), values.length);
        array_push(&values, data[i].b);
    }

    uint64_t x = 0;
    x = values.data[smol_lookup(0, &shm, string_hash(SV("World")), 0)];   assert (x == 123);
    x = values.data[smol_lookup(0, &shm, string_hash(SV("random")), 0)];  assert (x == 0);
    x = values.data[smol_lookup(0, &shm, string_hash(SV("Bar")), 0)];     assert (x == 124);
    x = values.data[smol_lookup(0, &shm, string_hash(SV("12345")), 0)];   assert (x == 99);

    smol_lookup(arena, &shm, string_hash(SV("World")), 1000);
    x = smol_lookup(0, &shm, string_hash(SV("World")), 1000);
    assert(x == 1000);

#if 0
    // will crash since value collides but `replace` is set to false
    smol_put(arena, &shm, string_hash(SV("Foo")), 0, false);
#endif

    arena_free(arena);
    free(values.data);
}

void test_singly_linked_list() {
    typedef struct IntNode IntNode;
    struct IntNode {
        int x;
        IntNode *next;
    };

    Arena *a = arena_init();
    IntNode *top = NULL;
    for (int i = 0; i < 10; i++) {
        IntNode *n = arena_new(a, IntNode);
        n->x = i;
        stack_push(top, n);
    }

    for (size_t i = 0; i < 10; i++) {
        printf("%d ", top->x);
        stack_pop(top);
    }
    printf("\n");

    IntNode *start = NULL, *end = NULL;
    for (int i = 0; i < 10; i++) {
        IntNode *n = arena_new(a, IntNode);
        n->x = i;
        queue_push(start, end, n);
    }

    for (size_t i = 0; i < 10; i++) {
        printf("%d ", start->x);
        queue_pop(start, end);
    }
    printf("\n");
}

void test_doubly_linked_list() {
    typedef struct IntNode IntNode;
    struct IntNode {
        int x;
        IntNode *next;
        IntNode *prev;
    };

    Arena *a = arena_init();

    // forwards iteration
    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 10; i++) {
            IntNode *n = arena_new(a, IntNode);
            n->x = i;
            dll_push_head(head, tail, n);
        }
        for (IntNode *end = tail; end; end=end->prev) {
            printf("%d ", end->x);
        }
        printf("\n");
    }

    // reverse iteration
    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 10; i++) {
            IntNode *n = arena_new(a, IntNode);
            n->x = i;
            dll_push_tail(head, tail, n);
        }
        for (IntNode *end = tail; end; end=end->prev) {
            printf("%d ", end->x);
        }
        printf("\n");
    }

    // popping head
    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 10; i++) {
            IntNode *n = arena_new(a, IntNode);
            n->x = i;
            dll_push_tail(head, tail, n);
        }
        for (size_t i = 0; i < 10; i++) {
            printf("%d ", head->x);
            dll_pop_head(head, tail);
        }
        assert(head == tail && tail == NULL);
        printf("\n");
    }

    // popping tail
    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 10; i++) {
            IntNode *n = arena_new(a, IntNode);
            n->x = i;
            dll_push_head(head, tail, n);
        }
        for (size_t i = 0; i < 10; i++) {
            printf("%d ", tail->x);
            dll_pop_tail(head, tail);
        }
        assert(head == tail && tail == NULL);
        printf("\n");
    }

    // insert after
    {
        typedef struct FloatNode FloatNode;
        struct FloatNode {
            float x;
            FloatNode *next;
            FloatNode *prev;
        };

        FloatNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 5; i++) {
            FloatNode *n = arena_new(a, FloatNode);
            n->x = (float)i;
            dll_push_tail(head, tail, n);
        }
        FloatNode *mid = head;
        for (size_t i = 0; i < 5; i++) {
            FloatNode *n = arena_new(a, FloatNode);
            n->x = -(float)i;
            dll_push_head(head, tail, n);
        }
        FloatNode *n = arena_new(a, FloatNode);
        n->x = 0.5;
        dll_insert_after(head, tail, mid, n);

        n = arena_new(a, FloatNode);
        n->x = -0.5;
        dll_insert_before(head, tail, mid, n);

        list_print(head, FloatNode, "%.1f", node->x);
    }

    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 5; i++) {
            IntNode *n = arena_new(a, IntNode);
            dll_push_head(head, tail, n)->x = i;
        }
        IntNode *mid = tail;
        for (int i = 0; i < 5; i++) {
            IntNode *n = arena_new(a, IntNode);
            dll_push_tail(head, tail, n)->x = i;
        }
        IntNode *n = arena_new(a, IntNode);
        n->x = 100;
        dll_replace(head, tail, mid, n);
        n = arena_new(a, IntNode);
        n->x = 1000;
        dll_replace(head, tail, mid, n);
        list_print(head, IntNode, "%d", node->x);
    }

    {
        IntNode *head = NULL, *tail = NULL;
        for (int i = 0; i < 5; i++) {
            IntNode *n = arena_new(a, IntNode);
            dll_push_head(head, tail, n)->x = i;
        }
        IntNode *mid = tail;
        for (int i = 0; i < 5; i++) {
            IntNode *n = arena_new(a, IntNode);
            dll_push_tail(head, tail, n)->x = i;
        }
        list_print(head, IntNode, "%d", node->x);
        dll_remove(head, tail, mid);
        dll_remove(head, tail, mid); // removing mid again does nothing

        // mid still has next and prev pointers
        dll_remove(head, tail, mid->next);
        dll_remove(head, tail, mid->prev);
        list_print(head, IntNode, "%d", node->x);
    }
}

// TODO: loop through and compare the snapshots of each of these tests
void test_linked_list() {
    test_singly_linked_list();
    test_doubly_linked_list();
}

void test_dynamic_string() {
    DString a = DS("hello");
    dstring_push(&a, SV(" world"));
    dstring_push_cstr(&a, " GGWP!!!!");
    dstring_push(&a, SV("\nTEST"));
    dstring_pushf(&a, " - %d %f %s %.*s", 123, -23423.123, "does this", SV_FMT(SV("even work??")));

    const char *actual = dstring_to_temp_cstr(&a);
    const char *expected = "hello world GGWP!!!!\nTEST - 123 -23423.123000 does this even work??";
    assertf(strcmp(actual, expected) == 0, "strings should be equal");
    assertf(a.string.data[a.string.length] == 0, "null terminator is present but popped off actual string");

    dstring_free(&a);
    assertf(mem_eq(&a, &((DString){0})), "dynamic string should be zeroed out");
}

int main() {
    test_dynamic_string();
    printf("\nExiting successfully\n");
    return 0;
}
