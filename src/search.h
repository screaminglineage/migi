#ifndef SEARCH_H
#define SEARCH_H

// Linear and Binary Search Functions
// There are 2 kinds of functions: search/binary_search and search_key/binary_search_key
// The `search_key` functions can be optionally passed a fields of a struct to compare
// by instead of having to write a custom comparator for each kind.
// See below for more info.

#include "migi_core.h"
#include "migi_string.h"

typedef int (*BinSearchCompFn)(void *left, void *right, void *user_data);

typedef struct {
    BinSearchCompFn comparator;
    void *user_data;
} SearchOpt;

static size_t binary_search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt);

// Convenience macros
//
// Search an array
#define search(arr, length, key, ...)                                                \
    search_opt((byte *)(check_type(type_of(key), arr)), sizeof(*(arr)), (length), 0, \
            binarysearch__addr_of(*(arr), (key)),(SearchOpt){ .comparator = compare__func_for(*(arr)), __VA_ARGS__ })


// Search a particular field of a struct with a key
// For example:
// struct { int a, b; } *arr = { /* ... */ };
// search_key(arr, array_len(arr), a, 12) will only search for items where the field `a` is `12`
#define search_key(arr, length, field, key, ...)                                          \
    (check_type_value(type_of((arr)->field), (key)),                                      \
    search_opt((byte *)arr, sizeof(*(arr)), (length),                                     \
            offsetof(type_of(*(arr)), field), binarysearch__addr_of((arr)->field, (key)), \
            (SearchOpt){ .comparator = compare__func_for((arr)->field), __VA_ARGS__ }))

// Binary search an array
#define binary_search(arr, length, key, ...)                                                \
    binary_search_opt((byte *)(check_type(type_of(key), arr)), sizeof(*(arr)), (length), 0, \
            binarysearch__addr_of(*(arr), (key)),(SearchOpt){ .comparator = compare__func_for(*(arr)), __VA_ARGS__ })


// Binary search a particular field of a struct with a key
// For example:
// struct { int a, b; } *arr = { /* ... */ };
// binary_search_key(arr, array_len(arr), a, 12) will only search for items where the field `a` is `12`
#define binary_search_key(arr, length, field, key, ...)                                   \
    (check_type_value(type_of((arr)->field), (key)),                                      \
    binary_search_opt((byte *)arr, sizeof(*(arr)), (length),                              \
            offsetof(type_of(*(arr)), field), binarysearch__addr_of((arr)->field, (key)), \
            (SearchOpt){ .comparator = compare__func_for((arr)->field), __VA_ARGS__ }))



// Some common comparison functions
static int compare_u64(void *left, void *right, void *user_data);
static int compare_i32(void *left, void *right, void *user_data);
static int compare_i64(void *left, void *right, void *user_data);
static int compare_f32(void *left, void *right, void *user_data);
static int compare_f64(void *left, void *right, void *user_data);
static int compare_str(void *left, void *right, void *user_data);
static int compare_cstr(void *left, void *right, void *user_data);
static int compare_char(void *left, void *right, void *user_data);

static size_t binary_search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt) {
    assertf(opt.comparator, "no suitable comparison function found, provide one manually");

    // TODO: do some performance testing to see if it really
    // is faster for this particular configuration
    // branchless version of binary search
#ifdef BRANCHLESS
    byte *base = arr;
    size_t len = length;

    while (len > 1) {
        size_t half = len/2;
        byte *half_ptr = base + elem_size*half + field_offset;
        int result = opt.comparator(half_ptr, key, opt.user_data);
        base += result < 0? elem_size*half: 0;
        len  -= half;
    }

    byte *elem = base + field_offset;
    bool less_than_key = opt.comparator(elem, key, opt.user_data) < 0;
    size_t idx = (base - arr) + less_than_key*elem_size;

    bool found = false;
    if (idx < length*elem_size) {
        found = opt.comparator(arr + idx + field_offset, key, opt.user_data) == 0;
    }
    return found? idx/elem_size: length;

#else
    size_t left = 0;
    size_t right = length;

    while (left < right) {
        size_t mid = left + (right - left)/2;
        byte *elem = arr + elem_size*mid + field_offset;

        int result = opt.comparator(elem, key, opt.user_data);
        if (result == 0) return mid;

        if (result > 0) {
            right = mid;
        } else if (result < 0) {
            left  = mid + 1;
        }
    }
    return length;
#endif // #ifdef BRANCHLESS
}

static size_t search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt) {
    assertf(opt.comparator, "no suitable comparison function found, provide one manually");

    for (size_t i = 0; i < length; i++) {
        byte *elem = arr + elem_size*i + field_offset;
        if (opt.comparator(elem, key, opt.user_data) == Ordering_Eq) return i;
    }
    return length;
}

static int compare_u64(void *left, void *right, void *user_data) {
    unused(user_data);
    uint64_t *a = left;
    uint64_t *b = right;
    return *a - *b;
}

static int compare_i32(void *left, void *right, void *user_data) {
    unused(user_data);
    int32_t *a = left;
    int32_t *b = right;
    return *a - *b;
}

static int compare_i64(void *left, void *right, void *user_data) {
    unused(user_data);
    int64_t *a = left;
    int64_t *b = right;
    return *a - *b;
}


static int compare_f32(void *left, void *right, void *user_data) {
    unused(user_data);
    float *a = left;
    float *b = right;
    return *a - *b;
}

static int compare_f64(void *left, void *right, void *user_data) {
    unused(user_data);
    double *a = left;
    double *b = right;
    return *a - *b;
}

static int compare_str(void *left, void *right, void *user_data) {
    unused(user_data);
    Str *a = left;
    Str *b = right;
    return str_cmp(*a, *b, 0);
}

static int compare_cstr(void *left, void *right, void *user_data) {
    unused(user_data);
    char **a = left;
    char **b = right;
    return strcmp(*a, *b);
}

static int compare_char(void *left, void *right, void *user_data) {
    unused(user_data);
    char *a = left;
    char *b = right;
    return *a - *b;
}

#define compare__func_for(type) \
    _Generic((type),            \
        Str:      compare_str,  \
        char *:   compare_cstr, \
        char:     compare_char, \
        float:    compare_f32,  \
        double:   compare_f64,  \
        int32_t:  compare_i32,  \
        int64_t:  compare_i64,  \
        uint64_t: compare_u64,  \
        default:  NULL          \
    )

#define binarysearch__addr_of(T, x) ((type_of(T)[1]){x})


#endif // SEARCH_H
