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

typedef struct {
    const char *data;
    size_t length;
} String;

#define SV_FMT(sv) (int)(sv).length, (sv).data
#define SV(cstr) (String){(cstr), (sizeof(cstr) - 1)}

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

#define STRING_BUILDER_INIT_CAPACITY 4

static void sb_push(StringBuilder *sb, char to_push) {
    *array_reserve(sb, 1) = to_push;
    sb->length += 1;
}

static void sb_push_str(StringBuilder *sb, String string) {
    memcpy(array_reserve(sb, string.length), string.data, string.length);
    sb->length += string.length;
}

static void sb_push_buffer(StringBuilder *sb, char *buf, size_t len) {
    memcpy(array_reserve(sb, len), buf, len);
    sb->length += len;
}

// TODO: Try making a custom sprintf like function instead
static void sb_pushf(StringBuilder *sb, const char *fmt, ...) {
    va_list args1;
    va_start(args1, fmt);

    va_list args2;
    va_copy(args2, args1);

    int reserved = 64;
    int actual = vsnprintf(array_reserve(sb, reserved), reserved, fmt, args1);

    if (actual > reserved) {
        // array_reserve doesnt increment length, so its fine to reserve again
        // the previous space will simply be reused

        // TODO: resize allocation to what is actually needed
        vsnprintf(array_reserve(sb, actual), actual, fmt, args2);
    }

    // TODO: resize allocation to what is actually needed
    sb->length += actual;

    va_end(args2);
    va_end(args1);
}

static StringBuilder sb_from_string(String string) {
    StringBuilder sb = {0};
    sb_push_str(&sb, string);
    return sb;
}

static String sb_to_string(StringBuilder *sb) {
    return (String){
        .data = sb->data,
        .length = sb->length
    };
}

static const char *sb_to_cstr(StringBuilder *sb) {
    sb_push(sb, 0);
    return sb->data;
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

static int string_find_char(String haystack, char needle) {
    for (size_t i = 0; i < haystack.length; i++) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int string_find_char_rev(String haystack, char needle) {
    for (int i = haystack.length - 1; i >= 0; i--) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int string_find(String haystack, String needle) {
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

static int string_find_rev(String haystack, String needle) {
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

static String string_skip(String str, size_t index) {
    assertf(index < str.length, "string_skip: index out of bounds");
    return (String){
        .data = str.data + index,
        .length = str.length - index,
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
    int suffix_start = string_find_suffix(str, suffix);
    if (suffix_start == -1) {
        return str;
    }
    return string_slice(str, 0, suffix_start);
}

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

    int n = fread(array_reserve(builder, size), sizeof(*builder->data), size, file);
    if (n != file_pos || ferror(file)) {
        fprintf(stderr, "%s: failed to read from file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return false;
    }
    builder->length += size;

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
    size_t n = fwrite(sb->data, sizeof(*sb->data), sb->length, file);
    if (n != sb->length) {
        fprintf(stderr, "%s: failed to write to file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

#endif // MIGI_STRING_H
