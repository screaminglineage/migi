#ifndef DYNAMIC_DEQUE_H
#define DYNAMIC_DEQUE_H

// Dynamically growing deque using virtual memory.
//
// Note that this never wraps around, but rather utilises the virtual
// address space to reserve a huge amount of memory (64 GB by default),
// only committing when needed. The actual deqeue is allocated in the middle of
// this reservation, allowing it to grow in either direction until it reaches
// the edges
//
// Due to this as an added bonus reads/writes can use memcpy, memmove, etc.

// TODO: detect when the head/tail reaches one end but there is still space, and relocate
// the deque back to the middle of the allocation

#include <stddef.h>

#include "migi.h"
#include "migi_memory.h"

typedef struct {
    byte *data;
    size_t head;
    size_t tail;
    size_t committed_start;
    size_t committed_end;
    size_t total;
} Deque;

#ifndef DEQUE_DEFAULT_CAPACITY
    #define DEQUE_DEFAULT_CAPACITY 64*GB
#endif // DEQUE_DEFAULT_CAPACITY

// Decommits excess memory from this threshold onwards
#ifndef DEQUE_DECOMMIT_THRESHOLD
    #define DEQUE_DECOMMIT_THRESHOLD 64*MB
#endif // DEQUE_DECOMMIT_THRESHOLD


#define deque_init() (deque_init_ex(DEQUE_DEFAULT_CAPACITY))
static inline Deque deque_init_ex(size_t total);
static inline void deque_free(Deque *deque);

#define deque_push_head(deque, type, length) \
    (type*)deque_push_head_bytes((deque), (length)*sizeof(type), _Alignof(type))

#define deque_pop_head(deque, type, length) \
    deque_pop_head_bytes((deque), (length)*sizeof(type))

#define deque_push_tail(deque, type, length) \
    (type*)deque_push_tail_bytes((deque), (length)*sizeof(type), _Alignof(type))

#define deque_pop_tail(deque, type, length) \
    deque_pop_tail_bytes((deque), (length)*sizeof(type))

static void *deque_push_head_bytes(Deque *deque, size_t size, size_t align);
static void deque_pop_head_bytes(Deque *deque, size_t size);
static void *deque_push_tail_bytes(Deque *deque, size_t size, size_t align);
static void deque_pop_tail_bytes(Deque *deque, size_t size);

static inline Deque deque_init_ex(size_t total) {
    Deque deque = {0};
    deque.total = total;
    deque.data = memory_reserve(deque.total);

    size_t median = align_up_page_size(deque.total / 2);
    deque.head = median;
    deque.tail = median;
    deque.committed_start = median;
    deque.committed_end = median;
    return deque;
}

static inline void deque_free(Deque *deque) {
    memory_free(deque->data, deque->total);
    mem_clear(deque);
}

static void *deque_push_tail_bytes(Deque *d, size_t size, size_t align) {
    byte *alloc_start = d->data + d->tail;
    size_t alignment = align_up_padding((uintptr_t)alloc_start, align);
    size_t alloc_end = d->tail + alignment + size;

    if (alloc_end > d->committed_end) {
        size_t new_committed_end = align_up_page_size(alloc_end);
        avow(new_committed_end - d->head <= d->total,
            "deque_push_tail_bytes: reserved virtual address space of %zu bytes exhausted", d->total);

        size_t extra_tail = new_committed_end - d->committed_end;
        memory_commit(d->data + d->committed_end, extra_tail);
        d->committed_end = new_committed_end;
    }

    void *mem = d->data + d->tail + alignment;
    d->tail = alloc_end;
    return mem;
}

static void *deque_push_head_bytes(Deque *d, size_t size, size_t align) {
    byte *start = d->data + d->head - size;
    size_t alignment = align_down_padding((uintptr_t)start, align);
    size_t alloc_start = d->head - size - alignment;

    if (alloc_start < d->committed_start) {
        size_t new_committed_start = align_down_page_size(alloc_start);
        avow(d->tail - new_committed_start <= d->total,
            "deque_push_head_bytes: reserved virtual address space of %zu bytes exhausted", d->total);

        size_t extra_head = d->committed_start - new_committed_start;
        memory_commit(d->data + new_committed_start, extra_head);
        d->committed_start = new_committed_start;
    }

    d->head = alloc_start;
    void *mem = d->data + d->head;
    return mem;
}


static void deque_pop_tail_bytes(Deque *d, size_t size) {
    if (d->committed_end == 0) return;
    size_t length = d->tail - d->head;
    size_t extra = clamp_top(size, length);
    d->tail -= extra;

    size_t next_page_after_tail = align_up_page_size(d->tail);
    size_t extra_length = d->committed_end - next_page_after_tail;
    if (extra_length >= DEQUE_DECOMMIT_THRESHOLD) {
        memory_decommit(d->data + next_page_after_tail, extra_length);
        d->committed_end = next_page_after_tail;
    }
}

static void deque_pop_head_bytes(Deque *d, size_t size) {
    if (d->committed_end == 0) return;
    size_t length = d->tail - d->head;
    size_t extra = clamp_top(size, length);
    d->head += extra;


    size_t prev_page_before_head = align_down_page_size(d->head);
    size_t extra_length = prev_page_before_head - d->committed_start;
    if (extra_length >= DEQUE_DECOMMIT_THRESHOLD) {
        memory_decommit(d->data + d->committed_start, extra_length);
        d->committed_start = prev_page_before_head;
    }
}


#endif // DYNAMIC_DEQUE_H
