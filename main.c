#include <stddef.h>
#include <stdio.h>

#include "migi.c"

typedef struct {
    int *data;
    size_t length;
    size_t capacity;
} Ints;

int main() {
    Ints ints = {0};
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, i);
    }

    Ints ints_new = {0};
    array_reserve(&ints_new, 100);
    for (size_t i = 0; i < 100; i++) {
        array_add(&ints, 2*i);
    }

    array_extend(&ints_new, &ints);
    array_swap_remove(&ints_new, 50);
    printf("ints = %zu, new_ints = %zu\n", ints.length, ints_new.length);

    for (size_t i = 0; i < ints_new.length; i++) {
        printf("%d ", ints_new.data[i]);
    }

    printf("\n");

    return 0;
}
