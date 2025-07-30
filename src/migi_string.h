#ifndef MIGI_STRING_H
#define MIGI_STRING_H

#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "migi.h"
#include "linear_arena.h"

typedef struct {
    const char *data;
    size_t length;
} String;

#define SV_FMT(sv) (int)(sv).length, (sv).data
#define SV(cstr) (String){(cstr), (sizeof(cstr) - 1)}

typedef struct {
    LinearArena arena;
} StringBuilder;

#define STRING_BUILDER_INIT_CAPACITY 4

static void sb_push(StringBuilder *sb, char to_push) {
    *lnr_arena_push(&sb->arena, char, 1) = to_push;
}

static void sb_push_string(StringBuilder *sb, String string) {
    memcpy(lnr_arena_push(&sb->arena, char, string.length), string.data, string.length);
}

static void sb_push_buffer(StringBuilder *sb, char *buf, size_t len) {
    memcpy(lnr_arena_push(&sb->arena, char, len), buf, len);
}

// NOTE: sb_pushf doesnt append a null terminator at the end
// of the format string unlike regular sprintf
static void sb_pushf(StringBuilder *sb, const char *fmt, ...) {
    va_list args1;
    va_start(args1, fmt);

    va_list args2;
    va_copy(args2, args1);

    int reserved = 1024;
    int actual = vsnprintf(lnr_arena_push(&sb->arena, char, reserved), reserved, fmt, args1);
    // vsnprintf doesnt count the null terminator
    actual += 1;

    if (actual > reserved) {
        lnr_arena_pop(&sb->arena, char, reserved);
        vsnprintf(lnr_arena_push(&sb->arena, char, actual), actual, fmt, args2);
    } else if (actual < reserved) {
        lnr_arena_pop(&sb->arena, char, abs_difference(actual, reserved));
    }
    // pop off the null terminator
    lnr_arena_pop(&sb->arena, char, 1);

    va_end(args2);
    va_end(args1);
}

static StringBuilder sb_from_string(String string) {
    StringBuilder sb = {0};
    sb_push_string(&sb, string);
    return sb;
}

static String sb_to_string(StringBuilder *sb) {
    return (String){
        .data = (char *)sb->arena.data,
        .length = sb->arena.length
    };
}

static char *sb_to_cstr(StringBuilder *sb) {
    sb_push(sb, 0);
    return (char *)sb->arena.data;
}

static String string_from_cstr(const char *cstr) {
    return (String){
        .data = cstr,
        .length = (cstr == NULL)? 0: strlen(cstr)
    };
}

static bool string_eq(String a, String b) {
    if (a.length != b.length) return false;
    return migi_mem_eq(a.data, b.data, a.length);
}

