#ifndef MIGI_TEMP_ALLOC_H
#define MIGI_TEMP_ALLOC_H

#include "migi.h"
#include <stdarg.h>

#ifndef ARENA_DEFAULT_ZONE_CAP
    #define ARENA_DEFAULT_ZONE_CAP 4*KB
#endif
#include "arena.h"
#include "migi_string.h"

// TODO: maybe make this thread local
static Arena *temp_allocator_global_arena;

#define temp_alloc(type, size) \
    ((type *)arena_push_bytes(temp_allocator_global_arena, sizeof(type) * size, _Alignof(type), true))

migi_printf_format(1, 2) static String temp_format(const char *fmt, ...);
static inline void temp_init();
static void temp_reset();
static Checkpoint temp_save();
static void temp_rewind(Checkpoint checkpoint);

static inline void temp_init() {
    temp_allocator_global_arena = arena_init();
}

static String temp_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String res = string__format(temp_allocator_global_arena, fmt, args);
    va_end(args);
    return res;
}

static void temp_reset() {
    arena_reset(temp_allocator_global_arena);
}

static Checkpoint temp_save() {
    return arena_save(temp_allocator_global_arena);
}

static void temp_rewind(Checkpoint checkpoint) {
    arena_rewind(checkpoint);
}


#endif // MIGI_TEMP_ALLOC_H
