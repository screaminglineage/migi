#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "arena.h"
#include "migi.h"
#include "hashmap.h"
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

    Point *p = hms_entry(&hm, Point, SV("foo"));
    printf("%d %d\n", p->x, p->y);

    p = hms_entry(&hm, Point, SV("abcd"));
    if (!p) printf("key not present!\n");

    ptrdiff_t i = hms_index(&hm, SV("bar"));
    if (i != -1) {
        Point p = hm.data[i].value;
        printf("%d %d\n", p.x, p.y);
    }

    KVStrPoint *pair = hms_entry_pair(&hm, KVStrPoint, SV("baz"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);

    KVStrPoint pair1 = hms_get_pair(&hm, KVStrPoint, SV("bazz"));
    printf("`%.*s`: (Point){%d %d}\n", SV_FMT(pair1.key), pair1.value.x, pair1.value.y);

    Point p1 = hms_get(&hm, Point, SV("bla"));
    printf("bla: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, Point, SV("blah"));
    printf("EMPTY: (Point){%d %d}\n", p2.x, p2.y);

    printf("\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);
    }

    KVStrPoint deleted = hms_pop(&hm, KVStrPoint, SV("bar"));
    printf("Deleted: %.*s: (Point){%d %d}\n", SV_FMT(deleted.key), deleted.value.x, deleted.value.y);

    printf("\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);
    }

    hms_del(&hm, SV("aaaaa"));
}


int main() {
    // test_basic();

    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));

    hms_set_default(&hm, ((Point){100, 100}));

    Point p1 = hms_get(&hm, Point, SV("foo"));
    printf("foo: (Point){%d %d}\n", p1.x, p1.y);

    Point p2 = hms_get(&hm, Point, SV("oof!"));
    printf("oof!: (Point){%d %d}\n", p2.x, p2.y);

    KVStrPoint p3 = hms_get_pair(&hm, KVStrPoint, SV("aaaaa"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(p3.key), p3.value.x, p3.value.y);



    return 0;
}
