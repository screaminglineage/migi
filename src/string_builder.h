#ifndef MIGI_STRING_BUILDER_H
#define MIGI_STRING_BUILDER_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "migi_core.h"
#include "migi_string.h"
#include "arena.h"

// TODO: should it initially store the data on a buffer on the stack?
// TODO: If the above is implemented, then it also needs to copy the
// string over to another block to be returned.
typedef struct {
    Arena *arena;
    char *data;
    int64_t length;
    bool owns_arena;
} StrBuilder;


// Convenience function which allows pushing in any supported type
// NOTE: `bool` and `char` literals are both converted to int, so
// they will not work. They must be passed through variables in order
// to dispatch to the respective function.
// Interestingly, GCC (not even clang) seems to support `bool` literals.
#define sb_push(sb, elem)                    \
    _Generic((elem),                         \
        Str:                sb_push_str,     \
        char *:             sb_push_cstr,    \
        const char *:       sb_push_cstr,    \
        StrSpan:            sb_push_strspan, \
        bool:               sb_push_bool,    \
        char:               sb_push_char,    \
        signed char:        sb_push_i64,     \
        unsigned char:      sb_push_u64,     \
        short:              sb_push_i64,     \
        unsigned short:     sb_push_u64,     \
        int:                sb_push_i64,     \
        unsigned int:       sb_push_u64,     \
        long:               sb_push_i64,     \
        unsigned long:      sb_push_u64,     \
        long long:          sb_push_i64,     \
        unsigned long long: sb_push_u64,     \
        float:              sb_push_f64,     \
        double:             sb_push_f64,     \
        default:            sb_push_ptr      \
    )((sb), (elem))



static void sb_push_bool(StrBuilder *sb, bool to_push);
static void sb_push_char(StrBuilder *sb, char to_push);
static void sb_push_i64(StrBuilder *sb, int64_t to_push);
static void sb_push_u64(StrBuilder *sb, uint64_t to_push);
static void sb_push_f64(StrBuilder *sb, double to_push);
static void sb_push_str(StrBuilder *sb, Str string);
static void sb_push_cstr(StrBuilder *sb, const char *cstr);
static void sb_push_ptr(StrBuilder *sb, const void *ptr);
static void sb_push_buffer(StrBuilder *sb, const char *buf, size_t length);
static void sb_push_strspan(StrBuilder *sb, StrSpan str_span);
migi_printf_format(2, 3) static void sb_pushf(StrBuilder *sb, const char *fmt, ...);


// Create a string builder from a string
// Optionally, the arena for the builder can also be passed in.
// NOTE: If `string` was the last allocation on `opt.arena`, then no
// extra copying is done by the builder since it uses the same
// arena as it's backing store.
typedef struct {
    Arena *arena;
} SBFromOpt;

static StrBuilder sb_from_str_opt(Str string, SBFromOpt opt);
#define sb_from_str(string, ...) sb_from_str_opt((string), (SBFromOpt){__VA_ARGS__})


typedef struct {
    bool no_reset; // Whether to reset the string builder after creating the Str or cstr.
} StrBuilderOpt;

static Str sb_to_str_opt(StrBuilder *sb, StrBuilderOpt opt);
static const char *sb_to_cstr_opt(StrBuilder *sb, StrBuilderOpt opt);

#define sb_to_str(sb, ...)            sb_to_str_opt((sb), (StrBuilderOpt){__VA_ARGS__})
#define sb_to_cstr(sb, ...)           sb_to_cstr_opt((sb), (StrBuilderOpt){__VA_ARGS__})


// NOTE: `sb_reset` also resets the arena if it is owned by the string builder.
// If it doesn't own the arena (was created with `StrBuilder sb = {.arena=a};`),
// then this function simply sets the builder's `length` to 0 and `data` to
// point to the end of the arena. This keeps all previous allocations while also
// resetting the state of the builder.
void sb_reset(StrBuilder *sb);
void sb_free(StrBuilder *sb);

typedef struct {
    Temp temp;
    char *data;
    int64_t length;
    bool owns_arena;
} StrBuilderTemp;
StrBuilderTemp sb_save(StrBuilder *sb);
void sb_rewind(StrBuilder *sb, StrBuilderTemp temp);



// This needs to be a function (and not a macro) due to the weird rules on _Generic
static void sb__push_no_match(StrBuilder *sb, void *elem) {
    unused(sb);
    unused(elem);
    crash_with_message("sb_push: no types matched");
}

