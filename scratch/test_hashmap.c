// #define HASHMAP_INIT_CAP 4
// #define HASHMAP_LOAD_FACTOR 0.25
// #define HASHMAP_COLLECT_STATS
#include "hashmap.h"
#include "migi.h"
#include "migi_random.h"
#include "file.h"

void test_basic() {
    typedef struct {
        int x, y;
    } Point;

    Arena *a = arena_init();
    HashMap(Str, Point) hm = {0};

    hashmap_put(a, &hm, S("foo"), ((Point){1, 2}));
    hashmap_put(a, &hm, S("bar"), ((Point){3, 4}));
    hashmap_put(a, &hm, S("baz"), ((Point){5, 6}));

    Point *p = hashmap_at(&hm, S("foo"));
    assert(p->x == 1 && p->y == 2);

    p = hashmap_at(&hm, S("abcd"));
    assert(!p);

    Point *p0 = hashmap_at(&hm, S("bar"));
    assert(p0 != NULL);
    assert(p0->x == 3 && p0->y == 4);

    *hashmap_entry(a, &hm, S("bla")) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, S("bla"));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, S("blah"));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SArg(pair->key), pair->value.x, pair->value.y);
    }
    printf("\n");

    assert(mem_eq(&hm.pairs[0].key, &(Str){0}));    assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &(S("foo"))));  assert(mem_eq(&hm.pairs[1].value, &((Point){1, 2})));
    assert(mem_eq(&hm.pairs[2].key, &(S("bar"))));  assert(mem_eq(&hm.pairs[2].value, &((Point){3, 4})));
    assert(mem_eq(&hm.pairs[3].key, &(S("baz"))));  assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
    assert(mem_eq(&hm.pairs[4].key, &(S("bla"))));  assert(mem_eq(&hm.pairs[4].value, &((Point){7, 8})));

    Point deleted = hashmap_del(&hm, S("bar"));
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, S("bar"));
    assertf(mem_eq(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, S("bla"));
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_del(&hm, S("aaaaa"));
    assertf(mem_eq(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of `foo`
    hashmap_put(a, &hm, S("foo"), ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%.*s: (Point){%d %d}\n", SArg(pair->key), pair->value.x, pair->value.y);
    }
    assert(mem_eq(&hm.pairs[0].key, &((Str){0})));    assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &(S("foo"))));    assert(mem_eq(&hm.pairs[1].value, &((Point){10, 20})));
    assert(mem_eq(&hm.pairs[2].key, &(S("bla"))));    assert(mem_eq(&hm.pairs[2].value, &((Point){7, 8})));
    assert(mem_eq(&hm.pairs[3].key, &(S("baz"))));    assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
}

void test_default_values() {
    typedef struct {
        int x, y;
    } Point;

    Arena *a = arena_init();
    HashMap(Str, Point) hm = {0};

    // Setting default key and value
    hashmap_set_default(a, &hm, S("default"), ((Point){100, 100}));

    hashmap_put(a, &hm, S("foo"), ((Point){1, 2}));
    hashmap_put(a, &hm, S("bar"), ((Point){3, 4}));

    Point p1 = hashmap_get(&hm, S("foo"));
    assert(p1.x == 1 && p1.y == 2);

    p1 = hashmap_get(&hm, S("bar"));
    assert(p1.x == 3 && p1.y == 4);

    Point p2 = hashmap_get(&hm, S("oof!"));
    assert(p2.x == 100 && p2.y == 100);

    Point p3 = hashmap_get(&hm, S("aaaaa"));
    assert(p3.x == 100 && p3.y == 100);
}

typedef struct {
    Str key;
    int64_t value;
} KVStrInt;

typedef HashMap(Str, int64_t) MapStrInt;

int hash_entry_cmp(const void *a, const void *b) {
    return ((KVStrInt *)b)->value - ((KVStrInt *)a)->value;
}

