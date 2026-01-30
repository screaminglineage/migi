#ifndef MIGI_RING_BUFFER_H
#define MIGI_RING_BUFFER_H

// Simple dynamically growing ring buffer of bytes without using any tricks
// Use ring_push/ring_pop to add/remove elements
//
// Since it can store heterogenous elements, it's possible to read the wrong
// kind of element when a pop is done. So either only store the same kind of
// elements in the same buffer, or use tagged unions to check which type you 
// just popped.
//
// ```c
// ring_push(ring, int, ints, 10)
// ring_push(ring, float, floats, 10)
//
// // This pop will read the wrong type
// ring_pop(ring, float, floats, 10)
// ```
//

#include <stdlib.h>
#include "migi_core.h"
#include "migi_math.h"

#ifndef RING_DEFAULT_CAPACITY
    #define RING_DEFAULT_CAPACITY 64
#endif

typedef struct {
    byte *data;
    size_t head;
    size_t length;
    size_t capacity;
} Ring;


// Initialize and Free ring buffer
// If `capacity` is 0, then the default capacity is used
static Ring ring_init(size_t capacity);
static void ring_free(Ring *ring);

// Get the tail of the ring buffer
// NOTE: Shouldnt be called when capacity is 0
static size_t ring_tail(Ring *ring);

// Push into the tail of the ring buffer
static void ring_push_bytes(Ring *ring, byte *data, size_t size, size_t length, size_t align);
#define ring_push(ring, type, data, length) \
    ring_push_bytes((ring), (byte *)(check_type(type, (data))), sizeof(type), (length), align_of(type));

// Pop the head of the ring buffer
static bool ring_pop_bytes(Ring *ring, byte *data, size_t size, size_t length, size_t align);
#define ring_pop(ring, type, data, length) \
    ring_pop_bytes((ring), (byte *)(check_type(type, (data))), sizeof(type), (length), align_of(type));


static Ring ring_init(size_t capacity) {
    if (capacity == 0) {
        capacity = RING_DEFAULT_CAPACITY;
    }
    capacity = next_power_of_two(capacity);
    return (Ring){
        .data = malloc(capacity),
        .capacity = capacity
    };
}

static void ring_free(Ring *ring) {
    free(ring->data);
    mem_clear(ring);
}

static size_t ring_tail(Ring *ring) {
    assertf(ring->capacity != 0, "ring_tail called with ring->capacity == 0");
    return (ring->head + ring->length) % ring->capacity;
}

// Ensure a minimum of `at_least` bytes fit into the buffer
// Returns the index at which it wraps if `at_least` bytes dont fit contiguously
// If it does fit contiguously then the return value is equal to `at_least`;
static size_t ring__ensure_atleast(Ring *ring, size_t at_least) {
    size_t wrap_index = at_least;

    // check if allocation fits in a contiguous manner, 
    // otherwise return the index where it wraps
    if (ring->length + at_least <= ring->capacity) {
        size_t space_left = ring->capacity - ring_tail(ring);
        if (space_left < at_least) wrap_index = space_left;
        return wrap_index;
    }

    size_t new_capacity = ring->capacity + next_power_of_two(at_least);

    byte *mem = malloc(new_capacity);
    assertf(mem, "Error! Please refer to https://downloadmoreram.com/");

    size_t tail = (ring->capacity == 0)? 0: ring_tail(ring);
    if (ring->length > 0) {
        if (ring->head < tail) {
            memcpy(mem, &ring->data[ring->head], ring->length);
        } else {
            size_t elems_at_end = ring->capacity - ring->head;
            memcpy(mem, &ring->data[ring->head], elems_at_end);
            memcpy(&mem[elems_at_end], &ring->data[0], tail);
        }
        free(ring->data);
    }

    ring->data = mem;
    ring->head = 0;
    ring->capacity = new_capacity;

    return wrap_index;
}

static void ring_push_bytes(Ring *ring, byte *data, size_t size, size_t length, size_t align) {
    size_t tail = (ring->capacity == 0)? 0: ring_tail(ring);
    size_t align_pad = align_up_pow2_amt(tail, align);
    size_t push_start = tail + align_pad;
    size_t push_size = align_pad + size * length;

    size_t wrap_index = ring__ensure_atleast(ring, push_size) - align_pad;
    memcpy(&ring->data[push_start], data, wrap_index);
    if (wrap_index < push_size) {
        memcpy(&ring->data[0], &data[wrap_index], push_size - wrap_index);
    }

    ring->length += push_size;
}

static bool ring_pop_bytes(Ring *ring, byte *data, size_t size, size_t length, size_t align) {
    size_t align_pad = align_up_pow2_amt(ring->head, align);
    size_t pop_start = (ring->head + align_pad) % ring->capacity;
    size_t pop_size = size * length;

    if (pop_size > ring->length) {
        return false;
    }
    
    size_t space_left = ring->capacity - ring->head - align_pad;
    if (space_left >= pop_size) {
        memcpy(data, &ring->data[pop_start], pop_size);
    } else {
        memcpy(data, &ring->data[pop_start], space_left);
        memcpy(&data[space_left], &ring->data[0], pop_size - space_left);
    }

    ring->head = (pop_start + pop_size) % ring->capacity;
    ring->length -= align_pad + pop_size;
    return true;
}



#endif // ifndef MIGI_RING_BUFFER_H

