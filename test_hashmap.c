#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILER_H_IMPLEMENTATION
#define ENABLE_PROFILING
#include "profiler.h"

#include "arena.h"

#include "hashmap.h"
#include "migi.h"
#include "migi_lists.h"
#include "migi_string.h"
#include "migi_random.h"

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
    printf("%d %d\n", p->x, p->y);

    p = hms_get_ptr(&hm, SV("abcd"));
    if (!p)
        printf("key not present!\n");

    ptrdiff_t i = hms_get_index(&hm, SV("bar"));
    if (i != -1) {
        Point p = hm.data[i].value;
        printf("%d %d\n", p.x, p.y);
    }

    KVStrPoint *pair = hms_get_pair_ptr(&hm, SV("baz"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
           pair->value.y);

    KVStrPoint pair1 = hms_get_pair(&hm, SV("bazz"));
    printf("`%.*s`: (Point){%d %d}\n", SV_FMT(pair1.key), pair1.value.x,
           pair1.value.y);

    Point p1 = hms_get(&hm, SV("bla"));
    printf("bla: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, SV("blah"));
    printf("EMPTY: (Point){%d %d}\n", p2.x, p2.y);

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
               pair->value.y);
    }
    printf("\n");

    KVStrPoint deleted = hms_pop(&hm, SV("bar"));
    printf("Deleted: %.*s: (Point){%d %d}\n", SV_FMT(deleted.key),
           deleted.value.x, deleted.value.y);

    assertf(migi_mem_eq_single(&hms_get_pair(&hm, SV("bar")), &(KVStrPoint){0}),
            "empty returned for deleted keys");

    KVStrPoint bla = hms_get_pair(&hm, SV("bla"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(bla.key), bla.value.x, bla.value.y);

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
               pair->value.y);
    }

    hms_delete(&hm, SV("aaaaa"));
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
    printf("foo: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, SV("oof!"));
    printf("oof!: (Point){%d %d}\n", p2.x, p2.y);

    KVStrPoint p3 = hms_get_pair(&hm, SV("aaaaa"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(p3.key), p3.value.x, p3.value.y);
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
// Should print 13 0
void test_small_hashmap_collision() {
    MapStrInt hm = {0};
    Arena a = {0};
    *hms_entry(&a, &hm, SV("abcd")) = 12;
    *hms_entry(&a, &hm, SV("efgh")) = 13;

    hms_delete(&hm, SV("abcd"));
    int n = hms_get(&hm, SV("efgh"));
    printf("%d\n", n);
    n = hms_get(&hm, SV("efg"));
    printf("%d\n", n);
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
    // int *p1 = hms_get_ptr(&map2, SV("abcd"));

    Point i = hms_get(&map2, SV("efgh"));
    // Point ab = hms_get(&map, SV("a"));
    // int i1 = hms_get(&map2, SV("efgh"));

    KVStrInt pair = hms_get_pair(&map, SV("abcd"));
    // KVStrPoint pair1 = hms_get_pair(&map, SV("abcd"));

    KVStrInt *kv = hms_get_pair_ptr(&map, SV("abcd"));
    // KVStrPoint *kv1 = hms_get_pair_ptr(&map, SV("abcd"));

    KVStrInt del_int = hms_pop(&map, SV("abcd"));
    // KVStrPoint del_point = hms_pop(&map, SV("abcd"));

    hm_foreach(&map, pair) {
        printf("%.*s: %ld", SV_FMT(pair->key), pair->value);
    }
}

String random_string(Arena *a, size_t length) {
    char *chars = arena_push(a, char, length);

    for (size_t i = 0; i < length; i++) {
        chars[i] = random_range('a', 'z');
    }
    return (String){
        .data = chars,
        .length = length
    };
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
    for (size_t i = 0; i < 1024*1024; i++) {
        String str = random_string(&a, length);
        *hms_entry(&a, &map, str) = 1;
    }
    end_profiling_and_print_stats();

    begin_profiling();
    size_t count = 0;
    for (size_t i = 0; i < 1024*1024; i++) {
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
    test_search_fail();
    return 0;
}

