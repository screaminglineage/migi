#include "timing.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILER_H_IMPLEMENTATION
#define ENABLE_PROFILING
#include "profiler.h"

#include "arena.h"

// #define HASHMAP_INIT_CAP 4
// #define HASHMAP_LOAD_FACTOR 0.25
// #define HASHMAP_TRACK_MAX_PROBE_LENGTH
#include "hashmap.h"
#include "migi.h"
#include "migi_lists.h"
#include "migi_random.h"
#include "migi_string.h"


void test_basic() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        String key;
        Point value;
    } KVStrPoint;

    typedef struct {
        HASHMAP_HEADER;

        KVStrPoint *data;
    } MapStrPoint;

    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));
    hms_put(&a, &hm, SV("baz"), ((Point){5, 6}));
    hms_put_pair(&a, &hm, ((KVStrPoint){
        .key = SV("bla"),
        .value = (Point){7, 8}}));

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

    KVStrPoint deleted = hms_pop(&hm, SV("bar"));

    assert(string_eq(deleted.key, SV("bar")) && deleted.value.x == 3 && deleted.value.y == 4);

    KVStrPoint t = hms_get_pair(&hm, SV("bar"));
    assertf(migi_mem_eq_single(&t, &(KVStrPoint){0}), "empty returned for deleted keys");

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
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        String key;
        Point value;
    } KVStrPoint;

    typedef struct {
        HASHMAP_HEADER;

        KVStrPoint *data;
    } MapStrPoint;

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

// key must come before value
// there shouldnt be any other elements between them
typedef struct {
    String key;
    int64_t value;
} KVStrInt;

typedef struct {
    HASHMAP_HEADER;

    // only needed for non-string hashmaps
    // bool (*hm_key_eq)(Key a, Key b);
    KVStrInt *data;
} MapStrInt;


int hash_entry_cmp(const void *a, const void *b) {
    return ((KVStrInt *)b)->value - ((KVStrInt *)a)->value;
}

// Counts frequency of occurence of words from a text file
// Regular linear probing performs slightly better here, probably
// due to the overhead of robin hood probing
void frequency_analysis() {
    StringBuilder sb = {0};
    read_file(&sb, SV("shakespeare.txt"));
    // read_file(&sb, SV("gatsby.txt"));
    // read_file(&sb, SV("hashmap_test.txt"));
    String contents = sb_to_string(&sb);

    Arena a = {0};
    StringList words =
        string_split_chars_ex(&a, contents, SV(" \n"), SPLIT_SKIP_EMPTY);

    MapStrInt map = {0};

    printf("Inserting items:\n");
    begin_profiling();
    list_foreach(words.head, StringNode, word) {
        String key = string_to_lower(&a, word->string);
        *hms_entry(&a, &map, key) += 1;
    }
    printf("size = %zu, capacity = %zu\n", map.size, map.capacity);
    end_profiling_and_print_stats();

    KVStrInt *entries = arena_memdup(&a, KVStrInt, map.data + 1, map.size);
    qsort(entries, map.size, sizeof(*entries), hash_entry_cmp);

#ifdef ENABLE_PROFILING
    printf("\n\nDeleting items:\n");
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
#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    printf("Maximum Probe Length: %zu\n", hashmap_max_probe_length);
#endif
}

