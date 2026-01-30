#ifndef MIGI_DYNAMIC_STRING_H
#define MIGI_DYNAMIC_STRING_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "migi_core.h"
#include "migi_string.h"

typedef struct {
    union {
        Str as_string;
        struct {
            const char *data;
            size_t length;
        };
    };
    size_t capacity;
} DStr;

// TODO: what would be a good initial allocation size?
#define DSTRING_INIT_CAP 32

#define DS(cstr) dstr_new((cstr), sizeof(cstr) - 1)

static DStr dstr_new(const char *data, size_t length);
static void dstr_free(DStr *dstr);

static DStr dstr_from_string(Str str);

// Create a temporary CString from the dynamic string
// NOTE: Any additional pushes to the dynamic string will destroy the cstring
static const char *dstr_to_temp_cstr(DStr *dstr);

static void dstr_push(DStr *dstr, Str str);
static void dstr_pushf(DStr *dstr, const char *fmt, ...);
static void dstr_push_char(DStr *dstr, char ch);
static void dstr_push_cstr(DStr *dstr, const char *cstr);
static void dstr_push_buffer(DStr *dstr, const char *data, size_t length);

// Extend `dstr` with `dstr_other` and then free the latter
static void dstr_consume(DStr *dstr, DStr *dstr_other);

static DStr dstr_new(const char *data, size_t length) {
    size_t size_bytes = clamp_bottom(DSTRING_INIT_CAP, next_power_of_two(length));
    Str string = {
        .data = memcpy(malloc(size_bytes), data, length),
        .length = length
    };
    return (DStr){
        .as_string = string,
        .capacity = DSTRING_INIT_CAP
    };
}

static const char *dstr_to_temp_cstr(DStr *dstr) {
    dstr_push_char(dstr, 0);
    dstr->length -= 1;
    return dstr->data;
}

static DStr dstr_from_string(Str str) {
    return dstr_new(str.data, str.length);
}

static void dstr_push_buffer(DStr *dstr, const char *data, size_t length) {
    size_t new_length = dstr->length + length;
    if (new_length >= dstr->capacity) {
        size_t new_capacity = next_power_of_two(new_length);
        dstr->data = realloc((char *)dstr->data, new_capacity);
        avow(dstr->data, "dstring__push: out of memory");
        dstr->capacity = new_capacity;
    }
    memcpy((char *)(dstr->data + dstr->length), data, length);
    dstr->length = new_length;
}

static void dstr_push_char(DStr *dstr, char ch) {
    dstr_push_buffer(dstr, &ch, 1);
}

static void dstr_push_cstr(DStr *dstr, const char *cstr) {
    size_t len = strlen(cstr);
    dstr_push_buffer(dstr, cstr, len);
}

static void dstr_push(DStr *dstr, Str str) {
    dstr_push_buffer(dstr, str.data, str.length);
}

migi_printf_format(2, 3)
static void dstr_pushf(DStr *dstr, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_saved;
    va_copy(args_saved, args);

    // vsnprintf doesnt count the null terminator
    int push_length = vsnprintf(NULL, 0, fmt, args) + 1;
    size_t new_length = dstr->length + push_length;

    if (new_length > dstr->capacity) {
        size_t new_capacity = next_power_of_two(new_length);
        dstr->data = realloc((char *)dstr->data, new_capacity);
        avow(dstr->data, "dstring__push: out of memory");
        dstr->capacity = new_capacity;
    }
    vsnprintf((char *)(dstr->data + dstr->length), push_length, fmt, args_saved);
    // popping off the null terminator
    dstr->length = new_length - 1;

    va_end(args_saved);
    va_end(args);
}

static void dstr_consume(DStr *dstr, DStr *dstr_other) {
    dstr_push_buffer(dstr, dstr_other->data, dstr_other->length);
    dstr_free(dstr_other);
}

static void dstr_free(DStr *dstr) {
    free((char *)dstr->data);
    mem_clear(dstr);
}


#endif // MIGI_DYNAMIC_STRING_H
