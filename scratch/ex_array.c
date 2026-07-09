#include "migi_core.h"
#include "arena.h"
#include "random.h"

#define EXARRAY_FIRST_BLOCK_SIZE 8
#define EXARRAY_NUM_BLOCKS 20

#define EXARRAY_HEADER \
    size_t length;     \
    int32_t _tmp[2];   \
    Arena *arena;

typedef struct {
    EXARRAY_HEADER
} ExArrayHeader;

#define ExArray(type)      \
    union {                \
        ExArrayHeader _h;  \
        struct {           \
            EXARRAY_HEADER \
            type **arrays; \
        };                 \
    }

int migi_log2(int64_t n) {
    int result = 0;
    while (n >>= 1) result++;
    return result;
}

// EXARRAY_FIRST_BLOCK_SIZE * (2**n - 1)
#define exarr__block_size(n) (EXARRAY_FIRST_BLOCK_SIZE * ((1 << (n)) - 1))

void exarr__index(size_t index, int32_t *n1, int32_t *n2) {
    // 2**n = (1 << n)
    *n1 = migi_log2(index/EXARRAY_FIRST_BLOCK_SIZE + 1);
    *n2 = (uint32_t)(index - exarr__block_size(*n1));
}

void **exarr__reserve(ExArrayHeader *h, void **arrays, size_t elem_size, size_t elem_align, size_t at_least) {
    if (!arrays) {
        if (!h->arena) h->arena = arena_init(.type=Arena_Linear);
        arrays = arena_push(h->arena, void *, EXARRAY_NUM_BLOCKS);
    }
    exarr__index(at_least, &h->_tmp[0], &h->_tmp[1]);

    avow(h->_tmp[0] < EXARRAY_NUM_BLOCKS, "exarr__reserve: out of memory");

    for (int i = h->_tmp[0]; !arrays[i] && i >= 0; i--) {
        size_t block_size = EXARRAY_FIRST_BLOCK_SIZE + exarr__block_size(i);
        arrays[i] = arena_push_bytes(h->arena, elem_size*block_size, elem_align, true);
    }
    return arrays;
}

void **exarr__push(ExArrayHeader *h, void **arrays, size_t elem_size, size_t elem_align) {
    return exarr__reserve(h, arrays, elem_size, elem_align, h->length++);
}

// Array with a single element that decays to a pointer
// Needed for calls like `hashmap_put(&h, 1, foo)`, since `&1` is invalid
// NOTE: type_of(x) cannot be used here since if x is a c-string,
// then type_of(x) returns `char[1][LEN(cstr)]` instead of `char**`
// TODO: since hashmap, exponential_array, and search all use this, this should be moved into migi_core
#define exarr__addr_of(T, x) ((type_of(T)[1]){x})


#define exarr_push(arr, elem)                                                         \
    (void)(                                                                           \
        (arr)->arrays = (type_of((arr)->arrays))                                      \
            exarr__push(&(arr)->_h, (void **)(arr)->arrays,                           \
                        sizeof(**(arr)->arrays), align_of(type_of(**(arr)->arrays))), \
        (arr)->arrays[(arr)->_tmp[0]][(arr)->_tmp[1]] = (elem)                        \
    )


#define exarr_reserve(arr, at_least)                                                             \
    (void)(                                                                                      \
        (arr)->arrays = (type_of((arr)->arrays))                                                 \
            exarr__reserve(&(arr)->_h, (void **)(arr)->arrays,                                   \
                        sizeof(**(arr)->arrays), align_of(type_of(**(arr)->arrays)), (at_least)) \
    )


#define exarr_at(arr, index)                                                                                                    \
    (assertf((size_t)(index) < (arr)->length, "exarr_at: index out of bounds, length: %zu, index: %d", (arr)->length, (index)), \
     exarr__index((index), &(arr)->_tmp[0], &(arr)->_tmp[1]),                                                                   \
    &(arr)->arrays[(arr)->_tmp[0]][(arr)->_tmp[1]])


#define exarr_last(arr)                                                \
    (exarr__index((arr)->length - 1, &(arr)->_tmp[0], &(arr)->_tmp[1]), \
    &(arr)->arrays[(arr)->_tmp[0]][(arr)->_tmp[1]])


#define exarr_pop(arr)                                                 \
    *(exarr__index(--(arr)->length, &(arr)->_tmp[0], &(arr)->_tmp[1]), \
    &(arr)->arrays[(arr)->_tmp[0]][(arr)->_tmp[1]])


#define exarr_swap_remove(arr, index)                                                                                           \
do {                                                                                                                            \
    assertf((index) < (arr)->length, "exarr_swap_remove: index out of bounds, length: %zu, index: %d", (arr)->length, (index)); \
    type_of(**(arr)->arrays) make_unique(last) = *exarr_last((arr));                                                            \
    exarr__index((index), &(arr)->_tmp[0], &(arr)->_tmp[1]);                                                                    \
    (arr)->arrays[(arr)->_tmp[0]][(arr)->_tmp[1]] = make_unique(last);                                                          \
    (arr)->length--;                                                                                                            \
} while (0)


#define exarr_free(arr) mem_clear((arr))


#define exarr_foreach(arr, elem)                                                                                        \
    for (struct {size_t i; size_t j; } __s = {0, 0}; (arr)->arrays[__s.i] && __s.i < EXARRAY_NUM_BLOCKS; __s.i++)       \
        for (type_of(*(arr)->arrays) elem = (arr)->arrays[__s.i];                                                       \
            __s.j < (arr)->length && elem < (arr)->arrays[__s.i] + EXARRAY_FIRST_BLOCK_SIZE + exarr__block_size(__s.i); \
            elem++, __s.j++)                                                                                            \


int main() {
    Temp tmp = arena_temp();

    // Passing an arena is optional
    // ExArray will create its own arena if arena is NULL
    ExArray(int) arr = {.arena = tmp.arena};
    exarr_reserve(&arr, 1000);

    int s = 1000;
    for (int i = 0; i < s; i++) {
        exarr_push(&arr, i);
    }

    int r = (int)rand_range_exclusive(0, s);
    int *n = exarr_at(&arr, r);
    printf("index #%d = %d\n", r, *n);
    assert(r == *n);

    int *last = exarr_last(&arr);
    printf("last = %d\n", *last);
    assert(*last == s - 1);

    *exarr_at(&arr, 2) = 100;
    assert(*exarr_at(&arr, 2) == 100);

#if PRINT_BLOCKS
    for (size_t j = 0; j < EXARRAY_NUM_BLOCKS; j++) {
        if (arr.arrays[j]) {
            array_print(arr.arrays[j], bit(migi_log2(EXARRAY_FIRST_BLOCK_SIZE) + j), "%d");
        }
    }
#endif

    // Should fail an assert
    // *exarr_at(&arr, 1000) = 12;

    int c = 0;
    exarr_foreach(&arr, i) {
        c++;
    }
    assertf((size_t)c == arr.length, "each element must be visited");

    int m = exarr_pop(&arr);
    assert(m == s - 1 && *exarr_last(&arr) == s - 2);

    int prev_last = *exarr_last(&arr);
    exarr_swap_remove(&arr, 500);
    assert(*exarr_at(&arr, 500) == prev_last);

    exarr_free(&arr);

    arena_temp_release(tmp);
    printf("Exiting Successfully\n");
    return 0;
}
