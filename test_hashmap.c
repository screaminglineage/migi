#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#include "arena.h"

#include "hashmap.h"
#include "migi.h"
#include "migi_lists.h"
#include "migi_random.h"
#include "migi_string.h"

typedef struct {
    int x, y;
} Point;

typedef struct {
    String key;
    Point value;
} KVStrPoint;

#if 0
typedef struct {
    HASHMAP_HEADER;

    // only needed for non-string hashmaps
    // bool (*hm_key_eq)(Key a, Key b);
    KVStrPoint *data;
} MapStrPoint;
#endif

typedef MapStr(KVStrPoint) MapStrPoint;

void test_basic() {
    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));
    hms_put(&a, &hm, SV("baz"), ((Point){5, 6}));
    hms_put(&a, &hm, SV("bla"), ((Point){7, 8}));

    Point *p = hms_get_ptr(&hm, SV("foo"));
    assert(p->x == 1 && p->y == 2);

    p = hms_get_ptr(&hm, SV("abcd"));
    assert(!p);

    ptrdiff_t i = hms_get_index(&hm, SV("bar"));
    assert(i != -1);
    Point p0 = hm.data[i].value;
    assert(p0.x == 3 && p0.y == 4);

    KVStrPoint *pair = hms_get_pair_ptr(&hm, SV("baz"));
    assert(string_eq(pair->key, SV("baz")) && pair->value.x == 5 && pair->value.y == 6);

    KVStrPoint pair1 = hms_get_pair(&hm, SV("bazz"));
    assert(string_eq(pair1.key, SV("")) && pair1.value.x == 0 && pair1.value.y == 0);

    Point p1 = hms_get(&hm, SV("bla"));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hms_get(&hm, SV("blah"));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
               pair->value.y);
    }
    printf("\n");

    assert(migi_mem_eq_single(&hm.data[0], &((KVStrPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVStrPoint){SV("foo"), ((Point){1, 2})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVStrPoint){SV("bar"), ((Point){3, 4})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVStrPoint){SV("baz"), ((Point){5, 6})})));
    assert(migi_mem_eq_single(&hm.data[4], &((KVStrPoint){SV("bla"), ((Point){7, 8})})));

    KVStrPoint deleted = hms_pop(&a, &hm, SV("bar"));
    assert(string_eq(deleted.key, SV("bar")) && deleted.value.x == 3 && deleted.value.y == 4);

    assertf(migi_mem_eq_single(&hms_get_pair(&hm, SV("bar")), &(KVStrPoint){0}),
            "empty returned for deleted keys");

    KVStrPoint bla = hms_get_pair(&hm, SV("bla"));
    assert(string_eq(bla.key, SV("bla")) && bla.value.x == 7 && bla.value.y == 8);

    hms_delete(&hm, SV("aaaaa"));

    // replacing old value of `foo`
    hms_put(&a, &hm, SV("foo"), ((Point){10, 20}));

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
                pair->value.y);
    }
    assert(migi_mem_eq_single(&hm.data[0], &((KVStrPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVStrPoint){SV("foo"), ((Point){10, 20})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVStrPoint){SV("bla"), ((Point){7, 8})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVStrPoint){SV("baz"), ((Point){5, 6})})));
}

void test_default_values() {
    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));

    // Setting default key and value
    // NOTE: This can only be done after atleast 1 insertion into the hashmap
    hm.data[HASHMAP_DEFAULT_PAIR] =
        (KVStrPoint){.key = SV("default"), .value = (Point){100, 100}};

    Point p1 = hms_get(&hm, SV("foo"));
    assert(p1.x == 1 && p1.y == 2);

    p1 = hms_get(&hm, SV("bar"));
    assert(p1.x == 3 && p1.y == 4);

    Point p2 = hms_get(&hm, SV("oof!"));
    assert(p2.x == 100 && p2.y == 100);

    KVStrPoint p3 = hms_get_pair(&hm, SV("aaaaa"));
    assert(string_eq(p3.key, SV("default")) && p3.value.x == 100 && p3.value.y == 100);
}

typedef struct {
    String key;
    int64_t value;
} KVStrInt;

typedef MapStr(KVStrInt) MapStrInt;

int hash_entry_cmp(const void *a, const void *b) {
    return ((KVStrInt *)b)->value - ((KVStrInt *)a)->value;
}

