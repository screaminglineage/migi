#include "migi_core.h"
#include "arena.h"

typedef struct {
    uint8_t *data;
    size_t length;
} BitSet;

typedef struct {
    uint8_t *buffer;       // backing buffer for bitset
    size_t buffer_length;  // length of the backing buffer
    size_t length;         // length of the bitset itself (NOTE: this is not the length of the array but rather the number of bits)
} BitInitOpt;

#define bit_init(arena, ...) \
    bit_init_opt((arena), (BitInitOpt){__VA_ARGS__})

static BitSet bit_init_opt(Arena *a, BitInitOpt opt) {
    BitSet result = {0};
    if (opt.buffer) {
        result.data = opt.buffer;
        result.length = opt.buffer_length;
    } else {
        result.length = align_up_pow2(opt.length, 8) / 8;
        result.data = arena_push(a, uint8_t, result.length);
    }
    return result;
}

static void bit__alloc(BitSet *set) {
    if (!set->data) {
        set->data = calloc(set->length, sizeof(*set->data));
    }
}

// TODO: take endianness into account
void bit__index(size_t index, size_t *n, int *m) {
    *n = index/8;
    *m = align_up_pow2_amt(index + 1, 8);
}

bool bit_set(BitSet *set, size_t index) {
    bit__alloc(set);
    size_t n;
    int m;
    bit__index(index, &n, &m);
    assertf(n < set->length, "bit_set: index out of bounds, length: %zu, index: %zu", set->length, n);

    set->data[n] |= bit(m);
    return set->data[n] & bit(m);
}

bool bit_clear(BitSet *set, size_t index) {
    bit__alloc(set);
    size_t n;
    int m;
    bit__index(index, &n, &m);
    assertf(n < set->length, "bit_clear: index out of bounds, length: %zu, index: %zu", set->length, n);
    set->data[n] &= ~bit(m);
    return set->data[n] & bit(m);
}

void bit_clear_all(BitSet *set) {
    if (set->data) mem_clear_array(set->data, set->length);
}

bool bit_flip(BitSet *set, size_t index) {
    bit__alloc(set);
    size_t n;
    int m;
    bit__index(index, &n, &m);
    assertf(n < set->length, "bit_flip: index out of bounds, length: %zu, index: %zu", set->length, n);
    set->data[n] ^= bit(m);
    return set->data[n] & bit(m);
}

void bit_flip_all(BitSet *set) {
    for (size_t i = 0; i < set->length; i++) {
        set->data[i] = ~set->data[i];
    }
}

bool bit_get(BitSet *set, size_t index) {
    bit__alloc(set);
    size_t n;
    int m;
    bit__index(index, &n, &m);
    assertf(n < set->length, "bit_get: index out of bounds, length: %zu, index: %zu", set->length, n);
    return set->data[n] & bit(m);
}

// Returns a mask of [start, end] for bits
// Eg. bit_range(1, 3) == 0b011100000
// TODO: The range here is inclusive instead of being exclusive of the end, which
// is different from all other ranges. Fix that
#define bit_range(start, end) (((1 << (start_m + 1)) - 1) & (0xff << (end_m)))


// Slice a bitset into [start, end) (end exclusive range)
BitSet bit_slice(Arena *a, BitSet *set, size_t start, size_t end) {
    size_t start_n, end_n;
    int start_m, end_m;
    bit__index(start, &start_n, &start_m);
    bit__index(end - 1, &end_n, &end_m);

    BitSet result = {0};
    if (start_m == 0 && end_n == 7) {
        result.data = &set->data[start_n];
        result.length = (end_n - start_n + 1)*8;
        return result;
    }

    // TODO: take endianness into account
    if (start_n == end_n) {
        // same block
        result.length = 1;
        result.data = arena_new(a, uint8_t);
        int upper_bits = (1 << (start_m + 1)) - 1;
        int lower_bits = 0xff << (end_m);
        int mask =  upper_bits & lower_bits; 
        *result.data = (set->data[start_n] & mask) << (7 - start_m);
    } else {
        // different blocks
        todo();
    }

    // size_t first = start_m;
    // size_t mid   = (end_n == start_n)? 0: (end_n - start_n - 1)*8;
    // size_t last  = end_m;
    //
    // result.length = first + mid + last;
    // result.data = arena_push(a, uint8_t, result.length);
    //
    // uint8_t *data = result.data;
    // memcpy(data, set->data + start_n + (7 - start_m), first);
    // data += first;
    //
    // memcpy(data, set->data + start_m + 1, mid);
    // data += mid;
    //
    // memcpy(data, set->data + end_n + end_m, last);
    return result;
}

typedef enum {
    BitOp_And,
    BitOp_Or,
    BitOp_Xor,
} BitOp;

// TODO: should this pad when the sets are not of equal length?
void bit_op(BitSet *set1, BitSet set2, BitOp op) {
    assertf(set1->length == set2.length, "bit_op: bit sets must be of equal length");
    for (size_t i = 0; i < set1->length; i++) {
        switch (op) {
            case BitOp_And: {
                set1->data[i] &= set2.data[i];
            } break;
            case BitOp_Or: {
                set1->data[i] |= set2.data[i];
            } break;
            case BitOp_Xor: {
                set1->data[i] ^= set2.data[i];
            } break;
        }
    }
}


// TODO: take endianness into account
void bit_print(BitSet *set) {
    if (!set->data) return;

    for (size_t i = 0; i < set->length; i++) {
        for (int j = 7; j >= 0; j--) {
            if (set->data[i] & bit(j)) {
                printf("1");
            } else {
                printf("0");
            }
        }
    }
    printf("\n");
}

void bit_free(BitSet *set) {
    mem_clear(set);
}

int main() {
    Temp tmp = arena_temp();
    BitSet b = bit_init(tmp.arena, .length=100); 

    bit_set(&b, 15);
    bit_set(&b, 24);
    bit_print(&b);

    bit_flip(&b, 40);
    bit_print(&b);

    bit_clear(&b, 40);
    bit_print(&b);

    bit_flip_all(&b);
    bit_print(&b);

    bit_clear_all(&b);
    bit_print(&b);

    bit_set(&b, 2);
    bit_set(&b, 3);
    bit_set(&b, 4);
    bit_print(&b);
    BitSet b1 = bit_slice(tmp.arena, &b, 1, 5);
    printf("should be 0b01110\n");
    bit_print(&b1);
    return 0;
    BitSet b2 = bit_slice(tmp.arena, &b, 6, 10);
    // bit_op(&b1, b2, BitOp_And);

    bit_free(&b);
    arena_temp_release(tmp);
    return 0;
}