// Counts frequency of occurence of words from a text file
// Regular linear probing performs slightly better here, probably
// due to the overhead of robin hood probing
void frequency_analysis() {
    printf("\n%s\n------------------------------------\n", __FUNCTION__);
    Arena *a = arena_init();

    Str contents = str_from_file(a, S("shakespeare.txt"));
    // read_file(&sb, S("gatsby.txt"));
    // read_file(&sb, S("hashmap_test.txt"));

    MapStrInt map = {0};
    printf("Inserting items:\n");
    begin_profiling();
    strcut_foreach(contents, S(" \n"), Cut_AsChars, it) {
        Str key = str_to_lower(a, str_trim(it.split));
        *hashmap_entry(a, &map, key) += 1;
    }

    printf("size = %zu, capacity = %zu\n", map.size, map.capacity);
    end_profiling_and_print_stats();

    KVStrInt *entries = arena_push(a, KVStrInt, map.size);
    size_t i = 0; hashmap_foreach(&map, pair) {
        entries[i++] = (KVStrInt){.key = pair->key, .value = pair->value};
    }
    qsort(entries, map.size, sizeof(*entries), hash_entry_cmp);

#ifdef ENABLE_PROFILING
    printf("\n\nDeleting items:\n");
    begin_profiling();
    for (size_t i = 0; i <= map.size; i++) {
        KVStrInt *pair = entries + i;
        hashmap_del(&map, pair->key);
    }
    end_profiling_and_print_stats();
#else
    printf("Words sorted in descending order:\n");
    for (size_t i = 0; i < map.size; i++) {
        KVStrInt *pair = entries + i;
        printf("%.*s => %ld\n", SArg(pair->key), pair->value);
    }
#endif
#ifdef HASHMAP_COLLECT_STATS
    printf("Maximum Probe Length: %d\n", map.stats.max_probe_length);
    printf("Total Collisions %d\n", map.stats.total_collisions);
#endif
}

// Define the following before #including hashmap for this to actually test something
// #define HASHMAP_INIT_CAP 4
// #define HASHMAP_LOAD_FACTOR 0.75
void test_small_hashmap_collision() {
    MapStrInt hm = {0};
    Arena *a = arena_init();
    *hashmap_entry(a, &hm, S("abcd")) = 12;
    *hashmap_entry(a, &hm, S("efgh")) = 13;

    assert(hashmap_del(&hm, S("abcd")) == 12);
    assert(hashmap_get(&hm, S("efgh")) == 13);
    assert(hashmap_get(&hm, S("efg")) == 0);
    assert(hashmap_get(&hm, S("abcd")) == 0);

    // Deleting last value in table
    *hashmap_entry(a, &hm, S("abcd")) = 10;
    assert(hashmap_del(&hm, S("abcd")) == 10);
    hashmap_del(&hm, S("abcd"));
    assert(hashmap_get(&hm, S("efgh")) == 13);

    // Popping (with swap-remove) with hashmap at max capacity
    // [3/4 elements] (with load factor >= 0.75, and init capacity 4)
    MapStrInt map = {0};
    hashmap_put(a, &map, S("a"), 1);
    hashmap_put(a, &map, S("b"), 2);
    hashmap_put(a, &map, S("c"), 3);

    assert(hashmap_del(&map, S("a")) == 1);
    assert(hashmap_del(&map, S("b")) == 2);
    assert(hashmap_del(&map, S("c")) == 3);
}


// Uncomment the lines to see the errors when the correct types are not provided
void test_type_safety() {
    typedef struct {
        int x, y;
    } Point;

    typedef HashMap(Str, Point) MapStrPoint;

    Arena *a = arena_init();
    MapStrInt map = {0};
    *hashmap_entry(a, &map, S("abcd")) = 12;

    hashmap_put(a, &map, S("ijkl"), 100);
    // hashmap_put(a, &map, S("ijkl"), ((Point){1, 1}));

    MapStrPoint map2 = {0};
    *hashmap_entry(a, &map2, S("abcd")) = (Point){1, 2};
    // *hashmap_entry(a, &map2, S("abcd")) = 100;

    hashmap_put(a, &map2, S("efgh"), ((Point){3, 4}));
    // hashmap_put(a, &map2, S("efgh"), S("aaaaaaa"));

    Point *p = hashmap_at(&map2, S("abcd"));
    unused(p);
    // int *p1 = hashmap_at(&map2, S("abcd"));

    Point i = hashmap_get(&map2, S("efgh"));
    unused(i);
    // Point ab = hashmap_get(&map, S("a"));
    // int i1 = hashmap_get(&map2, S("efgh"));

    int64_t pair = hashmap_get(&map, S("abcd"));
    unused(pair);
    // Point pair1 = hashmap_get(&map, S("abcd"));

    int64_t *kv = hashmap_at(&map, S("abcd"));
    unused(kv);
    // Point *kv1 = hashmap_at(&map, S("abcd"));

    int64_t del_int = hashmap_del(&map, S("abcd"));
    unused(del_int);
    // Point del_int = hashmap_del(&map, S("abcd"));

    assert(mem_eq(&map.pairs[0].key, &(Str){0}));       assert(mem_eq(&map.pairs[0].value, &(int64_t){0}));
    assert(mem_eq(&map.pairs[1].key, &(S("ijkl"))));    assert(mem_eq(&map.pairs[1].value, &(int64_t){100}));

    assert(mem_eq(&map2.pairs[0].key, &(Str){0}));      assert(mem_eq(&map2.pairs[0].value, &(int64_t){0}));
    assert(mem_eq(&map2.pairs[1].key, &(S("abcd"))));   assert(mem_eq(&map2.pairs[1].value, &((Point){1, 2})));
    assert(mem_eq(&map2.pairs[2].key, &(S("efgh"))));   assert(mem_eq(&map2.pairs[2].value, &((Point){3, 4})));

    hashmap_foreach(&map, pair) {
        printf("%.*s: %ld", SArg(pair->key), pair->value);
    }
    printf("\n");
    hashmap_foreach(&map2, pair) {
        printf("%.*s: (Point){%d, %d}\n", SArg(pair->key), pair->value.x, pair->value.y);
    }
    printf("\n");
}

