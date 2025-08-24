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
#include "migi_random.h"
#include "migi_string.h"


void test_basic() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        HASHMAP_HEADER;
        String *keys;
        Point *values;
    } MapStrPoint;

    Arena a = {0};
    MapStrPoint hm = {0};

    hashmap_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hashmap_put(&a, &hm, SV("bar"), ((Point){3, 4}));
    hashmap_put(&a, &hm, SV("baz"), ((Point){5, 6}));

    Point *p = hashmap_get_ptr(&hm, SV("foo"));
    assert(p->x == 1 && p->y == 2);

    p = hashmap_get_ptr(&hm, SV("abcd"));
    assert(!p);

    ptrdiff_t i = hashmap_get_index(&hm, SV("bar"));
    assert(i != -1);
    Point p0 = hm.values[i];
    assert(p0.x == 3 && p0.y == 4);

    *hashmap_entry(&a, &hm, SV("bla")) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, SV("bla"));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, SV("blah"));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(*pair.key), pair.value->x,
               pair.value->y);
    }
    printf("\n");

    assert(mem_eq_single(&hm.keys[0], &(String){0}));  assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &(SV("foo"))));  assert(mem_eq_single(&hm.values[1], &((Point){1, 2})));
    assert(mem_eq_single(&hm.keys[2], &(SV("bar"))));  assert(mem_eq_single(&hm.values[2], &((Point){3, 4})));
    assert(mem_eq_single(&hm.keys[3], &(SV("baz"))));  assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
    assert(mem_eq_single(&hm.keys[4], &(SV("bla"))));  assert(mem_eq_single(&hm.values[4], &((Point){7, 8})));

    Point deleted = hashmap_pop(&hm, SV("bar"));
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, SV("bar"));
    assertf(mem_eq_single(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, SV("bla"));
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_pop(&hm, SV("aaaaa"));
    assertf(mem_eq_single(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of `foo`
    hashmap_put(&a, &hm, SV("foo"), ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(*pair.key), pair.value->x, pair.value->y);
    }
    assert(mem_eq_single(&hm.keys[0], &((String){0})));  assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &(SV("foo"))));    assert(mem_eq_single(&hm.values[1], &((Point){10, 20})));
    assert(mem_eq_single(&hm.keys[2], &(SV("bla"))));    assert(mem_eq_single(&hm.values[2], &((Point){7, 8})));
    assert(mem_eq_single(&hm.keys[3], &(SV("baz"))));    assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
}

void test_default_values() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        HASHMAP_HEADER;
        String *keys;
        Point *values;
    } MapStrPoint;

    Arena a = {0};
    MapStrPoint hm = {0};

    hashmap_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hashmap_put(&a, &hm, SV("bar"), ((Point){3, 4}));

    // Setting default key and value
    // NOTE: This can only be done after atleast 1 insertion into the hashmap
    hm.keys[HASHMAP_DEFAULT_INDEX] = SV("default");
    hm.values[HASHMAP_DEFAULT_INDEX] = (Point){100, 100};

    Point p1 = hashmap_get(&hm, SV("foo"));
    assert(p1.x == 1 && p1.y == 2);

    p1 = hashmap_get(&hm, SV("bar"));
    assert(p1.x == 3 && p1.y == 4);

    Point p2 = hashmap_get(&hm, SV("oof!"));
    assert(p2.x == 100 && p2.y == 100);

    Point p3 = hashmap_get(&hm, SV("aaaaa"));
    assert(p3.x == 100 && p3.y == 100);
}

typedef struct {
    String key;
    int64_t value;
} KVStrInt;

