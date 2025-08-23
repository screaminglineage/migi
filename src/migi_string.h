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
#include "arena.h"

// TODO: forward declare all functions
// TODO: dont pass in pointers to string_builder for functions that dont modify it
// TODO: look into adding case-insensitive find and replace

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

typedef struct {
    LinearArena arena;
} StringBuilder;

migi_printf_format(2, 3) static void sb_pushf(StringBuilder *sb, const char *fmt, ...);

static void sb_push(StringBuilder *sb, char to_push) {
    *lnr_arena_push(&sb->arena, char, 1) = to_push;
}

static void sb_push_string(StringBuilder *sb, String string) {
    memcpy(lnr_arena_push(&sb->arena, char, string.length), string.data, string.length);
}

static void sb_push_cstr(StringBuilder *sb, char *cstr) {
    size_t length = strlen(cstr);
    lnr_arena_strdup(&sb->arena, cstr, length);
}

static void sb_push_buffer(StringBuilder *sb, char *buf, size_t length) {
    lnr_arena_strdup(&sb->arena, buf, length);
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

void sb_reset(StringBuilder *sb) {
    // TODO: should this pop off the data?
    // lnr_arena_pop(&sb->arena, char, sb->arena.length);
    sb->arena.length = 0;
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
    return !a.length || mem_eq(a.data, b.data, a.length);
}

bool _string_eq_any(String to_match, String *matches, size_t matches_len) {
    for (size_t i = 0; i < matches_len; i++) {
        if (string_eq(to_match, matches[i])) {
            return true;
        }
    }
    return false;
}

#define string_eq_any(to_match, ...) \
    (_string_eq_any((to_match), __VA_ARGS__, sizeof((__VA_ARGS__))/sizeof(*(__VA_ARGS__))))

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

// Find `needle` within `haystack`
static int64_t string_find(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (size_t i = 0; i <= haystack.length - needle.length; i++) {
        if (mem_eq(haystack.data + i, needle.data, needle.length)) {
            return i;
        }
    }
    return -1;
}

// Find `needle` within `haystack`, starting from the end of `haystack`
static int64_t string_find_rev(String haystack, String needle) {
    if (needle.length > haystack.length) return -1;
    if (needle.length == 0 && haystack.length == 0) return 0;

    for (int i = haystack.length - needle.length; i >= 0; i--) {
        if (mem_eq(haystack.data + i, needle.data, needle.length)) {
            return i;
        }
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

// Take `amount` characters from beginning of string
static String string_take(String str, size_t amount) {
    assertf(amount <= str.length, "string_take: index out of bounds");
    return (String){
        .data = str.data,
        .length = amount,
    };
}

// Get index of suffix or -1 if not found
static int string_find_suffix(String str, String suffix) {
    if (suffix.length > str.length) return -1;
    // memcmp forbids NULL pointers even if the size is 0
    if (!suffix.data) return 0;
    if (!str.data) return -1;

    int start = str.length - suffix.length;
    return (mem_eq(str.data + start, suffix.data, suffix.length))? start: -1;
}

static bool string_starts_with(String str, String prefix) {
    if (prefix.length > str.length) return false;
    // memcmp forbids NULL pointers even if the size is 0
    if (!prefix.data) return true;
    if (!str.data) return false;

    return mem_eq(str.data, prefix.data, prefix.length);
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
static bool _string_is_equal_chars(char ch, void *data) {
    String chars = *(String *)data;
    for (size_t i = 0; i < chars.length; i++) {
        if (ch == chars.data[i]) return true;
    }
    return false;
}

// Skips from start of string as long as one of the elements of `chars` are present
static String string_skip_chars(String str, String chars) {
    return string_skip_while(str, _string_is_equal_chars, &chars);
}

// Skips from end of string as long as one of the elements of `chars` are present
static String string_skip_chars_rev(String str, String chars) {
    return string_skip_while_rev(str, _string_is_equal_chars, &chars);
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
    char *lower = arena_push(arena, char, str.length);
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
    char *upper = arena_push(arena, char, str.length);
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
    char *reversed = arena_push(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        reversed[str.length - i - 1] = str.data[i];
    }
    return (String){reversed, str.length};
}


static String string_replace(Arena *arena, String str, String find, String replace_with) {
    if (find.length == 0) {
        return (String){
            .data = arena_strdup(arena, str.data, str.length),
            .length = str.length
        };
    }

    size_t max_length = replace_with.length * str.length;
    char *replaced = arena_push(arena, char, max_length);

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
    arena_pop_current(arena, char, max_length - actual_length);
    return (String){.data = replaced, .length = actual_length};
}

typedef struct {
    bool is_over;
    String string;
} SplitIterator;

static SplitIterator string_split_first(String *str, String split_at) {
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
// Eg: string_split_chars_first("a+-b", "-+") will return "a", then "", and finally "b"
static SplitIterator string_split_chars_first(String *str, String delims) {
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
            first_match = min(first_match, new_index);
            last_match = max(last_match, new_index);
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
// A bit cursed, so use with care
#define string_split_foreach(str, split_at, it)                                          \
    SplitIterator make_unique(si__) = {0};                                               \
    String make_unique(str0__) = (str);                                                  \
    String it;                                                                           \
    int make_unique(cnt__) = 0;                                                          \
    while ((make_unique(si__) = string_split_first(&make_unique(str0__), (split_at))),   \
           (it = make_unique(si__).string),                                              \
           (make_unique(cnt__) = make_unique(si__).is_over ? make_unique(cnt__) + 1: 0), \
           (!make_unique(si__).is_over || make_unique(cnt__) < 2))                       \


#define string_split_chars_foreach(str, delims, it)                                        \
    SplitIterator make_unique(si__) = {0};                                                 \
    String make_unique(str0__) = (str);                                                    \
    String it;                                                                             \
    int make_unique(cnt__) = 0;                                                            \
    while ((make_unique(si__) = string_split_chars_first(&make_unique(str0__), (delims))), \
           (it = make_unique(si__).string),                                                \
           (make_unique(cnt__) = make_unique(si__).is_over ? make_unique(cnt__) + 1: 0),   \
           (!make_unique(si__).is_over || make_unique(cnt__) < 2))                         \



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
