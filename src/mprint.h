#include "migi.h"

// Type safe variadic (somewhat) printf
//
// The following macros are available:
// All the functions support upto 12 variadic arguments
//
// Python like print functions. The `_ex` version allows separator and end
// characters to be specified as the 1st and 2nd arguments respectively.
// mprint(...)
// mfprint(out, ...)
// mprint_ex(sep, end, ...)
// mfprint_ex(out, sep, end, ...)
//
// C like printf but the only a single `%` placeholder needs to be specified for each
// data type. For example, mprintf("%: %-%-%: %", S("Example"), 2026, 5LL, 23U, -1.323);
// mprintf(fmt, ...)
// mfprintf(out, fmt, ...)

typedef enum {
    MPrint_Bool,

    MPrint_Char,
    MPrint_SChar,
    MPrint_UChar,

    MPrint_Short,
    MPrint_UShort,

    MPrint_Int,
    MPrint_UInt,

    MPrint_Long,
    MPrint_ULong,

    MPrint_LongLong,
    MPrint_ULongLong,

    MPrint_Float,
    MPrint_Double,

    MPrint_CStr,
    MPrint_Str,
    MPrint_Ptr,
} MPrint_Type;


typedef struct {
    MPrint_Type type;
    union {
        bool b;

        char c;
        signed char sc;
        unsigned char uc;

        short s;
        unsigned short us;

        int i;
        unsigned int ui;

        long l;
        unsigned long ul;

        long long ll;
        unsigned long long ull;

        float f;
        double d;

        const char *cstr;
        Str str;
        const void *ptr;
    };
} MPrint;


static MPrint mprint_bool(bool e)                    { return (MPrint){ .type = MPrint_Bool,      .b = e    }; }
static MPrint mprint_char(char e)                    { return (MPrint){ .type = MPrint_Char,      .c = e    }; }
static MPrint mprint_schar(signed char e)            { return (MPrint){ .type = MPrint_SChar,     .sc = e   }; }
static MPrint mprint_uchar(unsigned char e)          { return (MPrint){ .type = MPrint_UChar,     .uc = e   }; }
static MPrint mprint_short(short e)                  { return (MPrint){ .type = MPrint_Short,     .s = e    }; }
static MPrint mprint_ushort(unsigned short e)        { return (MPrint){ .type = MPrint_UShort,    .us = e   }; }
static MPrint mprint_int(int e)                      { return (MPrint){ .type = MPrint_Int,       .i = e    }; }
static MPrint mprint_uint(unsigned int e)            { return (MPrint){ .type = MPrint_UInt,      .ui = e   }; }
static MPrint mprint_long(long e)                    { return (MPrint){ .type = MPrint_Long,      .l = e    }; }
static MPrint mprint_ulong(unsigned long e)          { return (MPrint){ .type = MPrint_ULong,     .ul = e   }; }
static MPrint mprint_longlong(long long e)           { return (MPrint){ .type = MPrint_LongLong,  .ll = e   }; }
static MPrint mprint_ulonglong(unsigned long long e) { return (MPrint){ .type = MPrint_ULongLong, .ull = e  }; }
static MPrint mprint_float(float e)                  { return (MPrint){ .type = MPrint_Float,     .f = e    }; }
static MPrint mprint_double(double e)                { return (MPrint){ .type = MPrint_Double,    .d = e    }; }
static MPrint mprint_cstr(const char * e)            { return (MPrint){ .type = MPrint_CStr,      .cstr = e }; }
static MPrint mprint_str(Str e)                      { return (MPrint){ .type = MPrint_Str,       .str = e  }; }
static MPrint mprint_ptr(const void *e)              { return (MPrint){ .type = MPrint_Ptr,       .ptr = e  }; }


void mprint_type(FILE *out, MPrint data) {
    switch (data.type) {
        case MPrint_Bool      : fprintf(out, "%.*s", SArg(bool_to_str(data.b)))           ; break;
        case MPrint_Char      : fprintf(out, "%c",   data.c)                              ; break;
        case MPrint_SChar     : fprintf(out, "%c",   data.sc)                             ; break;
        case MPrint_UChar     : fprintf(out, "%c",   data.uc)                             ; break;
        case MPrint_Short     : fprintf(out, "%hd",  data.s)                              ; break;
        case MPrint_UShort    : fprintf(out, "%hu",  data.us)                             ; break;
        case MPrint_Int       : fprintf(out, "%d",   data.i)                              ; break;
        case MPrint_UInt      : fprintf(out, "%u",   data.ui)                             ; break;
        case MPrint_Long      : fprintf(out, "%ld",  data.l)                              ; break;
        case MPrint_ULong     : fprintf(out, "%lu",  data.ul)                             ; break;
        case MPrint_LongLong  : fprintf(out, "%lld", data.ll)                             ; break;
        case MPrint_ULongLong : fprintf(out, "%llu", data.ull)                            ; break;
        case MPrint_Float     : fprintf(out, "%f",   data.f)                              ; break;
        case MPrint_Double    : fprintf(out, "%f",   data.d)                              ; break;
        case MPrint_CStr      : fprintf(out, "%s",   data.cstr)                           ; break;
        case MPrint_Str       : fprintf(out, "%.*s", (int)data.str.length, data.str.data) ; break;
        case MPrint_Ptr       : fprintf(out, "%p",   data.ptr)                            ; break;
    }
}