// `#define HASHMAP_INIT_CAP 4` before including hashmap.h
void test_small_hashmap_collision() {
    MapStrInt hm = {0};
    Arena a = {0};
    *hms_entry(&a, &hm, SV("abcd")) = 12;
    *hms_entry(&a, &hm, SV("efgh")) = 13;

    assert(hms_pop(&hm, SV("abcd")).value == 12);
    assert(hms_get(&hm, SV("efgh")) == 13);
    assert(hms_get(&hm, SV("efg")) == 0);
    assert(hms_get(&hm, SV("abcd")) == 0);

    // Deleting last value in table
    *hms_entry(&a, &hm, SV("abcd")) = 10;
    assert(hms_pop(&hm, SV("abcd")).value == 10);
    hms_delete(&hm, SV("abcd"));
    assert(hms_get(&hm, SV("efgh")) == 13);

    // Popping (with swap-remove) with hashmap at max capacity
    // [3/4 elements] (with load factor >= 0.75, and init capacity 4)
    MapStrInt map = {0};
    hms_put(&a, &map, SV("a"), 1);
    hms_put(&a, &map, SV("b"), 2);
    hms_put(&a, &map, SV("c"), 3);

    KVStrInt kv = hms_pop(&map, SV("a"));
    assert(migi_mem_eq_single(&kv, &((KVStrInt){SV("a"), 1})));

    kv = hms_get_pair(&map, SV("b"));
    assert(migi_mem_eq_single(&kv, &((KVStrInt){SV("b"), 2})));

    kv = hms_get_pair(&map, SV("c"));
    assert(migi_mem_eq_single(&kv, &((KVStrInt){SV("c"), 3})));
}

