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
#include "arena.h"

// TODO: forward declare all functions
// TODO: look into adding case-insensitive find and replace
// TODO: add string styling (camelcase/snakecase/etc. conversions)

typedef struct {
    const char *data;
    size_t length;
} String;

typedef struct {
    String *data;
    size_t length;
} StringSlice;

#define SV_FMT(sv) (int)(sv).length, (sv).data
#define SV(cstr) (String){(cstr), (sizeof(cstr) - 1)}

static String string_from_cstr(const char *cstr) {
    return (String){
        .data = cstr,
        .length = (cstr == NULL)? 0: strlen(cstr)
    };
}

static char *string_to_cstr(Arena *arena, String str) {
    char *cstr = arena_push_nonzero(arena, char, str.length + 1);
    if (str.data) {
        memcpy(cstr, str.data, str.length);
    }
    cstr[str.length] = 0;
    return cstr;
}

static String string_copy(Arena *arena, String str) {
    return (String){
        .data = arena_copy(arena, char, str.data, str.length),
        .length = str.length
    };
}

static bool string_eq(String a, String b) {
    if (a.length != b.length) return false;
    return !a.length || mem_eq_array(a.data, b.data, a.length);
}

static bool string_eq_cstr(String a, const char *b) {
    if (b == NULL) return a.length == 0;
    if (a.length != strlen(b)) return false;
    return memcmp(a.data, b, a.length) == 0;
}

bool string_eq_any_slice(String to_match, String *matches, size_t matches_len) {
    for (size_t i = 0; i < matches_len; i++) {
        if (string_eq(to_match, matches[i])) {
            return true;
        }
    }
    return false;
}

#define string_eq_any(to_match, ...) \
    (string_eq_any_slice((to_match), __VA_ARGS__, sizeof((__VA_ARGS__))/sizeof(*(__VA_ARGS__))))

static int64_t string_find_char(String haystack, char needle) {
    for (size_t i = 0; i < haystack.length; i++) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

static int64_t string_find_char_rev(String haystack, char needle) {
    for (int64_t i = haystack.length - 1; i >= 0; i--) {
        if (haystack.data[i] == needle) return i;
    }
    return -1;
}

// Find `needle` within `haystack`
static int64_t string_find(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (size_t i = 0; i <= haystack.length - needle.length; i++) {
        if (mem_eq_array(haystack.data + i, needle.data, needle.length)) {
            return i;
        }
    }
    return -1;
}

// Find `needle` within `haystack`, starting from the end of `haystack`
static int64_t string_find_rev(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (int64_t i = haystack.length - needle.length; i >= 0; i--) {
        if (mem_eq_array(haystack.data + i, needle.data, needle.length)) {
            return i;
        }
    }
    return -1;
}

// Slice string into [start, end) (exclusive range)
static String string_slice(String str, size_t start, size_t end) {
    assertf(start <= str.length && end <= str.length, "string_slice: index out of bounds");
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

// Take `amount` characters from beginning of string
static String string_take(String str, size_t amount) {
    assertf(amount <= str.length, "string_take: index out of bounds");
    return (String){
        .data = str.data,
        .length = amount,
    };
}

// Get index of suffix or -1 if not found
static int64_t string_find_suffix(String str, String suffix) {
    if (suffix.length > str.length) return -1;
    // memcmp forbids NULL pointers even if the size is 0
    if (!suffix.data) return 0;
    if (!str.data) return -1;

    int64_t start = str.length - suffix.length;
    return (mem_eq_array(str.data + start, suffix.data, suffix.length))? start: -1;
}

static bool string_starts_with(String str, String prefix) {
    if (prefix.length > str.length) return false;
    // memcmp forbids NULL pointers even if the size is 0
    if (!prefix.data) return true;
    if (!str.data) return false;

    return mem_eq_array(str.data, prefix.data, prefix.length);
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

// Function type for string_skip_while_ functions
// Gets passed in a character of the string and an optional `void *data`
typedef bool (string_skip_while_func) (char ch, void *data);

// Skips from start of string as long as the passed in function returns true
// The `data` argument is passed into the `skip_char` function to emulate a closure
static String string_skip_while(String str, string_skip_while_func *skip_char, void *data) {
    while (str.length > 0) {
        if (!skip_char(str.data[0], data)) break;

        str.data++;
        str.length--;
    }
    return str;
}

// Skips from end of string as long as the passed in function returns true
// The `data` argument is passed into the `skip_char` function to emulate a closure
static String string_skip_while_rev(String str, string_skip_while_func *skip_char, void *data) {
    while (str.length > 0) {
        if (!skip_char(str.data[str.length - 1], data)) break;
        str.length--;
    }
    return str;
}

// Special case for `string_skip_while`
// Probably the most common use for skipping in a string
static bool string__is_equal_chars(char ch, void *data) {
    String chars = *(String *)data;
    for (size_t i = 0; i < chars.length; i++) {
        if (ch == chars.data[i]) return true;
    }
    return false;
}

// Skips from start of string as long as one of the elements of `chars` are present
static String string_skip_chars(String str, String chars) {
    return string_skip_while(str, string__is_equal_chars, &chars);
}

// Skips from end of string as long as one of the elements of `chars` are present
static String string_skip_chars_rev(String str, String chars) {
    return string_skip_while_rev(str, string__is_equal_chars, &chars);
}

// TODO: check for other whitespace characters
// https://stackoverflow.com/a/46637343
static String string_trim_left(String str) {
    return string_skip_chars(str, SV(" \n\r\t"));
}

static String string_trim_right(String str) {
    return string_skip_chars_rev(str, SV(" \n\r\t"));
}

static String string_trim(String str) {
    return string_trim_left(string_trim_right(str));
}

static String string_to_lower(Arena *arena, String str) {
    char *lower = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        if (str.data[i] >= 'A' && str.data[i] <= 'Z') {
            lower[i] = str.data[i] + 32;
        } else {
            lower[i] = str.data[i];
        }
    }
    return (String){lower, str.length};
}

static String string_to_upper(Arena *arena, String str) {
    char *upper = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        if (str.data[i] >= 'a' && str.data[i] <= 'z') {
            upper[i] = str.data[i] - 32;
        } else {
            upper[i] = str.data[i];
        }
    }
    return (String){upper, str.length};
}

static String string_reverse(Arena *arena, String str) {
    char *reversed = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        reversed[str.length - i - 1] = str.data[i];
    }
    return (String){reversed, str.length};
}