static void sb__init(StrBuilder *sb) {
    if (!sb->data) {
        if (!sb->arena) {
            sb->arena      = arena_init(.type=Arena_Linear);
            sb->owns_arena = true;
        }
        assertf(sb->arena->type != Arena_Chained, "StrBuilder cannot work on chained arenas");
        sb->data = (char *)((byte *)sb->arena + sb->arena->position);
    }
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
        int64_t integral   = (int64_t)to_push;
        int64_t fractional = ((int64_t)to_push - integral)*prec;
        sb_push_i64(sb, integral);
        sb_push_char(sb, '.');
        for (int64_t i = prec/10; i > 1; i /= 10) {
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

static void sb_push_ptr(StrBuilder *sb, const void *ptr) {
    sb_push_str(sb, S("0x"));
    sb_push_hex(sb, (uintptr_t)ptr);
}

static void sb_push_char(StrBuilder *sb, char to_push) {
    sb_push_buffer(sb, &to_push, 1);
}

static void sb_push_str(StrBuilder *sb, Str string) {
    sb_push_buffer(sb, string.data, string.length);
}

static void sb_push_cstr(StrBuilder *sb, const char *cstr) {
    size_t length = strlen(cstr);
    sb_push_buffer(sb, cstr, length);
}

static void sb_push_strspan(StrBuilder *sb, StrSpan str_span) {
    array_foreach(&str_span, str) {
        sb_push_str(sb, *str);
    }
}

static void sb_push_buffer(StrBuilder *sb, const char *buf, size_t length) {
    sb__init(sb);
    arena_copy(sb->arena, char, buf, length);
    sb->length += length;
}

// NOTE: sb_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void sb_pushf(StrBuilder *sb, const char *fmt, ...) {
    // This function could have passed the result of `str__format` into
    // `sb_push_buffer`. However that will cause an extra copy of the formatted
    // string. So instead it simply performs the formatting directly into
    // the arena, and extends the length by the same amount.
    sb__init(sb);
    va_list args;
    va_start(args, fmt);
    sb->length += str__format(sb->arena, fmt, args).length;
    va_end(args);
}


void sb_reset(StrBuilder *sb) {
    if (sb->owns_arena) {
        arena_reset(sb->arena);
    }
    sb->data = (char *)((byte *)sb->arena + sb->arena->position);
    sb->length = 0;
}

void sb_free(StrBuilder *sb) {
    if (sb->owns_arena) arena_free(sb->arena);
    mem_clear(sb);
}

static StrBuilder sb_from_str_opt(Str string, SBFromOpt opt) {
    StrBuilder sb = {0};
    bool copy_string = true;
    if (opt.arena) {
        assertf(opt.arena->type != Arena_Chained, "StrBuilder cannot work on chained arenas");
        sb.arena = opt.arena;
        if ((byte *)sb.arena + sb.arena->position == (byte *)string.data + string.length) {
            sb.data   = string.data;
            sb.length = string.length;
            copy_string = false;
        }
    }
    if (copy_string) sb_push_str(&sb, string);
    return sb;
}

static Str sb_to_str_opt(StrBuilder *sb, StrBuilderOpt opt) {
    Str str = str_from(sb->data, sb->length);
    if (!opt.no_reset) sb_reset(sb);
    return str;
}

// NOTE: The cstring returned from this function is not separately allocated
// and is destroyed after a subsequent push onto the string builder
static const char *sb_to_cstr_opt(StrBuilder *sb, StrBuilderOpt opt) {
    sb_push_char(sb, 0);
    const char *cstr = (const char *)sb->data;
    arena_pop(sb->arena, char, 1);
    sb->length--;
    if (!opt.no_reset) sb_reset(sb);
    return cstr;
}



StrBuilderTemp sb_save(StrBuilder *sb) {
    return (StrBuilderTemp){
        .temp         = arena_save(sb->arena),
        .data         = sb->data,
        .length       = sb->length,
        .owns_arena   = sb->owns_arena,
    };
}

void sb_rewind(StrBuilder *sb, StrBuilderTemp temp) {
    arena_rewind(temp.temp);
    sb->data         = temp.data;
    sb->arena        = temp.temp.arena;
    sb->owns_arena   = temp.owns_arena;
    sb->length       = temp.length;
}

#endif // MIGI_STRING_BUILDER_H