void test_type_safety() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        String key;
        Point value;
    } KVStrPoint;

    typedef struct {
        HASHMAP_HEADER;

        KVStrPoint *data;
    } MapStrPoint;

    Arena a = {0};
    MapStrInt map = {0};
    *hms_entry(&a, &map, SV("abcd")) = 12;

    hms_put(&a, &map, SV("ijkl"), 100);
    // hms_put(&a, &map, SV("ijkl"), ((Point){1, 1}));

    MapStrPoint map2 = {0};
    *hms_entry(&a, &map2, SV("abcd")) = (Point){1, 2};
    // *hms_entry(&a, &map2, SV("abcd")) = 100;

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

    KVStrInt del_int = hms_pop(&map, SV("abcd"));
    unused(del_int);
    // KVStrPoint del_point = hms_pop(&a, &map, SV("abcd"));

    assert(migi_mem_eq_single(&map.data[0], &((KVStrPoint){0})));
    assert(migi_mem_eq_single(&map.data[1], &((KVStrInt){SV("ijkl"), 100})));

    assert(migi_mem_eq_single(&map2.data[0], &((KVStrPoint){0})));
    assert(migi_mem_eq_single(&map2.data[1], &((KVStrPoint){SV("abcd"), ((Point){1, 2})})));
    assert(migi_mem_eq_single(&map2.data[2], &((KVStrPoint){SV("efgh"), ((Point){3, 4})})));


    hm_foreach(&map, pair) {
        printf("%.*s: %ld", SV_FMT(pair->key), pair->value);
    }
    printf("\n");
    hm_foreach(&map2, pair) {
        printf("%.*s: (Point){%d, %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);
    }
    printf("\n");
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
void profile_search_fail() {
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

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    printf("Maximum Probe Length: %zu\n", hashmap_max_probe_length);
#endif
}


void test_basic_struct_key() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        int a, b;
        float f;
    } Foo;

    typedef struct {
        Foo key;
        Point value;
    } KVFooPoint;

    typedef struct {
        HASHMAP_HEADER;

        KVFooPoint *data;
    } MapFooPoint;

    Arena a = {0};
    MapFooPoint hm = {0};

    hm_put(&a, &hm, ((Foo){1, 2, 3.14f}), ((Point){1, 2}));
    hm_put(&a, &hm, ((Foo){3, 4, 1.14f}), ((Point){3, 4}));
    hm_put(&a, &hm, ((Foo){5, 6, 1.73f}), ((Point){5, 6}));
    hm_put_pair(&a, &hm, ((KVFooPoint){
        .key = (Foo){7, 8, 1.61f},
        .value = (Point){7, 8}}));

    Point *p = hm_get_ptr(&hm, ((Foo){1, 2, 3.14f}));
    assert(p->x == 1 && p->y == 2);

    p = hm_get_ptr(&hm, ((Foo){100, 200, 1.23f}));
    assert(!p);

    ptrdiff_t i = hm_get_index(&hm, ((Foo){3, 4, 1.14f}));
    assert(i != -1);
    Point p0 = hm.data[i].value;
    assert(p0.x == 3 && p0.y == 4);

    KVFooPoint *pair = hm_get_pair_ptr(&hm, ((Foo){ 5, 6, 1.73f }));
    assert(migi_mem_eq_single(&pair->key, &((Foo){ 5, 6, 1.73f })) && pair->value.x == 5 && pair->value.y == 6);

    KVFooPoint pair1 = hm_get_pair(&hm, ((Foo){100, 100, 0}));
    assert(pair1.key.a == 0 && pair1.key.b == 0 && pair1.key.f == 0.0f && pair1.value.x == 0 && pair1.value.y == 0);

    Point p1 = hm_get(&hm, ((Foo){7, 8, 1.61f}));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hm_get(&hm, ((Foo){99, 99, 0}));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        Foo f = pair->key;
        printf("(Foo){%d %d %.2f}: (Point){%d %d}\n", f.a, f.b, f.f, 
            pair->value.x, pair->value.y);
    }
    printf("\n");

    assert(migi_mem_eq_single(&hm.data[0], &((KVFooPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVFooPoint){(Foo){1, 2, 3.14f}, ((Point){1, 2})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVFooPoint){(Foo){3, 4, 1.14f}, ((Point){3, 4})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVFooPoint){(Foo){5, 6, 1.73f}, ((Point){5, 6})})));
    assert(migi_mem_eq_single(&hm.data[4], &((KVFooPoint){(Foo){7, 8, 1.61f}, ((Point){7, 8})})));

    Foo key_to_delete = (Foo){3, 4, 1.14f};
    KVFooPoint deleted = hm_pop(&hm, key_to_delete);

    assert(migi_mem_eq_single(&deleted.key, &key_to_delete) && deleted.value.x == 3 && deleted.value.y == 4);

    KVFooPoint t = hm_get_pair(&hm, key_to_delete);
    assertf(migi_mem_eq_single(&t, &(KVFooPoint){0}), "empty returned for deleted keys");

    Foo bla_key = (Foo){7, 8, 1.61f};
    KVFooPoint bla = hm_get_pair(&hm, bla_key);
    assert(migi_mem_eq_single(&bla.key, &bla_key) && bla.value.x == 7 && bla.value.y == 8);

    hm_delete(&hm, ((Foo){300, 400, 1e-6f}));

    // replacing old value of `(Foo){1, 2, 3.14f}``
    hm_put(&a, &hm, ((Foo){1, 2, 3.14f}), ((Point){10, 20}));

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        Foo f = pair->key;
        printf("(Foo){%d %d %.2f}: (Point){%d %d}\n", f.a, f.b, f.f,
                pair->value.x, pair->value.y);
    }
    assert(migi_mem_eq_single(&hm.data[0], &((KVFooPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVFooPoint){(Foo){1, 2, 3.14f}, ((Point){10, 20})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVFooPoint){(Foo){7, 8, 1.61f}, ((Point){7, 8})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVFooPoint){(Foo){5, 6, 1.73f}, ((Point){5, 6})})));
}