Str random_string(Arena *a, size_t length) {
    char *chars = arena_push(a, char, length);

    for (size_t i = 0; i < length; i++) {
        chars[i] = rand_range('a', 'z');
    }
    return (Str){.data = chars, .length = length};
}

// Inserts random strings into the hashmap and tries to later find them
// Since the strings are random, most of the searches will fail
// This is extremely slow on a regular linear probing due to having to search
// the entire table and then failing.
// The robin hood linear probing approach is MUCH faster in this case
void profile_search_fail() {
    printf("\n%s\n------------------------------------\n", __FUNCTION__);
    Arena *a = arena_init();
    MapStrInt map = {0};

    begin_profiling();
    size_t length = 5;
    for (size_t i = 0; i < 1024 * 1024; i++) {
        Str str = random_string(a, length);
        *hashmap_entry(a, &map, str) = 1;
    }
    end_profiling_and_print_stats();

    begin_profiling();
    size_t count = 0;
    for (size_t i = 0; i < 1024 * 1024; i++) {
        Str str = random_string(a, length);
        if (hashmap_at(&map, str)) {
            count++;
        }
    }
    end_profiling_and_print_stats();

    printf("Matched elements: %zu\n", count);
    printf("Unmatched elements: %zu\n", map.size - count);

#ifdef HASHMAP_COLLECT_STATS
    printf("Maximum Probe Length: %d\n", map.stats.max_probe_length);
    printf("Total Collisions %d\n", map.stats.total_collisions);
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

    Arena *a = arena_init();
    HashMap(Foo, Point) hm = {0};

    hashmap_put(a, &hm, ((Foo){1, 2, 1.0f}), ((Point){1, 2}));
    hashmap_put(a, &hm, ((Foo){3, 4, 2.0f}), ((Point){3, 4}));
    hashmap_put(a, &hm, ((Foo){5, 6, 3.0f}), ((Point){5, 6}));

    Point *p = hashmap_at(&hm, ((Foo){1, 2, 1.0f}));
    assert(p->x == 1 && p->y == 2);

    p = hashmap_at(&hm, ((Foo){9, 9, 9.0f}));
    assert(!p);

    Point *p0 = hashmap_at(&hm, ((Foo){3, 4, 2.0f}));
    assert(p0 != NULL);
    assert(p0->x == 3 && p0->y == 4);

    *hashmap_entry(a, &hm, ((Foo){7, 8, 4.0f})) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, ((Foo){7, 8, 4.0f}));
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, ((Foo){10, 11, 5.0f}));
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("(Foo){%d,%d,%.2f}: (Point){%d %d}\n",
               pair->key.a, pair->key.b, pair->key.f,
               pair->value.x, pair->value.y);
    }
    printf("\n");

    assert(mem_eq(&hm.pairs[0].key, &((Foo){0})));           assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &((Foo){1, 2, 1.0f})));  assert(mem_eq(&hm.pairs[1].value, &((Point){1, 2})));
    assert(mem_eq(&hm.pairs[2].key, &((Foo){3, 4, 2.0f})));  assert(mem_eq(&hm.pairs[2].value, &((Point){3, 4})));
    assert(mem_eq(&hm.pairs[3].key, &((Foo){5, 6, 3.0f})));  assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
    assert(mem_eq(&hm.pairs[4].key, &((Foo){7, 8, 4.0f})));  assert(mem_eq(&hm.pairs[4].value, &((Point){7, 8})));

    Point deleted = hashmap_del(&hm, ((Foo){3, 4, 2.0f}));
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, ((Foo){3, 4, 2.0f}));
    assertf(mem_eq(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, ((Foo){7, 8, 4.0f}));
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_del(&hm, ((Foo){99, 99, 9.9f}));
    assertf(mem_eq(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of `(Foo){1, 2, 1.0f}`
    hashmap_put(a, &hm, ((Foo){1, 2, 1.0f}), ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("(Foo){%d,%d,%.2f}: (Point){%d %d}\n",
               pair->key.a, pair->key.b, pair->key.f,
               pair->value.x, pair->value.y);
    }
    assert(mem_eq(&hm.pairs[0].key, &((Foo){0})));             assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &((Foo){1, 2, 1.0f})));    assert(mem_eq(&hm.pairs[1].value, &((Point){10, 20})));
    assert(mem_eq(&hm.pairs[2].key, &((Foo){7, 8, 4.0f})));    assert(mem_eq(&hm.pairs[2].value, &((Point){7, 8})));
    assert(mem_eq(&hm.pairs[3].key, &((Foo){5, 6, 3.0f})));    assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
}


