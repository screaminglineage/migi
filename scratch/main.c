#include "migi.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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


// #define DYNAMIC_ARRAY_USE_ARENA
#include "dynamic_array.h"
#include "migi_random.h"
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
        Temp save = arena_save(arena);
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
    Temp checkpoint = arena_save(arena1);
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

    Temp save = arena_save(arena);
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
    Temp checkpoint = arena_save(arena);

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
        sb_push_string(&sb, S("hello"));
        sb_push_string(&sb, S("foo"));
        sb_push_string(&sb, S("bar"));
        sb_push_string(&sb, S("baz"));

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
        sb_push_string(&sb1, S("foo"));
        sb_push_string(&sb1, S("bar"));
        sb_push_string(&sb1, S("baz"));
        sb_pushf(&sb1, "\nhello world! %d, %.*s, %f\n", 12, SV_FMT(S("more stuff")), 3.14);
        sb_pushf(&sb1, "abcd efgh 12345678 %x\n", 0xdeadbeef);

        String str = sb_to_string(&sb1);
        printf("%.*s\n", SV_FMT(str));
        sb_free(&sb1);
    }

    static char buf[1*MB];
    Arena *a = arena_init_static(buf, sizeof(buf));
    String str = str_from_file(a, S("./src/str_builder.h"));
    const char *cstr = str_to_cstr(a, str);

    sb_pushf(&sb, "%s\n", cstr);
    printf("%s", sb_to_cstr(&sb));
    assert(sb_length(&sb) == 67 + 67 + str.length + 1);

    {

        char buffer[2048] = {0};
        StringBuilder sb_static = sb_init_static(buffer, sizeof(buffer));
        sb_pushf(&sb_static, "%.*s/%s:%d\n", SV_FMT(S("FILE PATH")), __FILE__, __LINE__);
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

    assertf(mem_eq_array(buf1, buf2, size),
            "random with same seed must have same data");

    for (size_t i = 0; i < 10; i++) {
        assert(random_range_exclusive(-1, 0) != 0);
        assert(random_range_exclusive(0, 1) != 1);
    }

    size_t count = 5;
    int arr[]         = { 0,  1,  2,  3,  4};
    int64_t weights[] = {25, 50, 75, 50, 25};
    int frequencies[] = { 0,  0,  0,  0,  0};

    assert(array_len(arr)         == count);
    assert(array_len(weights)     == count);
    assert(array_len(frequencies) == count);

    int sample_size = 1000000;
    int total = 0;
    for (int i = 0; i < sample_size; i++) {
        int chosen = random_choose_fuzzy(arr, int, weights, array_len(weights));
        frequencies[chosen] += 1;
        total += 1;
    }

    for (size_t i = 0; i < count; i++) {
        double percentage = ((double)frequencies[i] / (double)total) * 100.0;
        printf("[%zu] => %.2f%%\n", i, percentage);
    }
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

static void assert_str_split(StringSplitTest t) {
    size_t count = 0;
    size_t char_count = 0;
    if (t.actual.total_size != 0) {
        strlist_foreach(&t.actual, node) {
            String expected = t.expected.data[count];
            String actual = node->string;

            assert(count < t.expected.length);
            assertf(str_eq(actual, expected), "expected: `%.*s,` got: `%.*s`",
                    SV_FMT(expected), SV_FMT(actual));
            count++;
            char_count += actual.length;
        }
    }
    assertf(count == t.expected.length, "expected length: %zu, actual length: %zu", t.expected.length, count);
    assert(char_count == t.actual.total_size);
}


void test_str_split_and_join() {
    Arena *a = arena_init();
    StringSplitTest splits[] = {
        {
            .expected = slice_from(String, StringSlice, S("Mary"), S("had"), S("a"), S("little"), S("lamb")),
            .actual = str_split(a, S("Mary had a little lamb"), S(" "))
        },
        {
            .expected = slice_from(String, StringSlice, S("Mary"), S("had"), S("a"), S("little"), S("lamb")),
            .actual =  str_split_ex(a, S(" Mary    had   a   little   lamb "), S(" "), Split_SkipEmpty)
        },
        {
            .expected = slice_from(String, StringSlice, S(""), S("Mary"), S(""), S(""), S(""), S("had"), S(""), S(""), 
                    S("a"), S(""), S(""), S("little"), S(""), S(""), S("lamb")),
            .actual =  str_split(a, S(" Mary    had   a   little   lamb"), S(" "))
        },
        {
            .expected = slice_from(String, StringSlice, S("Mary"), S("had"), S("a"), S("little"), S("lamb"), S("")),
            .actual = str_split(a, S("Mary--had--a--little--lamb--"), S("--"))
        },
        {
            .expected = (StringSlice){0},
            .actual = str_split(a, S("Mary had a little lamb"), S(""))
        },
        {
            .expected = slice_from(String, StringSlice, S(""), S("Mary"), S("had"), S("a"), S("little"), S("lamb")),
            .actual = str_split(a, S(" Mary had a little lamb"), S(" ")),
        },
        {
            .expected = slice_from(String, StringSlice, S(""), S("1"), S("")),
            .actual = str_split(a, S("010"), S("0"))
        },
        {
            .expected = slice_from(String, StringSlice, S("2020"), S("11"), S("03"), S("23"), S("59"), S("")),
            .actual = str_split_ex(a, S("2020-11-03 23:59@"), S("- :@"), Split_AsChars)
        },
        {
            .expected = slice_from(String, StringSlice, S("2020"), S("11"), S("03"), S("23"), S("59")),
            .actual = str_split_ex(a, S("2020-11--03 23:59@"), S("- :@"), Split_SkipEmpty|Split_AsChars)
        },
        {
            .expected = slice_from(String, StringSlice, S("2020"), S("11"), S(""), S("03"), S("23"), S("59"), S("")),
            .actual = str_split_ex(a, S("2020-11--03 23:59@"), S("- :@"), Split_AsChars)
        },
    };

    for (size_t i = 0; i < array_len(splits); i++) {
        assert_str_split(splits[i]);
    }

    StringList list = str_split_ex(a, S("2020-11--03 23:59@"), S("- :@"), Split_AsChars|Split_SkipEmpty);
    String expected = strlist_join(a, &list, S("-"));
    assert(str_eq(expected, S("2020-11-03-23-59")));

    list = str_split(a, S("--foo--bar--baz--"), S("--"));
    expected = strlist_join(a, &list, S("=="));
    assert(str_eq(expected, S("==foo==bar==baz==")));

    expected = strlist_join(a, &list, S(""));
    assert(str_eq(expected, S("foobarbaz")));
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

void test_str_list() {
    test_str_split_and_join();

    Temp tmp = arena_temp();
    Arena *a = tmp.arena;
    StringList sl = {0};

    strlist_push(a, &sl, S("This is a "));
    strlist_push(a, &sl, S("string being built "));
    strlist_push_cstr(a, &sl, "over time");
    strlist_push_char(a, &sl, '!');

    char *s = "\nMore Stuff Here\n";
    size_t len = strlen(s);
    strlist_push_buffer(a, &sl, s, len);
    strlist_pushf(a, &sl,
                  "%s:%d:%s: %.15f ... and more stuff... blah blah blah",
                  __FILE__, __LINE__, __func__, M_PI);
    String final_str = strlist_to_string(a, &sl);
    printf("%.*s", SV_FMT(final_str));



    String foo = S("foo bar,baz biz,1 2 3");
    StringList l = str_split(a, foo, S(","));
    l = strlist_split(a, &l, S(" "));

    String expected[] = {
        S("foo"), S("bar"), S("baz"), S("biz"), S("1"), S("2"), S("3"),
    };
    size_t i = 0;
    strlist_foreach(&l, node) {
        assertf(str_eq(expected[i], node->string), 
                "expected: %.*s, but got %.*s\n", 
                SV_FMT(expected[i]), SV_FMT(node->string));
        i++;
    }

    arena_temp_release(tmp);
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
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;


    // str_eq
    {
        assert(str_eq(S("abcd"), S("abcd")));
        assert(str_eq(S(""), S("")));
        assert(str_eq((String){0}, (String){0}));

        assert(str_eq_cstr((String){0}, "", 0));
        assert(str_eq_cstr((String){0}, "", 0));
        assert(str_eq_cstr(S("yes!!"), "yes!!", 0));

        assert(str_eq(str_skip(S("1234"), 5), (String){0}));
        assert(str_eq((String){0}, str_skip(S("4567"), 5)));
        assert(str_eq(str_skip(S("1234"), 5), str_skip(S("4567"), 5)));
        assert(str_eq(str_take(S("hello"), 0), str_take(S("world"), 0)));
        assert(str_eq(str_slice(S("hello"), 2, 2), str_slice(S("world"), 2, 2)));

        assert(str_eq_ex(S("STRING"), S("sTRinG"), Eq_IgnoreCase));
        assert(!str_eq_ex(S("foo"),   S("bar"),    Eq_IgnoreCase));
        assert(str_eq_cstr(S(""),     "",           Eq_IgnoreCase));
        assert(str_eq_cstr(S("abcd"), "ABCD",       Eq_IgnoreCase));
    }

    // upper/lower
    {
        assert(str_eq(str_to_lower(a, S("HELLO world!!!")), S("hello world!!!")));
        assert(str_eq(str_to_upper(a, S("FOO bar baz!")),   S("FOO BAR BAZ!")));
    }

    // str_skip_while
    {
        assert(str_eq(str_skip_while(S("1234abcd"),  skip_nums, NULL, 0),                 S("abcd")));
        assert(str_eq(str_skip_while(S("1234abcd"),  skip_nums, NULL, SkipWhile_Reverse), S("1234abcd")));
        assert(str_eq(str_skip_while(S("foo90"),     skip_nums, NULL, SkipWhile_Reverse), S("foo")));
        assert(str_eq(str_skip_while(S("foo90"),     skip_nums, NULL, 0),                 S("foo90")));
        assert(str_eq(str_skip_while(S(""),          skip_nums, NULL, 0),                 S("")));
        assert(str_eq(str_skip_chars(S("abcd"),      S("abd"), 0),                       S("cd")));
        assert(str_eq(str_skip_chars(S("abcd"),      S("da"), SkipWhile_Reverse),        S("abc")));
    }

    // str_trim
    {
        String str = S("\n    hello       \n");
        assert(str_eq(str_trim_right(str),       S("\n    hello")));
        assert(str_eq(str_trim_left(str),        S("hello       \n")));
        assert(str_eq(str_trim(str),             S("hello")));
        assert(str_eq(str_trim(S("foo")),       S("foo")));
        assert(str_eq(str_trim(S("\t\r\nfoo")), S("foo")));
        assert(str_eq(str_trim(S("foo\r\n\t")), S("foo")));
        assert(str_eq(str_trim(S(" \r\n\t")),   S("")));
        assert(str_eq(str_trim(S("")),          S("")));
    }

    // str_find
    {
        assert(str_find(S("hello"),  S("he"))     == 0);
        assert(str_find(S("hello"),  S("llo"))    == 2);
        assert(str_find(S("hello"),  S("o"))      == 4);
        assert(str_find(S("abcabc"), S("cab"))    == 2);
        assert(str_find(S("hello"),  S("world"))  == 5);
        assert(str_find(S("short"),  S("longer")) == 5);
        assert(str_find(S("abc"),    S("abcd"))   == 3);
        assert(str_find(S("abc"),    S("z"))      == 3);
        assert(str_find(S(""),       S(""))       == 0);
        assert(str_find(S("abc"),    S(""))       == 0);
        assert(str_find(S(""),       S("a"))      == 0);
        assert(str_find(S("aaaaa"),  S("aa"))     == 0);
    }

    // str_find (reverse)
    {
        assert(str_find_ex(S("hello"),  S("he"),  Find_Reverse) == 0);
        assert(str_find_ex(S("hello"),  S("llo"), Find_Reverse) == 2);
        assert(str_find_ex(S("hello"),  S("o"),   Find_Reverse) == 4);
        assert(str_find_ex(S("hello"),  S("world"),  Find_Reverse) == -1);
        assert(str_find_ex(S("short"),  S("longer"), Find_Reverse) == -1);
        assert(str_find_ex(S("abc"),    S("abcd"),   Find_Reverse) == -1);
        assert(str_find_ex(S("abc"),    S("z"),      Find_Reverse) == -1);
        assert(str_find_ex(S(""),       S(""),  Find_Reverse) == 0);
        assert(str_find_ex(S("abc"),    S(""),  Find_Reverse) == 3);
        assert(str_find_ex(S(""),       S("a"), Find_Reverse) == -1);
        assert(str_find_ex(S("aaaaa"),  S("aa"), Find_Reverse) == 3);
    }

    // str_starts_with
    {
        assert(str_starts_with(S("hello"),  S("he"))     == true);
        assert(str_starts_with(S("hello"),  S("hello"))  == true);
        assert(str_starts_with(S("hello"),  S("h"))      == true);
        assert(str_starts_with(S("hello"),  S("llo"))    == false);
        assert(str_starts_with(S("short"),  S("longer")) == false);
        assert(str_starts_with(S("abc"),    S("abcd"))   == false);
        assert(str_starts_with(S("abc"),    S(""))       == true);
        assert(str_starts_with(S(""),       S(""))       == true);
        assert(str_starts_with(S(""),       S("a"))      == false);
    }

    // str_ends_with
    {
        assert(str_ends_with(S("hello"),  S("lo"))     == true);
        assert(str_ends_with(S("hello"),  S("hello"))  == true);
        assert(str_ends_with(S("hello"),  S("he"))     == false);
        assert(str_ends_with(S("short"),  S("longer")) == false);
        assert(str_ends_with(S("abc"),    S("abcd"))   == false);
        assert(str_ends_with(S("abc"),    S(""))       == true);
        assert(str_ends_with(S(""),       S(""))       == true);
        assert(str_ends_with(S(""),       S("a"))      == false);
    }

    // str_slice
    {
        assert(str_eq(str_slice(S("hello"), 0, 5), S("hello")));
        assert(str_eq(str_slice(S("hello"), 0, 2), S("he")));
        assert(str_eq(str_slice(S("hello"), 2, 5), S("llo")));
        assert(str_eq(str_slice(S("hello"), 1, 4), S("ell")));
        assert(str_eq(str_slice(S("abc"), 0, 0), S("")));
        assert(str_eq(str_slice(S("abc"), 1, 1), S("")));
        assert(str_eq(str_slice(S("abc"), 4, 4), S("")));
        assert(str_eq(str_slice(S("abc"), 5, 5), S("")));
        assert(str_eq(str_slice(S(""), 0, 0), S("")));
        assert(str_eq(str_slice(S(""), 1, 2), S("")));
    }

    // str_skip
    {
        assert(str_eq(str_skip(S("hello"), 0), S("hello")));
        assert(str_eq(str_skip(S("hello"), 3), S("lo")));
        assert(str_eq(str_skip(S("hello"), 1), S("ello")));
        assert(str_eq(str_skip(S("hello"), 5), S("")));
        assert(str_eq(str_skip(S("hello"), 10), S("")));
        assert(str_eq(str_skip(S(""), 0), S("")));
        assert(str_eq(str_skip(S(""), 1), S("")));
    }

    // str_take
    {
        assert(str_eq(str_take(S("hello"), 0), S("")));
        assert(str_eq(str_take(S("hello"), 3), S("hel")));
        assert(str_eq(str_take(S("hello"), 1), S("h")));
        assert(str_eq(str_take(S("hello"), 5), S("hello")));
        assert(str_eq(str_take(S("hello"), 10), S("hello")));
        assert(str_eq(str_take(S(""), 0), S("")));
        assert(str_eq(str_take(S(""), 1), S("")));
    }

    // str_find_rev
    {
        assert(str_find_ex(S("hello"),  S("he"),     Find_Reverse) == 0);
        assert(str_find_ex(S("hello"),  S("llo"),    Find_Reverse) == 2);
        assert(str_find_ex(S("hello"),  S("o"),      Find_Reverse) == 4);
        assert(str_find_ex(S("banana"), S("ana"),    Find_Reverse) == 3);
        assert(str_find_ex(S("abcabc"), S("cab"),    Find_Reverse) == 2);
        assert(str_find_ex(S("hello"),  S("world"),  Find_Reverse) == -1);
        assert(str_find_ex(S("short"),  S("longer"), Find_Reverse) == -1);
        assert(str_find_ex(S("abc"),    S("abcd"),   Find_Reverse) == -1);
        assert(str_find_ex(S("abc"),    S("z"),      Find_Reverse) == -1);
        assert(str_find_ex(S(""),       S(""),       Find_Reverse) == 0);
        assert(str_find_ex(S("abc"),    S(""),       Find_Reverse) == 3);
        assert(str_find_ex(S(""),       S("a"),      Find_Reverse) == -1);
        assert(str_find_ex(S("aaaaa"),  S("aa"),     Find_Reverse) == 3);
    }

    // str_reverse
    {
        assert(str_eq(str_reverse(a, S("")), S("")));
        assert(str_eq(str_reverse(a, S("hello world")), S("dlrow olleh")));
    }

    // str_replace
    {
        assert(str_eq(str_replace(a, S(""),              S(""),      S("")),     S("")));
        assert(str_eq(str_replace(a, S("foo"),           S(""),      S("bar")),  S("barfbarobarobar")));
        assert(str_eq(str_replace(a, S("foo"),           S("bar"),   S("")),     S("foo")));
        assert(str_eq(str_replace(a, S("foo"),           S("foo"),   S("")),     S("")));
        assert(str_eq(str_replace(a, S("hello world!!"), S("ll"),    S("yy")),   S("heyyo world!!")));
        assert(str_eq(str_replace(a, S("aaa"),           S("a"),     S("bar")),  S("barbarbar")));
        assert(str_eq(str_replace(a, S("hello world"),   S("l"),     S("x")),    S("hexxo worxd")));
        assert(str_eq(str_replace(a, S("start starry starred restart started"),
                                                                 S("start"), S("part")),
                                                                                           S("part starry starred repart parted")));
    }

    // str_cut
    {
        // Default
        {
            StrCut cut = {0};

            cut = str_cut(S("hello world"), S(" "));
            assert(cut.found == true
                    && str_eq(cut.head, S("hello"))
                    && str_eq(cut.tail, S("world")));

            cut = str_cut(S("hello==++==world"), S("==++=="));
            assert(cut.found == true
                    && str_eq(cut.head, S("hello"))
                    && str_eq(cut.tail, S("world")));

            cut = str_cut(S("world"), S("world"));
            assert(cut.found == true
                    && str_eq(cut.head, S(""))
                    && str_eq(cut.tail, S("")));

            cut = str_cut(S("world"), S(""));
            assert(cut.found == true
                    && str_eq(cut.head, S(""))
                    && str_eq(cut.tail, S("world")));

            cut = str_cut(S(""), S(""));
            assert(cut.found == true
                    && str_eq(cut.head, S(""))
                    && str_eq(cut.tail, S("")));

            cut = str_cut(S("hello"), S("llo"));
            assert(cut.found == true
                    && str_eq(cut.head, S("he"))
                    && str_eq(cut.tail, S("")));

            cut = str_cut(S("abcd"), S("e"));
            assert(cut.found == false);

        }

        // Cut_AsChars
        {

            String str1 = S("2020-11--03 23:59@");
            strcut_foreach(str1, S("- :@"), Cut_AsChars, it) {
                printf("=> `%.*s`\n", SV_FMT(it.split));
            }
            assertf(str_eq(str1, S("2020-11--03 23:59@")), "original string remains intact");

            String str2 = S("a,b,c,");
            strcut_foreach(str2, S(","), 0, it) {
                printf("=> `%.*s`\n", SV_FMT(it.split));
            }
            assertf(str_eq(str2, S("a,b,c,")), "original string remains intact");

            {
                String c = S("a+-b");
                String delims = S("-+");
                StrCut cut = str_cut_ex(c, delims, Cut_AsChars);
                assert(cut.found);
                assert(str_eq(cut.head, S("a")));

                cut = str_cut_ex(cut.tail, delims, Cut_AsChars);
                assert(cut.found);
                assert(str_eq(cut.head, S("")));

                cut = str_cut_ex(cut.tail, delims, Cut_AsChars);
                assert(!cut.found);
                assert(str_eq(cut.head, S("b")));
            }
        }
    }

    arena_temp_release(tmp);
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
    Temp tmp = temp_save();
    for (size_t i = 0; i < 1000; i++) {
        assert(str_eq(S("3-2-1 go!"), temp_format("%d-%d-%d go!", 3, 2, 1)));
        int *a = temp_alloc(int, 25);
        for (int j = 0; j < 25; j++) {
            a[j] = j;
        }
    }
    temp_rewind(tmp);

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
        { S("Foo"),    121 },
        { S("Bar"),    124 },
        { S("Baz"),    127 },
        { S("Hello"),  130 },
        { S("World"),  123 },
        { S("abcd"),   118 },
        { S("efgh"),   11  },
        { S("12345"),  99  },
        { S("557w49"), 132 },
    };

    Ints values = {0};
    array_push(&values, 0); // reserving the 0th index
    for (size_t i = 0; i < array_len(data); i++) {
        smol_lookup(arena, &shm, str_hash(data[i].a), values.length);
        array_push(&values, data[i].b);
    }

    uint64_t x = 0;
    x = values.data[smol_lookup(0, &shm, str_hash(S("World")), 0)];   assert (x == 123);
    x = values.data[smol_lookup(0, &shm, str_hash(S("random")), 0)];  assert (x == 0);
    x = values.data[smol_lookup(0, &shm, str_hash(S("Bar")), 0)];     assert (x == 124);
    x = values.data[smol_lookup(0, &shm, str_hash(S("12345")), 0)];   assert (x == 99);

    smol_lookup(arena, &shm, str_hash(S("World")), 1000);
    x = smol_lookup(0, &shm, str_hash(S("World")), 1000);
    assert(x == 1000);

#if 0
    // will crash since value collides but `replace` is set to false
    smol_put(arena, &shm, str_hash(S("Foo")), 0, false);
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
    dstring_push(&a, S(" world"));
    dstring_push_cstr(&a, " GGWP!!!!");
    dstring_push(&a, S("\nTEST"));
    dstring_pushf(&a, " - %d %f %s %.*s", 123, -23423.123, "does this", SV_FMT(S("even work??")));

    const char *actual = dstring_to_temp_cstr(&a);
    const char *expected = "hello world GGWP!!!!\nTEST - 123 -23423.123000 does this even work??";
    assertf(strcmp(actual, expected) == 0, "strings should be equal");
    assertf(a.data[a.length] == 0, "null terminator is present but popped off actual string");

    dstring_free(&a);
    assertf(mem_eq(&a, &((DString){0})), "dynamic string should be zeroed out");
}


int main() {

    Temp tmp = arena_temp();
    Arena *a = tmp.arena;
    unused(a);

    test_string();
    arena_temp_release(tmp);
    printf("\nExiting successfully\n");

    return 0;
}