void test_basic_primitive_key() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        int key;
        Point value;
    } KVIntPoint;

    typedef struct {
        HASHMAP_HEADER;
        KVIntPoint *data;
    } MapIntPoint;


    Arena a = {0};
    MapIntPoint hm = {0};

    // inserts (equivalent to original Foo keys)
    hm_put(&a, &hm, 1, ((Point){1, 2}));
    hm_put(&a, &hm, 3, ((Point){3, 4}));
    hm_put(&a, &hm, 5, ((Point){5, 6}));
    hm_put_pair(&a, &hm, ((KVIntPoint){
        .key = 7,
        .value = (Point){7, 8}
    }));

    // get pointer for existing key
    Point *p = hm_get_ptr(&hm, 1);
    assert(p->x == 1 && p->y == 2);

    // missing key should return NULL pointer
    p = hm_get_ptr(&hm, 100);
    assert(!p);

    // index lookup
    ptrdiff_t i = hm_get_index(&hm, 3);
    assert(i != -1);
    Point p0 = hm.data[i].value;
    assert(p0.x == 3 && p0.y == 4);

    // get pair pointer and check contents
    KVIntPoint *pair = hm_get_pair_ptr(&hm, 5);
    assert(pair && pair->key == 5 && pair->value.x == 5 && pair->value.y == 6);

    // get pair for non-existent key returns default pair
    KVIntPoint pair1 = hm_get_pair(&hm, 100);
    assert(pair1.key == 0 && pair1.value.x == 0 && pair1.value.y == 0);

    Point p1 = hm_get(&hm, 7);
    assert(p1.x == 7 && p1.y == 8);

    // get for non-existent key returns default pair
    Point p2 = hm_get(&hm, 99);
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        int k = pair->key;
        printf("(key)%d: (Point){%d %d}\n", k,
                pair->value.x, pair->value.y);
    }
    printf("\n");

    // check internal array layout
    assert(migi_mem_eq_single(&hm.data[0], &((KVIntPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVIntPoint){1, ((Point){1, 2})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVIntPoint){3, ((Point){3, 4})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVIntPoint){5, ((Point){5, 6})})));
    assert(migi_mem_eq_single(&hm.data[4], &((KVIntPoint){7, ((Point){7, 8})})));

    // pop a key
    int key_to_delete = 3;
    KVIntPoint deleted = hm_pop(&hm, key_to_delete);
    assert(migi_mem_eq_single(&deleted.key, &((int){3})) && deleted.value.x == 3 && deleted.value.y == 4);

    // after pop, getting the pair for the deleted key should return empty/default
    KVIntPoint t = hm_get_pair(&hm, key_to_delete);
    assertf(migi_mem_eq_single(&t, &(KVIntPoint){0}), "empty returned for deleted keys");

    // check another key still present
    int bla_key = 7;
    KVIntPoint bla = hm_get_pair(&hm, bla_key);
    assert(migi_mem_eq_single(&bla.key, &((int){7})) && bla.value.x == 7 && bla.value.y == 8);

    // delete non-existent key (no-op)
    hm_delete(&hm, 300);

    // replace old value for key==1
    hm_put(&a, &hm, 1, ((Point){10, 20}));

    printf("\niteration:\n");
    hm_foreach(&hm, pair) {
        int k = pair->key;
        printf("(key)%d: (Point){%d %d}\n", k,
                pair->value.x, pair->value.y);
    }

    // final layout checks (matching original expectations)
    assert(migi_mem_eq_single(&hm.data[0], &((KVIntPoint){0})));
    assert(migi_mem_eq_single(&hm.data[1], &((KVIntPoint){1, ((Point){10, 20})})));
    assert(migi_mem_eq_single(&hm.data[2], &((KVIntPoint){7, ((Point){7, 8})})));
    assert(migi_mem_eq_single(&hm.data[3], &((KVIntPoint){5, ((Point){5, 6})})));
}


typedef struct {
    int key, value;
} KVIntInt;

typedef struct {
    HASHMAP_HEADER;
    KVIntInt *data;
} MapIntInt;

void profile_hashmap_iteration(Arena *a, MapIntInt *map, size_t capacity, int64_t cpu_freq, bool print_stats) {
        hm_free(map);
        arena_free(a);

        size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
        if (max_size == 0) max_size = capacity * 2;
        for (size_t i = 0; i < max_size; i++) {
            hm_put(a, map, i, 5025);
        }

        #define SAMPLES 10
        uint64_t samples[SAMPLES] = {0};

        for (size_t i = 0; i < SAMPLES; i++) {
            int key = random_range_exclusive(-max_size, -1); // missing key
            // int key = random_range_exclusive(0, max_size - 1); // valid key
            uint64_t start = read_cpu_timer();

            int value = hm_get(map, key);
            fprintf(stderr, "%d ", value);

            uint64_t end = read_cpu_timer();
            samples[i] = (end - start);
        }
        if (!print_stats) return;

        double elapsed_nanos = 0;
        for (size_t i = 0; i < SAMPLES; i++) {
            double t = (double)samples[i] / (double)cpu_freq;
            elapsed_nanos += t;
        }
        elapsed_nanos /= SAMPLES;
        elapsed_nanos *= NS;
        #undef SAMPLES

        assert(capacity == map->capacity);
        printf("%zu,%f\n", capacity, elapsed_nanos);
}


