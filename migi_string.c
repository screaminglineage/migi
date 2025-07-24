#ifndef MIGI_STRING_C
#define MIGI_STRING_C

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "migi.c"

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

void sb_push_str(StringBuilder *sb, String string) {
    memcpy(array_reserve(sb, string.length), string.data, string.length);
    sb->length += string.length;
}

void sb_push_buffer(StringBuilder *sb, char *buf, size_t len) {
    memcpy(array_reserve(sb, len), buf, len);
    sb->length += len;
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

static String string_from_cstr(const char *cstr) {
    return (String){
        .data = cstr,
        .length = strlen(cstr)
    };
}

static bool string_eq(String a, String b) {
    if (a.length != b.length) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

static int string_find_char(String haystack, char needle) {
    for (size_t i = 0; i < haystack.length; i++) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int string_find_char_rev(String haystack, char needle) {
    for (size_t i = haystack.length - 1; i >= 0; i--) {
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

static String string_slice(String target, size_t index) {
    assert(target.length > index && "Index out of bounds");
    return (String){
        .data = target.data + index,
        .length = target.length - index,
    };
}

// Returns index of suffix
static int string_find_suffix(String target, String suffix) {
    if (suffix.length > target.length) return -1;
    int end = target.length - suffix.length;
    return (memcmp(target.data + end, suffix.data, suffix.length) == 0)? end: -1;
}

// Slice off the suffix from the string
// Returns the original string if the suffix was not found
static String string_cut_suffix(String target, String suffix) {
    int suffix_start = string_find_suffix(target, suffix);
    if (suffix_start == -1) return target;
    return (String){
        .data = target.data,
        .length = suffix_start
    };
}

static bool string_ends_with(String target, String suffix) {
    return string_find_suffix(target, suffix) != -1;
}

static bool string_starts_with(String target, String prefix) {
    if (prefix.length > target.length) return false;
    return memcmp(target.data, prefix.data, prefix.length) == 0;
}

static bool read_to_string(StringBuilder *builder, String filepath) {
    StringBuilder filepath_builder = sb_from_string(filepath);
    sb_push(&filepath_builder, 0);
    filepath = sb_to_string(&filepath_builder);

    FILE *file = fopen(filepath.data, "r");
    if (!file) {
        fprintf(stderr, "bass: failed to open source file `%.*s`: %s\n", SV_FMT(filepath), strerror(errno));
        return false;
    }
    fseek(file, 0, SEEK_END);
    int64_t file_pos = ftell(file);
    if (file_pos == -1) {
        fprintf(stderr, "bass: couldnt read file position in `%.*s`: %s\n", SV_FMT(filepath), strerror(errno));
        return false;
    }

    // file position cannot be negative at this point
    size_t size = file_pos;
    rewind(file);

    fread(sb_alloc_atleast(builder, size), size, 1, file);
    builder->length += size;

    fclose(file);
    return true;
}

#endif // MIGI_STRING_C