static String string_replace(Arena *arena, String str, String find, String replace_with) {
    if (find.length == 0) {
        return (String){
            .data = arena_copy(arena, char, str.data, str.length),
            .length = str.length
        };
    }

    size_t max_length = replace_with.length * str.length;
    char *replaced = arena_push_nonzero(arena, char, max_length);

    char *replaced_at = replaced;
    while (true) {
        int64_t index = string_find(str, find);
        if (index == -1) {
            memcpy(replaced_at, str.data, str.length);
            replaced_at += str.length;
            break;
        }

        memcpy(replaced_at, str.data, index);
        replaced_at += index;
        memcpy(replaced_at, replace_with.data, replace_with.length);
        replaced_at += replace_with.length;
        str = string_skip(str, index + find.length);
    }

    size_t actual_length = replaced_at - replaced;
    arena_pop(arena, char, max_length - actual_length);
    return (String){.data = replaced, .length = actual_length};
}

typedef struct {
    String head;
    String tail;
    bool valid;
} StringCut;

static StringCut string_cut(String str, String cut_at) {
    StringCut cut = {0};
    int64_t cut_index = string_find(str, cut_at);
    if (cut_index == -1) {
        return cut;
    }
    cut.head = string_slice(str, 0, cut_index);
    cut.tail = string_slice(str, cut_index + cut_at.length, str.length);
    cut.valid = true;
    return cut;
}

typedef struct {
    bool is_over;
    String string;
} SplitIterator;

static SplitIterator string_split_next(String *str, String split_at) {
    SplitIterator it = {0};

    if (str->length == 0 || split_at.length == 0) {
        it.is_over = true;
        it.string = *str;
        return it;
    }

    int64_t index = string_find(*str, split_at);
    if (index == -1) {
        it.is_over = true;
        it.string = *str;
        return it;
    }

    it.string = string_slice(*str, 0, index);
    *str = string_skip(*str, index + split_at.length);
    return it;
}


// Splits up to the first occurrence of any of the characters of delimiter
// Always returns the nearest match if multiple characters are found
// Eg: string_split_chars_next("a+-b", "-+") will return "a", then "", and finally "b"
static SplitIterator string_split_chars_next(String *str, String delims) {
    SplitIterator it = {0};

    if (str->length == 0 || delims.length == 0) {
        it.is_over = true;
        it.string = *str;
        return it;
    }

    int64_t first_match = INT64_MAX;
    int64_t last_match = INT64_MIN;
    for (size_t i = 0; i < delims.length; i++) {
        int64_t new_index = string_find_char(*str, delims.data[i]);
        if (new_index != -1) {
            first_match = migi_min(first_match, new_index);
            last_match = migi_max(last_match, new_index);
        }
    }
    if ((first_match == -1 || first_match == INT64_MAX) && (last_match < 0)) {
        it.is_over = true;
        it.string = *str;
        return it;
    }

    it.string = string_slice(*str, 0, first_match);
    *str = string_skip(*str, first_match + 1);
    return it;
}