typedef struct {
    HASHMAP_HEADER;
    String *keys;
    int64_t *values;
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
    MapStrInt map = {0};
    printf("Inserting items:\n");
    begin_profiling();
    string_split_chars_foreach(contents, SV(" \n"), it) {
        String key = string_to_lower(&a, string_trim(it.split));
        *hashmap_entry(&a, &map, key) += 1;
    }

    printf("size = %zu, capacity = %zu\n", map.size, map.capacity);
    end_profiling_and_print_stats();

    KVStrInt *entries = arena_push(&a, KVStrInt, map.size);
    size_t i = 0; hashmap_foreach(&map, pair) {
        entries[i++] = (KVStrInt){.key = *pair.key, .value = *pair.value};
    }
    qsort(entries, map.size, sizeof(*entries), hash_entry_cmp);

#ifdef ENABLE_PROFILING
    printf("\n\nDeleting items:\n");
    begin_profiling();
    for (size_t i = 0; i <= map.size; i++) {
        KVStrInt *pair = entries + i;
        hashmap_pop(&map, pair->key);
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
    *hashmap_entry(&a, &hm, SV("abcd")) = 12;
    *hashmap_entry(&a, &hm, SV("efgh")) = 13;

    assert(hashmap_pop(&hm, SV("abcd")) == 12);
    assert(hashmap_get(&hm, SV("efgh")) == 13);
    assert(hashmap_get(&hm, SV("efg")) == 0);
    assert(hashmap_get(&hm, SV("abcd")) == 0);

    // Deleting last value in table
    *hashmap_entry(&a, &hm, SV("abcd")) = 10;
    assert(hashmap_pop(&hm, SV("abcd")) == 10);
    hashmap_pop(&hm, SV("abcd"));
    assert(hashmap_get(&hm, SV("efgh")) == 13);

    // Popping (with swap-remove) with hashmap at max capacity
    // [3/4 elements] (with load factor >= 0.75, and init capacity 4)
    MapStrInt map = {0};
    hashmap_put(&a, &map, SV("a"), 1);
    hashmap_put(&a, &map, SV("b"), 2);
    hashmap_put(&a, &map, SV("c"), 3);

    assert(hashmap_pop(&map, SV("a")) == 1);
    assert(hashmap_pop(&map, SV("b")) == 2);
    assert(hashmap_pop(&map, SV("c")) == 3);
}


void test_type_safety() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        HASHMAP_HEADER;
        String *keys;
        Point *values;
    } MapStrPoint;

    Arena a = {0};
    MapStrInt map = {0};
    *hashmap_entry(&a, &map, SV("abcd")) = 12;

    hashmap_put(&a, &map, SV("ijkl"), 100);
    // hashmap_put(&a, &map, SV("ijkl"), ((Point){1, 1}));

    MapStrPoint map2 = {0};
    *hashmap_entry(&a, &map2, SV("abcd")) = (Point){1, 2};
    // *hashmap_entry(&a, &map2, SV("abcd")) = 100;

    hashmap_put(&a, &map2, SV("efgh"), ((Point){3, 4}));
    // hashmap_put(&a, &map2, SV("efgh"), SV("aaaaaaa"));

    Point *p = hashmap_get_ptr(&map2, SV("abcd"));
    unused(p);
    // int *p1 = hashmap_get_ptr(&map2, SV("abcd"));

    Point i = hashmap_get(&map2, SV("efgh"));
    unused(i);
    // Point ab = hashmap_get(&map, SV("a"));
    // int i1 = hashmap_get(&map2, SV("efgh"));

    int64_t pair = hashmap_get(&map, SV("abcd"));
    unused(pair);
    // Point pair1 = hashmap_get(&map, SV("abcd"));

    int64_t *kv = hashmap_get_ptr(&map, SV("abcd"));
    unused(kv);
    // Point *kv1 = hashmap_get_ptr(&map, SV("abcd"));

    int64_t del_int = hashmap_pop(&map, SV("abcd"));
    unused(del_int);
    // Point del_int = hashmap_pop(&map, SV("abcd"));

    assert(mem_eq_single(&map.keys[0], &(String){0}));    assert(mem_eq_single(&map.values[0], &(int64_t){0}));
    assert(mem_eq_single(&map.keys[1], &(SV("ijkl"))));   assert(mem_eq_single(&map.values[1], &(int64_t){100}));

    assert(mem_eq_single(&map2.keys[0], &(String){0}));    assert(mem_eq_single(&map2.values[0], &(int64_t){0}));
    assert(mem_eq_single(&map2.keys[1], &(SV("abcd"))));   assert(mem_eq_single(&map2.values[1], &((Point){1, 2})));
    assert(mem_eq_single(&map2.keys[2], &(SV("efgh"))));   assert(mem_eq_single(&map2.values[2], &((Point){3, 4})));

    hashmap_foreach(&map, pair) {
        printf("%.*s: %ld", SV_FMT(*pair.key), *pair.value);
    }
    printf("\n");
    hashmap_foreach(&map2, pair) {
        printf("%.*s: (Point){%d, %d}\n", SV_FMT(*pair.key), pair.value->x, pair.value->y);
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
        *hashmap_entry(&a, &map, str) = 1;
    }
    end_profiling_and_print_stats();

    begin_profiling();
    size_t count = 0;
    for (size_t i = 0; i < 1024 * 1024; i++) {
        String str = random_string(&a, length);
        if (hashmap_get_index(&map, str) != -1) {
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
        HASHMAP_HEADER;
        Foo   *keys;
        Point *values;
    } MapFooPoint;

    Arena a = {0};
    MapFooPoint hm = {0};

    hashmap_put(&a, &hm, ((Foo){1, 2, 1.0f}), ((Point){1, 2}));
    hashmap_put(&a, &hm, ((Foo){3, 4, 2.0f}), ((Point){3, 4}));
    hashmap_put(&a, &hm, ((Foo){5, 6, 3.0f}), ((Point){5, 6}));

    Point *p = hashmap_get_ptr(&hm, ((Foo){1, 2, 1.0f}));
    assert(p->x == 1 && p->y == 2);

    p = hashmap_get_ptr(&hm, ((Foo){9, 9, 9.0f}));
    assert(!p);

    ptrdiff_t i = hashmap_get_index(&hm, ((Foo){3, 4, 2.0f}));
    assert(i != -1);
    Point p0 = hm.values[i];
    assert(p0.x == 3 && p0.y == 4);

    *hashmap_entry(&a, &hm, ((Foo){7, 8, 4.0f})) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, ((Foo){7, 8, 4.0f}));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, ((Foo){10, 11, 5.0f}));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("(Foo){%d,%d,%.2f}: (Point){%d %d}\n",
               pair.key->a, pair.key->b, pair.key->f,
               pair.value->x, pair.value->y);
    }
    printf("\n");

    assert(mem_eq_single(&hm.keys[0], &((Foo){0})));           assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &((Foo){1, 2, 1.0f})));  assert(mem_eq_single(&hm.values[1], &((Point){1, 2})));
    assert(mem_eq_single(&hm.keys[2], &((Foo){3, 4, 2.0f})));  assert(mem_eq_single(&hm.values[2], &((Point){3, 4})));
    assert(mem_eq_single(&hm.keys[3], &((Foo){5, 6, 3.0f})));  assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
    assert(mem_eq_single(&hm.keys[4], &((Foo){7, 8, 4.0f})));  assert(mem_eq_single(&hm.values[4], &((Point){7, 8})));

    Point deleted = hashmap_pop(&hm, ((Foo){3, 4, 2.0f}));
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, ((Foo){3, 4, 2.0f}));
    assertf(mem_eq_single(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, ((Foo){7, 8, 4.0f}));
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_pop(&hm, ((Foo){99, 99, 9.9f}));
    assertf(mem_eq_single(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of `(Foo){1, 2, 1.0f}`
    hashmap_put(&a, &hm, ((Foo){1, 2, 1.0f}), ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("(Foo){%d,%d,%.2f}: (Point){%d %d}\n",
               pair.key->a, pair.key->b, pair.key->f,
               pair.value->x, pair.value->y);
    }
    assert(mem_eq_single(&hm.keys[0], &((Foo){0})));             assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &((Foo){1, 2, 1.0f})));    assert(mem_eq_single(&hm.values[1], &((Point){10, 20})));
    assert(mem_eq_single(&hm.keys[2], &((Foo){7, 8, 4.0f})));    assert(mem_eq_single(&hm.values[2], &((Point){7, 8})));
    assert(mem_eq_single(&hm.keys[3], &((Foo){5, 6, 3.0f})));    assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
}