// NOTE: set HASHMAP_INIT_CAP to 2 (lowest valid capacity) for the best results
void profile_hashmap_lookup_times() {
    Arena a = {0};
    MapIntInt map = {0};

    uint64_t cpu_freq = estimate_cpu_timer_freq();
    size_t max_capacity = 10*MB;

    profile_hashmap_iteration(&a, &map, HASHMAP_INIT_CAP, cpu_freq, false);
    hm_free(&map);

    printf("capacity,valid key lookup time (ns)\n");
    for (size_t i = HASHMAP_INIT_CAP; i <= max_capacity; i*=2) {
        profile_hashmap_iteration(&a, &map, i, cpu_freq, true);
    }
    printf("Load Factor = %.2f\nWith Prefaulting\n", HASHMAP_LOAD_FACTOR);
    assertf(HASHMAP_LOAD_FACTOR*map.capacity == map.size, "hashmap is filled upto load factor");
}


void profile_hashmap_deletion_times() {
    Arena a = {0};
    MapIntInt map = {0};

    uint64_t cpu_freq = estimate_cpu_timer_freq();
    size_t max_capacity = 10*MB;

    printf("capacity,key removal time (ns)\n");

    profile_hashmap_iteration(&a, &map, HASHMAP_INIT_CAP, cpu_freq, false);

    for (size_t capacity = HASHMAP_INIT_CAP; capacity <= max_capacity; capacity*=2) {
        hm_free(&map);
        arena_free(&a);

        size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
        if (max_size == 0) continue;

        for (size_t j = 0; j < max_size; j++) {
            hm_put(&a, &map, j, 1234);
        }

        #define SAMPLES 10
        uint64_t samples[SAMPLES] = {0};

        for (size_t i = 0; i < SAMPLES; i++) {
            int key = random_range_exclusive(0, max_size - 1);
            uint64_t start = read_cpu_timer();
            hm_pop(&map, key);
            uint64_t end = read_cpu_timer();

            avow(!hm_contains(&map, key), "key was deleted");
            samples[i] = (end - start);
            hm_put(&a, &map, key, 1234);
        }

        double elapsed_nanos = 0;
        for (size_t i = 0; i < SAMPLES; i++) {
            double t = (double)samples[i] / (double)cpu_freq;
            elapsed_nanos += t;
        }
        elapsed_nanos /= SAMPLES;
        elapsed_nanos *= NS;
        #undef SAMPLES
        printf("%zu,%f\n", map.capacity, elapsed_nanos);
    }
    printf("Load Factor = %.2f\nWith Prefaulting\n", HASHMAP_LOAD_FACTOR);
}

void profile_huge_strings() {
    typedef struct {
        HASHMAP_HEADER;
        KVStrInt *data;
    } MapStrInt;

    Arena a = {0};
    MapStrInt map = {0};
    begin_profiling();
    for (size_t i = 0; i < 1024; i++) {
        String key = random_string(&a, 1024*1024);
        hms_put(&a, &map, key, 100);
    }
    end_profiling_and_print_stats();
}

int main() {
    // frequency_analysis();
    // profile_hashmap_lookup_times();
    // profile_hashmap_deletion_times();
    profile_search_fail();
    profile_huge_strings();
    // test_small_hashmap_collision();
    // test_basic();
    // test_basic_struct_key();
    // test_basic_primitive_key();
    // test_default_values();
    // test_type_safety();

    printf("\nexiting successfully\n");


    return 0;
}