// Iterator-like macros to loop over each string split
// Use `it.split` to get the splits each time
#define string_split_foreach(str, split_at, it)            \
    for (struct { String _copy;                            \
                  String split;                            \
                  SplitIterator _it;                       \
                  bool over; }                             \
        it = { ._copy = (str), };                          \
        it._it = string_split_next(&it._copy, (split_at)), \
            it.split = it._it.string,                      \
            !it.over;                                      \
        it._it.is_over? it.over = true: it.over)


#define string_split_chars_foreach(str, delims, it)            \
    for (struct { String _copy;                                \
                  String split;                                \
                  SplitIterator _it;                           \
                  bool over; }                                 \
        it = { ._copy = (str), };                              \
        it._it = string_split_chars_next(&it._copy, (delims)), \
            it.split = it._it.string,                          \
            !it.over;                                          \
        it._it.is_over? it.over = true: it.over)


static inline uint64_t string_hashfnv(String string, uint64_t seed) {
    uint64_t h = seed? seed: 0x100;
    for (size_t i = 0; i < string.length; i++) {
        h ^= string.data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

static inline uint64_t string_hash(String string) {
    return string_hashfnv(string, 0);
}

static String string__format(Arena *arena, const char *fmt, va_list args) {
    va_list args_saved;
    va_copy(args_saved, args);

    // TODO: change this to simply calling vsnprintf first to get the actual
    // and then to actually construct the formatted string
    int reserved = 1024;
    char *mem = arena_push(arena, char, reserved);
    int actual = vsnprintf(mem, reserved, fmt, args);
    // vsnprintf doesnt count the null terminator
    actual += 1;

    if (actual > reserved) {
        arena_pop(arena, char, reserved);
        mem = arena_push(arena, char, actual);
        vsnprintf(mem, actual, fmt, args_saved);
    } else if (actual < reserved) {
        arena_pop(arena, char, reserved - actual);
    }
    // pop off the null terminator
    arena_pop(arena, char, 1);

    va_end(args_saved);
    // actual includes the null terminator
    return (String){ .data = mem, .length = actual - 1 };
}

migi_printf_format(2, 3) static String stringf(Arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String result = string__format(arena, fmt, args);
    va_end(args);
    return result;
}


typedef struct {
    bool ok;
    String string;
} StringResult;

// TODO: use linux syscalls instead of C stdlib
static StringResult read_file(Arena *arena, String filepath) {
    Checkpoint c = arena_save(arena);
    FILE *file = fopen(string_to_cstr(arena, filepath), "r");
    arena_rewind(c);

    StringResult result = {0};
    if (!file) {
        fprintf(stderr, "%s: failed to open file `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        return result;
    }

    fseek(file, 0, SEEK_END);
    int64_t file_pos = ftell(file);
    if (file_pos == -1) {
        fprintf(stderr, "%s: couldnt read file position in `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        fclose(file);
        return result;
    }

    // file position cannot be negative at this point
    size_t length = file_pos;
    rewind(file);

    char *buf = arena_push(arena, char, length);
    int64_t n = fread(buf, sizeof(char), length, file);
    if (n != file_pos || ferror(file)) {
        fprintf(stderr, "%s: failed to read from file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return result;
    }

    fclose(file);
    result = (StringResult){
        .ok = true,
        .string = (String){
            .data = buf,
            .length = length,
        }
    };
    return result;
}

// TODO: use linux syscalls instead of C stdlib
static bool write_file(String string, String filepath, Arena *temp) {
    Checkpoint c = arena_save(temp);
    FILE *file = fopen(string_to_cstr(temp, filepath), "w");
    arena_rewind(c);

    if (!file) {
        fprintf(stderr, "%s: failed to open file `%.*s`: %s\n", __func__, SV_FMT(filepath), strerror(errno));
        return false;
    }
    size_t n = fwrite(string.data, sizeof(char), string.length, file);
    if (n != string.length) {
        fprintf(stderr, "%s: failed to write to file `%.*s`: \n", __func__, SV_FMT(filepath));
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

#endif // MIGI_STRING_H
