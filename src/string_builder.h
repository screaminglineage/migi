#ifndef MIGI_STRING_BUILDER_H
#define MIGI_STRING_BUILDER_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "migi_core.h"
#include "migi_string.h"
#include "arena.h"
#include "file.h"

typedef struct {
    Arena *arena;
} StrBuilder;


static StrBuilder sb_init();
static StrBuilder sb_init_static(char *buf, size_t buf_size);
static size_t sb_length(StrBuilder *sb);


// Convenience function which allows pushing in any supported type
// TODO: check if sb_push_bool and sb_push_char work on MSVC
#define sb_push(sb, elem)                 \
    _Generic((elem),                      \
        Str:                sb_push_str,  \
        char *:             sb_push_cstr, \
        bool:               sb_push_bool, \
        char:               sb_push_char, \
        signed char:        sb_push_i64,  \
        unsigned char:      sb_push_u64,  \
        short:              sb_push_i64,  \
        unsigned short:     sb_push_u64,  \
        int:                sb_push_i64,  \
        unsigned int:       sb_push_u64,  \
        long:               sb_push_i64,  \
        unsigned long:      sb_push_u64,  \
        long long:          sb_push_i64,  \
        unsigned long long: sb_push_u64,  \
        float:              sb_push_f64,  \
        double:             sb_push_f64,  \
        void *:             sb_push_ptr,  \
        default:            sb_push_ptr   \
    )((sb), (elem))



static void sb_push_bool(StrBuilder *sb, bool to_push);
static void sb_push_char(StrBuilder *sb, char to_push);
static void sb_push_i64(StrBuilder *sb, int64_t to_push);
static void sb_push_u64(StrBuilder *sb, uint64_t to_push);
static void sb_push_f64(StrBuilder *sb, double to_push);
static void sb_push_str(StrBuilder *sb, Str string);
static void sb_push_cstr(StrBuilder *sb, char *cstr);
static void sb_push_ptr(StrBuilder *sb, void *ptr);
static void sb_push_buffer(StrBuilder *sb, char *buf, size_t length);
static void sb_push_file(StrBuilder *sb, Str filename);
migi_printf_format(2, 3) static void sb_pushf(StrBuilder *sb, const char *fmt, ...);

static StrBuilder sb_from_string(Str string);
static Str sb_to_string(StrBuilder *sb);
static bool sb_to_file(StrBuilder *sb, Str filename);

void sb_reset(StrBuilder *sb);
void sb_free(StrBuilder *sb);






static StrBuilder sb_init() {
    Arena *a = arena_init(.type = Arena_Linear);
    return (StrBuilder){
        .arena = a
    };
}

static StrBuilder sb_init_static(char *buf, size_t buf_size) {
    Arena *a = arena_init_static(buf, buf_size);
    return (StrBuilder){
        .arena = a
    };
}

static size_t sb_length(StrBuilder *sb) {
    return sb->arena->position - sizeof(Arena);
}

// This needs to be a function (and not a macro) due to the weird rules on _Generic
static void sb__push_no_match(StrBuilder *sb, void *elem) {
    unused(sb);
    unused(elem);
    crash_with_message("sb_push: no types matched");
}

static void sb_push_i64(StrBuilder *sb, int64_t to_push) {
    char tmp[64];
    size_t i = array_len(tmp);

    int64_t num = to_push > 0? to_push: -to_push;
    do {
        tmp[--i] = '0' + num % 10;
        num /= 10;
    } while (num > 0);

    if (to_push < 0) {
        tmp[--i] = '-';
    }
    sb_push_buffer(sb, &tmp[i], array_len(tmp) - i);
}

static void sb_push_u64(StrBuilder *sb, uint64_t to_push) {
    char tmp[64];
    size_t i = array_len(tmp);

    do {
        tmp[--i] = '0' + to_push % 10;
        to_push /= 10;
    } while (to_push > 0);

    sb_push_buffer(sb, &tmp[i], array_len(tmp) - i);
}

static void sb_push_hex(StrBuilder *sb, uint64_t to_push) {
    char tmp[64];
    size_t i = array_len(tmp);

    static char *digits = "0123456789ABCDEF";
    do {
        tmp[--i] = digits[to_push % 16];
        to_push /= 16;
    } while (to_push > 0);

    sb_push_buffer(sb, &tmp[i], array_len(tmp) - i);
}


// Taken from: https://nullprogram.com/blog/2023/02/13/#float-formatting
static void sb_push_f64(StrBuilder *sb, double to_push) {
    int64_t prec = 1000000;  // i.e. 6 decimals

    if (to_push < 0) {
        sb_push_char(sb, '-');
        to_push = -to_push;
    }

    to_push += 0.5 / prec;  // round last decimal
    if (to_push >= (double)(-1UL>>1)) {  // out of long range?
        sb_push_str(sb, S("inf"));
    } else {
        long integral = to_push;
        long fractional = (to_push - integral)*prec;
        sb_push_i64(sb, integral);
        sb_push_char(sb, '.');
        for (long i = prec/10; i > 1; i /= 10) {
            if (i > fractional) {
                sb_push_char(sb, '0');
            }
        }
        sb_push_i64(sb, fractional);
    }
}

static void sb_push_bool(StrBuilder *sb, bool to_push) {
    if (to_push) {
        sb_push_str(sb, S("true"));
    } else {
        sb_push_str(sb, S("false"));
    }
}

static void sb_push_char(StrBuilder *sb, char to_push) {
    *arena_push(sb->arena, char, 1) = to_push;
}

static void sb_push_str(StrBuilder *sb, Str string) {
    arena_copy(sb->arena, char, string.data, string.length);
}

static void sb_push_cstr(StrBuilder *sb, char *cstr) {
    size_t length = strlen(cstr);
    arena_copy(sb->arena, char, cstr, length);
}

static void sb_push_buffer(StrBuilder *sb, char *buf, size_t length) {
    arena_copy(sb->arena, char, buf, length);
}

static void sb_push_ptr(StrBuilder *sb, void *ptr) {
    sb_push_str(sb, S("0x"));
    sb_push_hex(sb, (uintptr_t)ptr);
}

// NOTE: sb_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void sb_pushf(StrBuilder *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str__format(sb->arena, fmt, args);
    va_end(args);
}

static void sb_push_file(StrBuilder *sb, Str filename) {
    str_from_file(sb->arena, filename);
}

void sb_reset(StrBuilder *sb) {
    arena_reset(sb->arena);
}

void sb_free(StrBuilder *sb) {
    arena_free(sb->arena);
    mem_clear(sb);
}

static StrBuilder sb_from_string(Str string) {
    StrBuilder sb = {0};
    sb_push_str(&sb, string);
    return sb;
}

static Str sb_to_string(StrBuilder *sb) {
    return (Str){
        .data = (char *)sb->arena->data,
        .length = sb_length(sb)
    };
}

// NOTE: The cstring returned from this function is not separately allocated
// and is destroyed after a subsequent push onto the string builder
static const char *sb_to_cstr(StrBuilder *sb) {
    sb_push_char(sb, 0);
    const char *cstr = (const char *)sb->arena->data;
    arena_pop(sb->arena, char, 1);
    return cstr;
}

static bool sb_to_file(StrBuilder *sb, Str filename) {
    return str_to_file(sb_to_string(sb), filename);
}

#endif // MIGI_STRING_BUILDER_H