// Counts frequency of occurence of words from a text file
// Regular linear probing performs slightly better here, probably
// due to the overhead of robin hood probing
void frequency_analysis() {
    begin_profiling();
    StringBuilder sb = {0};
    read_file(&sb, SV("shakespeare.txt"));
    // read_file(&sb, SV("gatsby.txt"));
    // read_file(&sb, SV("test_hashmap.c"));
    String contents = sb_to_string(&sb);

    Arena a = {0};
    StringList words =
        string_split_chars_ex(&a, contents, SV(" \n"), SPLIT_SKIP_EMPTY);

    MapStrInt map = {0};

    list_foreach(words.head, StringNode, word) {
        String key = string_to_lower(&a, word->str);
        *hms_entry(&a, &map, key) += 1;
    }
    printf("size = %zu, capacity = %zu\n", map.size, map.capacity);
    end_profiling_and_print_stats();

    KVStrInt *entries = arena_memdup(&a, KVStrInt, map.data + 1, map.size);
    qsort(entries, map.size, sizeof(*entries), hash_entry_cmp);

#ifdef ENABLE_PROFILING
    begin_profiling();
    for (size_t i = 0; i < map.size; i++) {
        KVStrInt *pair = entries + i;
        hms_delete(&map, pair->key);
    }
    end_profiling_and_print_stats();
#else
    printf("Words sorted in descending order:\n");
    for (size_t i = 0; i < map.size; i++) {
        KVStrInt *pair = entries + i;
        printf("%.*s => %ld\n", SV_FMT(pair->key), pair->value);
    }
#endif
}

// `#define HASHMAP_INIT_CAP 4` before including hashmap.h
void test_small_hashmap_collision() {
    MapStrInt hm = {0};
    Arena a = {0};
    *hms_entry(&a, &hm, SV("abcd")) = 12;
    *hms_entry(&a, &hm, SV("efgh")) = 13;

    assert(hms_pop(&a, &hm, SV("abcd")).value == 12);
    assert(hms_get(&hm, SV("efgh")) == 13);
    assert(hms_get(&hm, SV("efg")) == 0);
    assert(hms_get(&hm, SV("abcd")) == 0);

    // Deleting last value in table
    *hms_entry(&a, &hm, SV("abcd")) = 10;
    hms_delete(&hm, SV("abcd"));
    assert(hms_get(&hm, SV("efgh")) == 13);
}

void test_type_safety() {
    Arena a = {0};
    MapStrInt map = {0};
    *hms_entry(&a, &map, SV("abcd")) = 12;

    MapStrPoint map2 = {0};
    *hms_entry(&a, &map2, SV("abcd")) = (Point){1, 2};

    hms_put(&a, &map2, SV("efgh"), ((Point){3, 4}));
    // hms_put(&a, &map2, SV("efgh"), SV("aaaaaaa"));

    Point *p = hms_get_ptr(&map2, SV("abcd"));
    unused(p);
    // int *p1 = hms_get_ptr(&map2, SV("abcd"));

    Point i = hms_get(&map2, SV("efgh"));
    unused(i);
    // Point ab = hms_get(&map, SV("a"));
    // int i1 = hms_get(&map2, SV("efgh"));

    KVStrInt pair = hms_get_pair(&map, SV("abcd"));
    unused(pair);
    // KVStrPoint pair1 = hms_get_pair(&map, SV("abcd"));

    KVStrInt *kv = hms_get_pair_ptr(&map, SV("abcd"));
    unused(kv);
    // KVStrPoint *kv1 = hms_get_pair_ptr(&map, SV("abcd"));

    KVStrInt del_int = hms_pop(&a, &map, SV("abcd"));
    unused(del_int);
    // KVStrPoint del_point = hms_pop(&a, &map, SV("abcd"));

    hm_foreach(&map, pair) {
        printf("%.*s: %ld", SV_FMT(pair->key), pair->value);
    }
}

String random_string(Arena *a, size_t length) {
    char *chars = arena_push(a, char, length);

    for (size_t i = 0; i < length; i++) {
        chars[i] = random_range('a', 'z');
    }
    return (String){.data = chars, .length = length};
}

// Inserts random strings into the hashmap and tries to later find them
// Since the strings are random, most of the searches will fail
// This is extremely slow on a regular linear probing due to having to search
// the entire table and then failing.
// The robin hood linear probing approach is MUCH faster in this case
void test_search_fail() {
    Arena a = {0};
    MapStrInt map = {0};

    begin_profiling();
    size_t length = 5;
    for (size_t i = 0; i < 1024 * 1024; i++) {
        String str = random_string(&a, length);
        *hms_entry(&a, &map, str) = 1;
    }
    end_profiling_and_print_stats();

    begin_profiling();
    size_t count = 0;
    for (size_t i = 0; i < 1024 * 1024; i++) {
        String str = random_string(&a, length);
        if (hms_contains(&map, str)) {
            count++;
        }
    }
    end_profiling_and_print_stats();

    printf("Matched elements: %zu\n", count);
    printf("Unmatched elements: %zu\n", map.size - count);
}

int main() {
    frequency_analysis();
    return 0;
}