void test_basic_primitive_key() {
    typedef struct {
        int x, y;
    } Point;

    Arena *a = arena_init();
    HashMap(int, Point) hm = {0};

    hashmap_put(a, &hm, 1, ((Point){1, 2}));
    hashmap_put(a, &hm, 2, ((Point){3, 4}));
    hashmap_put(a, &hm, 3, ((Point){5, 6}));

    Point *p = hashmap_at(&hm, 1);
    assert(p->x == 1 && p->y == 2);

    p = hashmap_at(&hm, 999);
    assert(!p);

    Point *p0 = hashmap_at(&hm, 2);
    assert(p0 != NULL);
    assert(p0->x == 3 && p0->y == 4);

    *hashmap_entry(a, &hm, 4) = (Point){7, 8};
    Point p1 = hashmap_get(&hm, 4);
    assert(p1.x == 7 && p1.y == 8);

    Point p2 = hashmap_get(&hm, 42);
    assert(p2.x == 0 && p2.y == 0);

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%d: (Point){%d %d}\n", pair->key, pair->value.x, pair->value.y);
    }
    printf("\n");

    assert(mem_eq(&hm.pairs[0].key, &(int){0}));   assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &(int){1}));   assert(mem_eq(&hm.pairs[1].value, &((Point){1, 2})));
    assert(mem_eq(&hm.pairs[2].key, &(int){2}));   assert(mem_eq(&hm.pairs[2].value, &((Point){3, 4})));
    assert(mem_eq(&hm.pairs[3].key, &(int){3}));   assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
    assert(mem_eq(&hm.pairs[4].key, &(int){4}));   assert(mem_eq(&hm.pairs[4].value, &((Point){7, 8})));

    Point deleted = hashmap_del(&hm, 2);
    assert(deleted.x == 3 && deleted.y == 4);

    Point t = hashmap_get(&hm, 2);
    assertf(mem_eq(&t, &(Point){0}), "empty returned for deleted keys");

    Point bla = hashmap_get(&hm, 4);
    assert(bla.x == 7 && bla.y == 8);

    Point x = hashmap_del(&hm, 999);
    assertf(mem_eq(&x, &(Point){0}), "empty returned for non-existent keys");

    // replacing old value of key=1
    hashmap_put(a, &hm, 1, ((Point){10, 20}));

    printf("\niteration:\n");
    hashmap_foreach(&hm, pair) {
        printf("%d: (Point){%d %d}\n", pair->key, pair->value.x, pair->value.y);
    }
    assert(mem_eq(&hm.pairs[0].key, &(int){0}));   assert(mem_eq(&hm.pairs[0].value, &((Point){0})));
    assert(mem_eq(&hm.pairs[1].key, &(int){1}));   assert(mem_eq(&hm.pairs[1].value, &((Point){10, 20})));
    assert(mem_eq(&hm.pairs[2].key, &(int){4}));   assert(mem_eq(&hm.pairs[2].value, &((Point){7, 8})));
    assert(mem_eq(&hm.pairs[3].key, &(int){3}));   assert(mem_eq(&hm.pairs[3].value, &((Point){5, 6})));
}