static int64_t string_find_char(String haystack, char needle) {
    for (size_t i = 0; i < haystack.length; i++) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int64_t string_find_char_rev(String haystack, char needle) {
    for (int i = haystack.length - 1; i >= 0; i--) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int64_t string_find(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (size_t i = 0; i <= haystack.length - needle.length; i++) {
        size_t j = 0;
        for (; j < needle.length; j++) {
            if (haystack.data[i + j] != needle.data[j]) break;
        }
        if (j == needle.length) return i;
    }
    return -1;
}

static int64_t string_find_rev(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (int i = haystack.length - needle.length; i >= 0; i--) {
        size_t j = 0;
        for (; j < needle.length; j++) {
            if (haystack.data[i + j] != needle.data[j]) break;
        }
        if (j == needle.length) return i;
    }
    return -1;
}

// Slice string into [start, end) (exclusive range)
static String string_slice(String str, size_t start, size_t end) {
    assertf(start < str.length && end <= str.length, "string_slice: index out of bounds");
    return (String){
        .data = str.data + start,
        .length = end - start
    };
}

// Skip `amount` characters from beginning of string
// If `amount` == `str.length`, then returns an empty string
static String string_skip(String str, size_t amount) {
    assertf(amount <= str.length, "string_skip: index out of bounds");
    return (String){
        .data = str.data + amount,
        .length = str.length - amount,
    };
}

// Get index of suffix or -1 if not found
static int string_find_suffix(String str, String suffix) {
    if (suffix.length > str.length) return -1;
    int start = str.length - suffix.length;
    return (migi_mem_eq(str.data + start, suffix.data, suffix.length))? start: -1;
}

static bool string_starts_with(String str, String prefix) {
    if (prefix.length > str.length) return false;
    return migi_mem_eq(str.data, prefix.data, prefix.length);
}

static bool string_ends_with(String str, String suffix) {
    return string_find_suffix(str, suffix) != -1;
}

// Slice off the prefix from the string
// Returns the original string if the prefix was not found
static String string_cut_prefix(String str, String prefix) {
    if (!string_starts_with(str, prefix)) {
        return str;
    }
    return string_skip(str, prefix.length);
}

// Slice off the suffix from the string
// Returns the original string if the suffix was not found
static String string_cut_suffix(String str, String suffix) {
    int64_t suffix_start = string_find_suffix(str, suffix);
    if (suffix_start == -1) {
        return str;
    }
    return string_slice(str, 0, suffix_start);
}

typedef struct {
    String *data;
    size_t length;
    size_t capacity;
} Strings;

#define SPLIT_SKIP_EMPTY 0x1

static Strings string_split_ex(String str, String delimiter, int flags) {
    Strings strings = {0};
    if (delimiter.length == 0) return strings;

    int64_t substr_start = string_find(str, delimiter);
    while (substr_start != -1 && str.length > 0) {
        String substr = (String){
            .data = str.data,
            .length = substr_start
        };

        if (!(flags & SPLIT_SKIP_EMPTY) || substr.length != 0) {
            array_add(&strings, substr);
        }
        str = string_skip(str, substr_start + delimiter.length);
        substr_start = string_find(str, delimiter);
    }
    String remaining_part = (String){
        .data = str.data,
        .length = str.length
    };
    if (!(flags & SPLIT_SKIP_EMPTY) || remaining_part.length != 0) {
        array_add(&strings, remaining_part);
    }
    return strings;
}


static Strings string_split_chars_ex(String str, char *delims, size_t delims_len, int flags) {
    Strings strings = {0};
    size_t start = 0;
    size_t length = 0;
    for (size_t i = 0; i < str.length; i++) {
        bool delim_found = false;
        for (size_t j = 0; j < delims_len; j++) {
            if (delims[j] == str.data[i]) {
                String substr = (String){
                    .data = str.data + start,
                    .length = length
                };
                if (!(flags & SPLIT_SKIP_EMPTY) || substr.length != 0) {
                    array_add(&strings, substr);
                }
                length = 0;
                start = i + 1;
                delim_found = true;
                break;
            }
        }
        if (!delim_found) length++;
    }
    String remaining_part = (String){
        .data = str.data + start,
        .length = length
    };
    if (!(flags & SPLIT_SKIP_EMPTY) || remaining_part.length != 0) {
        array_add(&strings, remaining_part);
    }
    return strings;
}

// Convenience macros which set flags to 0
#define string_split(str, delimiter) \
    (string_split_ex((str), (delimiter), 0))

#define string_split_chars(str, delims, delims_len) \
    (string_split_chars_ex((str), (delims), (delims_len), 0))



// TODO: use linux syscalls instead of C stdlib
static bool read_file(StringBuilder *builder, String filepath) {
    StringBuilder filepath_builder = sb_from_string(filepath);
    FILE *file = fopen(sb_to_cstr(&filepath_builder), "r");
    if (!file) {
        fprintf(stderr, "%s: failed to open file `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        return false;
    }

    fseek(file, 0, SEEK_END);
    int64_t file_pos = ftell(file);
    if (file_pos == -1) {
        fprintf(stderr, "%s: couldnt read file position in `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        fclose(file);
        return false;
    }

    // file position cannot be negative at this point
    size_t size = file_pos;
    rewind(file);

    int n = fread(lnr_arena_push(&builder->arena, char, size), sizeof(char), size, file);
    if (n != file_pos || ferror(file)) {
        fprintf(stderr, "%s: failed to read from file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

// TODO: use linux syscalls instead of C stdlib
static bool write_file(StringBuilder *sb, String filepath) {
    StringBuilder filepath_builder = sb_from_string(filepath);
    FILE *file = fopen(sb_to_cstr(&filepath_builder), "w");
    if (!file) {
        fprintf(stderr, "%s: failed to open file `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        return false;
    }
    size_t n = fwrite(sb->arena.data, sizeof(char), sb->arena.length, file);
    if (n != sb->arena.length) {
        fprintf(stderr, "%s: failed to write to file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

#endif // MIGI_STRING_H
