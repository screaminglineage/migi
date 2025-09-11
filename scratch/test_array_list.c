#include <stddef.h>
#include <stdio.h>

#include "arena.h"
#include "migi.h"
#include "migi_lists.h"

typedef struct IntNode IntNode;
struct IntNode {
    int *data;
    size_t length;
    size_t capacity;
    IntNode *next;
};

// Can also be typedef'ed if required to be passed into functions
// typedef ArrayList(IntNode) ArrayListInt;

int main() {
    Arena *a = arena_init();
    ArrayList(IntNode) ints = {0};
    // arrlist_init_capacity(a, &ints, IntNode, 32);
    for (int i = 0; i < 100; i++) {
        arrlist_add(a, &ints, IntNode, i);
    }

    list_foreach(ints.head, IntNode, array) {
        array_print(array->data, array->length, "%d");
    }


    return 0;
}
