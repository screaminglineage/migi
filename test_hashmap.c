#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PROFILER_H_IMPLEMENTATION
// #define ENABLE_PROFILING
#include "profiler.h"

#include "hashmap.h"
#include "arena.h"
#include "migi.h"
#include "migi_lists.h"
#include "migi_string.h"


typedef struct {
    int x, y;
} Point;

typedef struct {
    String key;
    Point value;
} KVStrPoint;

typedef struct {
    HASHMAP_HEADER;

    // only needed for non-string hashmaps
    // bool (*hm_key_eq)(Key a, Key b);
    KVStrPoint *data;
} MapStrPoint;

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

    KVStrPoint pair1 = hms_get_pair(&hm, SV("bazz"), KVStrPoint );
    printf("`%.*s`: (Point){%d %d}\n", SV_FMT(pair1.key), pair1.value.x,
           pair1.value.y);

    Point p1 = hms_get(&hm, SV("bla"), Point);
    printf("bla: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, SV("blah"), Point);
    printf("EMPTY: (Point){%d %d}\n", p2.x, p2.y);

    printf("\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
               pair->value.y);
    }

    KVStrPoint deleted = hms_pop(&hm, SV("bar"), KVStrPoint);
    printf("Deleted: %.*s: (Point){%d %d}\n", SV_FMT(deleted.key),
           deleted.value.x, deleted.value.y);

    assertf(migi_mem_eq(&hms_get_pair(&hm, SV("bar"), KVStrPoint),
                        &(KVStrPoint){0}, sizeof(KVStrPoint)),
            "empty returned for deleted keys");

    KVStrPoint bla = hms_get_pair(&hm, SV("bla"), KVStrPoint);
    printf("%.*s: (Point){%d %d}\n", SV_FMT(bla.key), bla.value.x, bla.value.y);

    printf("\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x,
               pair->value.y);
    }

    hms_del(&hm, SV("aaaaa"));
}

void test_default_values() {
    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));

    // Setting default key and value
    // NOTE: This can only be done after atleast 1 insertion into the hashmap
    hm.data->key = SV("default");
    hm.data->value = (Point){100, 100};

    Point p1 = hms_get(&hm, SV("foo"), Point);
    printf("foo: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, SV("oof!"), Point);
    printf("oof!: (Point){%d %d}\n", p2.x, p2.y);

    KVStrPoint p3 = hms_get_pair(&hm, SV("aaaaa"), KVStrPoint);
    printf("%.*s: (Point){%d %d}\n", SV_FMT(p3.key), p3.value.x, p3.value.y);
}

typedef struct {
    String key;
    int64_t value;
} KVStrInt;

typedef struct {
    HASHMAP_HEADER;

    KVStrInt *data;
} MapStrInt;

int hash_entry_cmp(const void *a, const void *b) {
    return ((KVStrInt *)b)->value - ((KVStrInt *)a)->value;
}

void frequency_analysis() {
    begin_profiling();
    StringBuilder sb = {0};
    read_file(&sb, SV("shakespeare.txt"));
    // read_file(&sb, SV("gatsby.txt"));
    // read_file(&sb, SV("test_hashmap.c"));
    String contents = sb_to_string(&sb);

    Arena a = {0};
    StringList words = string_split_chars_ex(&a, contents, SV(" \n"), SPLIT_SKIP_EMPTY);

    MapStrInt map = {0};

    list_foreach(words.head, StringNode, word) {
        String key = string_to_lower(&a, word->str);
        *hms_entry(&a, &map, key, int) += 1;
    }
    printf("size = %zu, capacity = %zu\n", map.size, map.capacity);
    end_profiling_and_print_stats();

    KVStrInt *entries = arena_memdup(&a, KVStrInt, map.data + 1, map.size);
    qsort(entries, map.size, sizeof(*entries), hash_entry_cmp);

#ifdef ENABLE_PROFILING
    begin_profiling();
    for (size_t i = 0; i < map.size; i++) {
        KVStrInt *pair = entries + i;
        hms_del(&map, pair->key);
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

// Set hashmap capacity to 4
// Should print 13 0
void test_small_hashmap_collision() {
    MapStrInt hm = {0};
    Arena a = {0};
    *hms_entry(&a, &hm, SV("abcd"), int) = 12;
    *hms_entry(&a, &hm, SV("efgh"), int) = 13;

    hms_del(&hm, SV("abcd"));
    int n = hms_get(&hm, SV("efgh"), int);
    printf("%d\n", n);
    n = hms_get(&hm, SV("efg"), int);
    printf("%d\n", n);
}

int main() {
    frequency_analysis();
    return 0;
}