void mprint_impl(FILE *out, const char *sep, const char *end, MPrint *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        mprint_type(out, arr[i]);
        fprintf(out, "%s", sep);
    }
    fprintf(out, "%s", end);
}

void mprintf_impl(FILE *out, Str fmt, MPrint *arr, size_t n) {
    size_t arr_i = 0;
    size_t i = 0;
    while (i < fmt.length) {
        int64_t index = str_find(fmt, S("%"));
        bool found = (size_t)index < fmt.length;
        Str skipped = str_take(fmt, index);
        fprintf(out, "%.*s", SArg(skipped));

        if (found) {
            assertf(arr_i < n, "More `%%` placeholders than data arguments");
            mprint_type(out, arr[arr_i++]);
        }

        fmt = str_skip(fmt, index + 1);
        i = 0;
    }
    assertf(arr_i == n, "Unused data arguments remain: %zu were provided, but only %zu were used", n, arr_i);
    fprintf(out, "%.*s", SArg(fmt));
}

#define get_mprint_type(e)                    \
    _Generic((e),                             \
        bool:               mprint_bool,      \
        char:               mprint_char,      \
        signed char:        mprint_schar,     \
        unsigned char:      mprint_uchar,     \
        short:              mprint_short,     \
        unsigned short:     mprint_ushort,    \
        int:                mprint_int,       \
        unsigned int:       mprint_uint,      \
        long:               mprint_long,      \
        unsigned long:      mprint_ulong,     \
        long long:          mprint_longlong,  \
        unsigned long long: mprint_ulonglong, \
        float:              mprint_float,     \
        double:             mprint_double,    \
        const char *:       mprint_cstr,      \
        char *:             mprint_cstr,      \
        Str:                mprint_str,       \
        const void *:       mprint_ptr,       \
        default:            mprint_ptr        \
    )((e))

#define apply_1(macro, arg)        macro(arg)
#define apply_2(macro, arg, ...)   macro(arg), apply_1(macro, __VA_ARGS__)
#define apply_3(macro, arg, ...)   macro(arg), apply_2(macro, __VA_ARGS__)
#define apply_4(macro, arg, ...)   macro(arg), apply_3(macro, __VA_ARGS__)
#define apply_5(macro, arg, ...)   macro(arg), apply_4(macro, __VA_ARGS__)
#define apply_6(macro, arg, ...)   macro(arg), apply_5(macro, __VA_ARGS__)
#define apply_7(macro, arg, ...)   macro(arg), apply_6(macro, __VA_ARGS__)
#define apply_8(macro, arg, ...)   macro(arg), apply_7(macro, __VA_ARGS__)
#define apply_9(macro, arg, ...)   macro(arg), apply_8(macro, __VA_ARGS__)
#define apply_10(macro, arg, ...)  macro(arg), apply_9(macro, __VA_ARGS__)
#define apply_11(macro, arg, ...)  macro(arg), apply_10(macro, __VA_ARGS__)
#define apply_12(macro, arg, ...)  macro(arg), apply_11(macro, __VA_ARGS__)
#define apply_13(macro, arg, ...)  macro(arg), apply_12(macro, __VA_ARGS__)
#define apply_14(macro, arg, ...)  macro(arg), apply_13(macro, __VA_ARGS__)

#define va_args_14th(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, ...) _14
#define va_args_count(...) \
    va_args_14th(__VA_ARGS__ __VA_OPT__(,) 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define apply_all(macro, ...) \
    macro_concat(apply_, va_args_count(__VA_ARGS__))(macro, __VA_ARGS__)

// The actual number of supported arguments is 1 less than the total number of arguments
// This enables checking for if extra arguments are provided
#define check_arg_count(...) \
    static_assert(va_args_count(__VA_ARGS__) <= 12, "mprint only supports upto 12 arguments")

#define mprint(...)                                                                                           \
do {                                                                                                          \
    check_arg_count(__VA_ARGS__);                                                                             \
    mprint_impl(stdout, " ", "\n", (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

#define mprint_ex(sep, end, ...)                                                                             \
do {                                                                                                         \
    check_arg_count(__VA_ARGS__);                                                                            \
    mprint_impl(stdout, sep, end, (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

#define mprintf(fmt, ...)                                                                                           \
do {                                                                                                                \
    check_arg_count(__VA_ARGS__);                                                                                   \
    mprintf_impl(stdout, S(fmt), (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

#define mfprint(out, ...)                                                                                          \
do {                                                                                                               \
    check_arg_count(__VA_ARGS__);                                                                                  \
    mprint_impl(out, " ", "\n", (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

#define mfprint_ex(out, sep, end, ...)                                                                            \
do {                                                                                                              \
    check_arg_count(__VA_ARGS__);                                                                                 \
    mprint_impl(out, sep, end, (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

#define mfprintf(out, fmt, ...)                                                                                  \
do {                                                                                                             \
    check_arg_count(__VA_ARGS__);                                                                                \
    mprintf_impl(out, S(fmt), (MPrint[]){ apply_all(get_mprint_type, __VA_ARGS__)}, va_args_count(__VA_ARGS__)); \
} while (0)