typedef HashMap(int, int) MapIntInt;

void profile_hashmap_iteration(Arena *a, MapIntInt *map, size_t capacity, int64_t cpu_freq, bool print_stats) {
    size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
    if (max_size == 0) max_size = capacity * 2;
    for (size_t i = 0; i < max_size; i++) {
        hashmap_put(a, map, i, 5025);
    }

#define SAMPLES 10
    uint64_t samples[SAMPLES] = {0};

    for (size_t i = 0; i < SAMPLES; i++) {
        int key = rand_range_exclusive(-max_size, 0); // missing key
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
    printf("\n%s\n------------------------------------\n", __FUNCTION__);
    Arena *a = arena_init();
    MapIntInt map = {0};

    uint64_t cpu_freq = estimate_cpu_timer_freq();
    size_t max_capacity = 1*MB;

    profile_hashmap_iteration(a, &map, HASHMAP_INIT_CAP, cpu_freq, false);

    printf("capacity,missing key lookup time (ns)\n");
    for (size_t i = HASHMAP_INIT_CAP; i <= max_capacity; i*=2) {
        hashmap_free(&map);
        arena_reset(a);
        profile_hashmap_iteration(a, &map, i, cpu_freq, true);
    }
    printf("Load Factor = %.2f\nWith Prefaulting\n", HASHMAP_LOAD_FACTOR);
    assertf(HASHMAP_LOAD_FACTOR*map.capacity == map.size, "hashmap is filled upto load factor");
#ifdef HASHMAP_COLLECT_STATS
    printf("Maximum Probe Length: %d\n", map.stats.max_probe_length);
    printf("Total Collisions %d\n", map.stats.total_collisions);
#endif
    arena_free(a);
}


void profile_hashmap_deletion_times() {
    printf("\n%s\n------------------------------------\n", __FUNCTION__);
    Arena *a = arena_init(.type = Arena_Chained);
    MapIntInt map = {0};

    uint64_t cpu_freq = estimate_cpu_timer_freq();
    size_t max_capacity = 1*MB;

    printf("capacity,key removal time (ns)\n");

    profile_hashmap_iteration(a, &map, HASHMAP_INIT_CAP, cpu_freq, false);

    for (size_t capacity = HASHMAP_INIT_CAP; capacity <= max_capacity; capacity*=2) {
        hashmap_free(&map);
        arena_reset(a);

        size_t max_size = HASHMAP_LOAD_FACTOR * capacity;
        if (max_size == 0) continue;

        for (size_t j = 0; j < max_size; j++) {
            hashmap_put(a, &map, j, 1234);
        }

#define SAMPLES 10
        uint64_t samples[SAMPLES] = {0};

        for (size_t i = 0; i < SAMPLES; i++) {
            int key = rand_range_exclusive(0, max_size);
            uint64_t start = read_cpu_timer();
            hashmap_del(&map, key);
            uint64_t end = read_cpu_timer();

            avow(hashmap_at(&map, key) == NULL, "key was deleted");
            samples[i] = (end - start);
            hashmap_put(a, &map, key, 1234);
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
#ifdef HASHMAP_COLLECT_STATS
    printf("Maximum Probe Length: %d\n", map.stats.max_probe_length);
    printf("Total Collisions %d\n", map.stats.total_collisions);
#endif
    arena_free(a);
}

void profile_huge_strings() {
    printf("\n%s\n------------------------------------\n", __FUNCTION__);
    Arena *a = arena_init(.reserve_size = 8*GB);
    HashMap(Str, int64_t) map = {0};
    begin_profiling();
    for (size_t i = 0; i < 1024; i++) {
        Str key = random_string(a, 1024*1024);
        hashmap_put(a, &map, key, 100);
    }
    end_profiling_and_print_stats();
#ifdef HASHMAP_COLLECT_STATS
    printf("Maximum Probe Length: %d\n", map.stats.max_probe_length);
    printf("Total Collisions %d\n", map.stats.total_collisions);
#endif
}

typedef HashMap(Str, int) StrMap;
static void put_strings(Arena *a, Arena *str_arena, StrMap *map, size_t n) {
    for (size_t i = 0; i < n; i++) {
        Str rand_str = random_string(str_arena, 16);
        hashmap_put(a, map, rand_str, (int)rand_range(0, 10000));
    }
}

static void test_reserve() {
    Arena *a = arena_init();
    Arena *str_arena = arena_init();

    size_t n = 1000;
    for (size_t i = 0; i < 2; i++) {
        StrMap map = {0};

        if (i == 1) hashmap_reserve(a, &map, n);
        size_t prev_capacity = map.capacity;

        put_strings(a, str_arena, &map, n);

        printf("%s:\n", i == 1? "WITH RESERVE": "WITHOUT RESERVE");
        printf("size: %zu\n", map.size);
        printf("capacity: %zu\n", map.capacity);
        printf("arena allocated: %zu\n\n", a->position);
        if (i == 1) assertf(map.capacity == prev_capacity, "expected `%zu` but got `%zu`", prev_capacity, map.capacity);
        arena_reset(str_arena);
        arena_reset(a);
    }

    arena_free(a);
    arena_free(str_arena);
}


typedef struct {
    int x, y;
} Point;

uint64_t hash_point(void *data, size_t size) {
    unused(size);
    Point *p = data;
    return (size_t)p->x * 1000003 + p->y;
}

bool eq_point(void *a, void *b, size_t size) {
    unused(size);
    Point *p1 = a;
    Point *p2 = b;
    return p1->x == p2->x && p1->y == p2->y;
}

void test_custom_hash() {

    Arena *a = arena_init();
    HashMap(Point, Str) map = {
        .hash_fn = hash_point,
        .eq_fn = eq_point,
    };

    hashmap_put(a, &map, ((Point){1, 2}),    S("1, 2"));
    hashmap_put(a, &map, ((Point){10, 45}),  S("10, 45"));
    hashmap_put(a, &map, ((Point){50, 32}),  S("50, 32"));
    hashmap_put(a, &map, ((Point){5, 13}),   S("5, 13"));

    assert(mem_eq(&map.pairs[0].key, &((Point){0})));      assert(str_eq(map.pairs[0].value, S("")));
    assert(mem_eq(&map.pairs[1].key, &((Point){1, 2})));   assert(str_eq(map.pairs[1].value, S("1, 2")));
    assert(mem_eq(&map.pairs[2].key, &((Point){10, 45}))); assert(str_eq(map.pairs[2].value, S("10, 45")));
    assert(mem_eq(&map.pairs[3].key, &((Point){50, 32}))); assert(str_eq(map.pairs[3].value, S("50, 32")));
    assert(mem_eq(&map.pairs[4].key, &((Point){5, 13})));  assert(str_eq(map.pairs[4].value, S("5, 13")));
}

void test_cstr_key() {
    Arena *a = arena_init();
    HashMap(char *, int) map = {0};

    hashmap_put(a, &map, "foo", 100);
    hashmap_put(a, &map, "bar", 149);
    hashmap_put(a, &map, "baz", -49);
    hashmap_put(a, &map, "buzz", 0);

    assert(str_eq_cstr(S(""),     map.pairs[0].key, 0)); assert(map.pairs[0].value == 0);
    assert(str_eq_cstr(S("foo"),  map.pairs[1].key, 0)); assert(map.pairs[1].value == 100);
    assert(str_eq_cstr(S("bar"),  map.pairs[2].key, 0)); assert(map.pairs[2].value == 149);
    assert(str_eq_cstr(S("baz"),  map.pairs[3].key, 0)); assert(map.pairs[3].value == -49);
    assert(str_eq_cstr(S("buzz"), map.pairs[4].key, 0)); assert(map.pairs[4].value == 0);
}

int main() {
    // frequency_analysis();
    // profile_hashmap_lookup_times();
    // profile_hashmap_deletion_times();
    // profile_search_fail();
    // profile_huge_strings();
    test_small_hashmap_collision();
    test_basic();
    test_basic_struct_key();
    test_basic_primitive_key();
    test_default_values();
    test_type_safety();
    test_reserve();
    test_custom_hash();
    test_cstr_key();


    printf("\nexiting successfully\n");

    return 0;
}


