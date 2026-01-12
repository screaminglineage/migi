#ifndef MIGI_DYNAMIC_STRING_H
#define MIGI_DYNAMIC_STRING_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "migi.h"
#include "migi_string.h"

typedef struct {
    String string;
    size_t capacity;
} DString;

// TODO: what would be a good initial allocation size?
#define DSTRING_INIT_CAP 32

#define DS(cstr) dstring_new((cstr), sizeof(cstr) - 1)
#define DS_FMT(ds) (int)(ds).string.length, (ds).string.data

static DString dstring_new(const char *data, size_t length);
static void dstring_free(DString *dstr);

static DString dstring_from_string(String str);

// Create a temporary CString from the dynamic string
// NOTE: Any additional pushes to the dynamic string will destroy the cstring
static const char *dstring_to_temp_cstr(DString *dstr);

static void dstring_push(DString *dstr, String str);
static void dstring_pushf(DString *dstr, const char *fmt, ...);
static void dstring_push_char(DString *dstr, char ch);
static void dstring_push_cstr(DString *dstr, const char *cstr);
static void dstring_push_buffer(DString *dstr, const char *data, size_t length);

// Extend `dstr` with `dstr_other` and then free the latter
static void dstring_consume(DString *dstr, DString *dstr_other);

static DString dstring_new(const char *data, size_t length) {
    size_t size_bytes = DSTRING_INIT_CAP * sizeof(*data);
    String string = {
        .data = memcpy(malloc(size_bytes), data, length),
        .length = length
    };
    return (DString){
        .string = string,
        .capacity = DSTRING_INIT_CAP
    };
}

static const char *dstring_to_temp_cstr(DString *dstr) {
    dstring_push_char(dstr, 0);
    dstr->string.length -= 1;
    return dstr->string.data;
}

static DString dstring_from_string(String str) {
    return dstring_new(str.data, str.length);
}

static void dstring_push_buffer(DString *dstr, const char *data, size_t length) {
    size_t new_length = dstr->string.length + length;
    if (new_length < dstr->capacity) {
        memcpy((char *)(dstr->string.data + dstr->string.length), data, length);
    } else {
        size_t new_capacity = next_power_of_two(new_length);
        dstr->string.data = realloc((char *)dstr->string.data, new_capacity);
        avow(dstr->string.data, "dstring__push: out of memory");
        dstr->capacity = new_capacity;
    }
    dstr->string.length = new_length;
}

static void dstring_push_char(DString *dstr, char ch) {
    dstring_push_buffer(dstr, &ch, 1);
}

static void dstring_push_cstr(DString *dstr, const char *cstr) {
    size_t len = strlen(cstr);
    dstring_push_buffer(dstr, cstr, len);
}

static void dstring_push(DString *dstr, String str) {
    dstring_push_buffer(dstr, str.data, str.length);
}

static void dstring_pushf(DString *dstr, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_saved;
    va_copy(args_saved, args);

    // vsnprintf doesnt count the null terminator
    int push_length = vsnprintf(NULL, 0, fmt, args) + 1;
    size_t new_length = dstr->string.length + push_length;

    if (new_length > dstr->capacity) {
        size_t new_capacity = next_power_of_two(new_length);
        dstr->string.data = realloc((char *)dstr->string.data, new_capacity);
        avow(dstr->string.data, "dstring__push: out of memory");
        dstr->capacity = new_capacity;
    }
    vsnprintf((char *)(dstr->string.data + dstr->string.length), push_length, fmt, args_saved);
    // popping off the null terminator
    dstr->string.length = new_length - 1;

    va_end(args_saved);
    va_end(args);
}

static void dstring_consume(DString *dstr, DString *dstr_other) {
    dstring_push_buffer(dstr, dstr_other->string.data, dstr_other->string.length);
    dstring_free(dstr_other);
}

static void dstring_free(DString *dstr) {
    free((char *)dstr->string.data);
    *dstr = (DString){0};
}


#endif // MIGI_DYNAMIC_STRING_H
