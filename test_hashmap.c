#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "arena.h"
#include "migi.h"
#include "hashmap.h"
#include "migi_string.h"


// -------------------------------------------------- 

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


int main() {
    Arena a = {0};
    MapStrPoint hm = {0};

    hms_put(&a, &hm, SV("foo"), ((Point){1, 2}));
    hms_put(&a, &hm, SV("bar"), ((Point){3, 4}));
    hms_put(&a, &hm, SV("baz"), ((Point){5, 6}));

    Point *p = hms_get(&hm, Point, SV("foo"));
    printf("%d %d\n", p->x, p->y);

    p = hms_get(&hm, Point, SV("abcd"));
    if (!p) printf("key not present!\n");

    ptrdiff_t i = hms_index(&hm, SV("bar"));
    if (i != -1) {
        Point p = hm.data[i].value;
        printf("%d %d\n", p.x, p.y);
    }

    KVStrPoint *pair = hms_get_pair(&hm, KVStrPoint, SV("baz"));
    printf("%.*s: (Point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);

    printf("\n\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);
    }

    KVStrPoint *deleted = hms_del(&hm, KVStrPoint, SV("bar"));
    printf("Deleted: %.*s: (point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);

    printf("\n\niteration:\n");
    hm_foreach(&hm, KVStrPoint, pair) {
        printf("%.*s: (point){%d %d}\n", SV_FMT(pair->key), pair->value.x, pair->value.y);
    }


    return 0;
}