void test_basic_primitive_key() {
    typedef struct {
        int x, y;
    } Point;

    typedef struct {
        HASHMAP_HEADER;
        int   *keys;
        Point *values;
    } MapIntPoint;

    Arena a = {0};
    MapIntPoint hm = {0};

    hashmap_put(&a, &hm, 1, ((Point){1, 2}));
    hashmap_put(&a, &hm, 2, ((Point){3, 4}));
    hashmap_put(&a, &hm, 3, ((Point){5, 6}));

    Point *p = hashmap_get_ptr(&hm, 1);
    assert(p->x == 1 && p->y == 2);

    p = hashmap_get_ptr(&hm, 999);
    assert(!p);

    ptrdiff_t i = hashmap_get_index(&hm, 2);
    assert(i != -1);
    Point p0 = hm.values[i];
    assert(p0.x == 3 && p0.y == 4);

    *hashmap_entry(&a, &hm, 4) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, 4);
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, 42);
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%d: (Point){%d %d}\n", *pair.key, pair.value->x, pair.value->y);
    }
    printf("\n");

    assert(mem_eq_single(&hm.keys[0], &(int){0}));   assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &(int){1}));   assert(mem_eq_single(&hm.values[1], &((Point){1, 2})));
    assert(mem_eq_single(&hm.keys[2], &(int){2}));   assert(mem_eq_single(&hm.values[2], &((Point){3, 4})));
    assert(mem_eq_single(&hm.keys[3], &(int){3}));   assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
    assert(mem_eq_single(&hm.keys[4], &(int){4}));   assert(mem_eq_single(&hm.values[4], &((Point){7, 8})));

    Point deleted = hashmap_pop(&hm, 2);
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, 2);
    assertf(mem_eq_single(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, 4);
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_pop(&hm, 999);
    assertf(mem_eq_single(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of key=1
    hashmap_put(&a, &hm, 1, ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%d: (Point){%d %d}\n", *pair.key, pair.value->x, pair.value->y);
    }
    assert(mem_eq_single(&hm.keys[0], &(int){0}));   assert(mem_eq_single(&hm.values[0], &((Point){0})));
    assert(mem_eq_single(&hm.keys[1], &(int){1}));   assert(mem_eq_single(&hm.values[1], &((Point){10, 20})));
    assert(mem_eq_single(&hm.keys[2], &(int){4}));   assert(mem_eq_single(&hm.values[2], &((Point){7, 8})));
    assert(mem_eq_single(&hm.keys[3], &(int){3}));   assert(mem_eq_single(&hm.values[3], &((Point){5, 6})));
}



typedef struct {
    HASHMAP_HEADER;
    int *keys;
    int *values;
} MapIntInt;

void profile_hashmap_iteration(Arena *a, MapIntInt *map, size_t capacity, int64_t cpu_freq, bool print_stats) {
    size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
    if (max_size == 0) max_size = capacity * 2;
    for (size_t i = 0; i < max_size; i++) {
        hashmap_put(a, map, i, 5025);
    }

#define SAMPLES 10
    uint64_t samples[SAMPLES] = {0};

    for (size_t i = 0; i < SAMPLES; i++) {
        int key = random_range_exclusive(-max_size, 0); // missing key
                                                        // int key = random_range_exclusive(0, max_size); // valid key
        uint64_t start = read_cpu_timer();

        int value = hashmap_get(map, key);
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
    size_t max_capacity = 100*MB;

    profile_hashmap_iteration(&a, &map, HASHMAP_INIT_CAP, cpu_freq, false);

    printf("capacity,missing key lookup time (ns)\n");
    for (size_t i = HASHMAP_INIT_CAP; i <= max_capacity; i*=2) {
        hashmap_free(&map);
        arena_free(&a);
        profile_hashmap_iteration(&a, &map, i, cpu_freq, true);
    }
    printf("Load Factor = %.2f\nWith Prefaulting\n", HASHMAP_LOAD_FACTOR);
    assertf(HASHMAP_LOAD_FACTOR*map.capacity == map.size, "hashmap is filled upto load factor");
    arena_free(&a);
}


void profile_hashmap_deletion_times() {
    Arena a = {0};
    MapIntInt map = {0};

    uint64_t cpu_freq = estimate_cpu_timer_freq();
    size_t max_capacity = 100*MB;

    printf("capacity,key removal time (ns)\n");

    profile_hashmap_iteration(&a, &map, HASHMAP_INIT_CAP, cpu_freq, false);

    for (size_t capacity = HASHMAP_INIT_CAP; capacity <= max_capacity; capacity*=2) {
        hashmap_free(&map);
        arena_free(&a);

        size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
        if (max_size == 0) continue;

        for (size_t j = 0; j < max_size; j++) {
            hashmap_put(&a, &map, j, 1234);
        }

#define SAMPLES 10
        uint64_t samples[SAMPLES] = {0};

        for (size_t i = 0; i < SAMPLES; i++) {
            int key = random_range_exclusive(0, max_size);
            uint64_t start = read_cpu_timer();
            hashmap_pop(&map, key);
            uint64_t end = read_cpu_timer();

            avow(hashmap_get_index(&map, key) == -1, "key was deleted");
            samples[i] = (end - start);
            hashmap_put(&a, &map, key, 1234);
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
    arena_free(&a);
}

void profile_huge_strings() {
    typedef struct {
        HASHMAP_HEADER;
        String *keys;
        int64_t *values;
    } MapStrInt;

    Arena a = {0};
    MapStrInt map = {0};
    begin_profiling();
    for (size_t i = 0; i < 1024; i++) {
        String key = random_string(&a, 1024*1024);
        hashmap_put(&a, &map, key, 100);
    }
    end_profiling_and_print_stats();
}

void test_reserve() {
    typedef struct {
        HASHMAP_HEADER;
        String *keys;
        int *values;
    } Map;

    Arena a = {0};
    Map map = {0};
    hashmap_reserve(&a, &map, 500);

    size_t capacity = map.capacity;
    for (size_t i = 0; i < 500; i++) {
        hashmap_put(&a, &map, SV("a"), i);
        assertf(map.capacity == capacity, "expected `%zu` but got `%zu`", capacity, map.capacity);
    }
}

int main() {
    frequency_analysis();
    // profile_hashmap_lookup_times();
    // profile_hashmap_deletion_times();
    // profile_search_fail();
    // profile_huge_strings();
    // test_small_hashmap_collision();
    // test_basic();
    // test_basic_struct_key();
    // test_basic_primitive_key();
    // test_default_values();
    // test_type_safety();
    // test_reserve();

    printf("\nexiting successfully\n");

    return 0;
}


