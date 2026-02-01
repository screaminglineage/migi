#ifndef MIGI_STRING_H
#define MIGI_STRING_H

#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "migi_core.h"
#include "arena.h"

#ifdef _WIN32
#else
#include <fcntl.h>
#include <unistd.h>
#endif // ifdef _WIN32

// TODO: add string styling (camelcase/snakecase/etc. conversions)

typedef struct {
    const char *data;
    size_t length;
} Str;

typedef struct {
    Str *data;
    size_t length;
} StrSlice;

#define S(cstr) (Str){(cstr), (sizeof(cstr) - 1)}
#define SArg(sv) (int)(sv).length, (sv).data


static Str str_from_span(char *start, char *end);
static Str str_from_cstr(const char *cstr);
static char *str_to_cstr(Arena *arena, Str str);

static Str str_copy(Arena *arena, Str str);

// Concatenates `tail` to the end of `head` if that was the
// last allocation, otherwise copies both `head` and `tail`
// Returns the concatenated string, so it can be chained
static Str str_cat(Arena *arena, Str head, Str tail);

typedef enum {
    Eq_IgnoreCase = (1 << 0),
} StrEqOpt;

static bool str_eq_ex(Str a, Str b, StrEqOpt flags);
#define str_eq(a, b) str_eq_ex((a), (b), 0)
static bool str_eq_cstr(Str a, const char *b, StrEqOpt flags);

// Check if string matches any element of an array
// NOTE: __VA_ARGS__ cannot be empty in str_eq_any for msvc compatibility
bool str_eq_any_slice(Str to_match, StrSlice matches);
#define str_eq_any(to_match, ...) \
    str_eq_any_slice((to_match), slice_from(Str, StrSlice, __VA_ARGS__))


// Slice string into [start, end) (exclusive range)
// Out of bounds accesses are clamped to the length of the string itself
// The above property holds true for the following 4 functions as well
static Str str_slice(Str str, size_t start, size_t end);

// Skip `amount` characters from beginning of string
// str_skip("abcde", 2) => "cde"
static Str str_skip(Str str, size_t amount);

// Take `amount` characters from beginning of string
// str_take("abcde", 2) => "ab"
static Str str_take(Str str, size_t amount);

// TODO: find where these two can be used

// Drop `amount` characters from end of string
// str_drop("abcde", 2) => "abc"
static Str str_drop(Str str, size_t amount);

// Lift `amount` characters from end of string
// str_lift("abcde", 2) => "de"
static Str str_lift(Str str, size_t amount);



typedef enum {
    Find_Reverse         = (1 << 0),
    Find_IgnoreCase      = (1 << 1), // TODO: implement this

    // Treat needle as a sequence of chars, and 
    // return index of the first match of any of them
    Find_AsChars         = (1 << 2),
} StrFindOpt;

// Find `needle` within `haystack`
// NOTE: Failure to find returns 1 past the last index searched
// For forwards it is `haystack.length`
// For reverse it is `-1`
static int64_t str_find_ex(Str haystack, Str needle, StrFindOpt flags);
#define str_find(haystack, needle) str_find_ex((haystack), (needle), 0)

// Returns index of `suffix` in `str`, -1 if not found
static int64_t str_find_suffix(Str str, Str suffix);

static bool str_starts_with(Str str, Str prefix);
static bool str_ends_with(Str str, Str suffix);

// Slice off the prefix from the string if it exists
static Str str_chop_prefix(Str str, Str prefix);

// Slice off the suffix from the string if it exists
static Str str_chop_suffix(Str str, Str suffix);


// Function type for str_skip_while_ functions
// Gets passed in a character of the string and an optional `void *data`
typedef bool (str_skip_while_func) (char ch, void *data);


typedef enum {
    SkipWhile_Reverse = (1 << 0),
} SkipWhileOpt;

// Skips characters from start (or end) of string as long as the passed in function returns true
// The `data` argument is passed into the `skip_char` function to emulate a closure
static Str str_skip_while(Str str, str_skip_while_func *func, void *data, SkipWhileOpt flags);

// Skips from start of string as long as one of the elements of `chars` are present
static Str str_skip_chars(Str str, Str chars, SkipWhileOpt flags);

static Str str_trim(Str str);
static Str str_trim_left(Str str);
static Str str_trim_right(Str str);

char char_to_upper(char ch);
char char_to_lower(char ch);

static Str str_to_lower(Arena *arena, Str str);
static Str str_to_upper(Arena *arena, Str str);
static Str str_reverse(Arena *arena, Str str);
static Str str_replace(Arena *arena, Str str, Str find, Str replace_with);


typedef struct {
    Str head;
    Str tail;
    bool found;
} StrCut;

typedef enum {
    // Start search from the end instead
    Cut_Reverse   = (1 << 0),

    // Splits up to the first occurrence of any of the characters of delimiter
    // Always returns the nearest match if multiple characters are found
    // Eg: str_split_chars_next("a+-b", "-+") will return "a", then "", and finally "b"
    Cut_AsChars   = (1 << 1)
} StrCutOpt;

// Cuts string into `head` and `tail` by splitting at the first occurence of `cut_at`
// If both `str` and `cut_at` are empty, then cut is still considered as `found`
// `head` contains `str` when no split is `found`
static StrCut str_cut_ex(Str str, Str cut_at, StrCutOpt flags);
#define str_cut(str, delim) str_cut_ex((str), (delim), 0)


// Loop through each split, (accessed by `cut.split`) of repeated `str_cut`s
// until there are no more matches
#define strcut_foreach(str, delim, flags, cut)                      \
    for (struct { StrCut _cut; int _count; Str split; }          \
        cut = {str_cut_ex((str), (delim), (flags)), 0, {0}};        \
        cut.split = cut._cut.head, cut._count < 1;                  \
        cut._cut.found                                              \
            ? cut._cut = str_cut_ex(cut._cut.tail, delim, flags), 0 \
            : cut._count++)


static inline uint64_t str_hash_fnv(Str string, uint64_t seed);
static inline uint64_t str_hash(Str string);

// Create formatted string on an arena
migi_printf_format(2, 3) static Str stringf(Arena *arena, const char *fmt, ...);

static Str str_from_file(Arena *arena, Str filepath);
static bool str_to_file(Str string, Str filepath);

static Str str_from_span(char *start, char *end) {
    return (Str){
        .data = start,
        .length = end - start
    };
}

static Str str_from_cstr(const char *cstr) {
    return (Str){
        .data = cstr,
        .length = (cstr == NULL)? 0: strlen(cstr)
    };
}

static char *str_to_cstr(Arena *arena, Str str) {
    char *cstr = arena_push_nonzero(arena, char, str.length + 1);
    if (str.data) {
        memcpy(cstr, str.data, str.length);
    }
    cstr[str.length] = 0;
    return cstr;
}


static Str str_copy(Arena *arena, Str str) {
    return (Str){
        .data = arena_copy(arena, char, str.data, str.length),
        .length = str.length
    };
}

static Str str_cat(Arena *arena, Str head, Str tail) {
    Arena *current = arena->current;
    if ((byte *)current + current->position != (byte *)head.data + head.length) {
        head = str_copy(arena, head);
    }
    head.length += str_copy(arena, tail).length;
    return head;
}

char char_to_upper(char ch) {
    if (between(ch, 'a', 'z')) ch -= ('a' - 'A');
    return ch;
}

char char_to_lower(char ch) {
    if (between(ch, 'A', 'Z')) ch += ('a' - 'A');
    return ch;
}

static bool str_eq_ex(Str a, Str b, StrEqOpt flags) {
    if (a.length != b.length) return false;

    // Prevents using memcmp with potentially NULL pointers
    if (a.length == 0) return true;

    if (!(flags & Eq_IgnoreCase)) return mem_eq_array(a.data, b.data, a.length);

    for (size_t i = 0; i < a.length; i++) {
        if (char_to_lower(a.data[i]) != char_to_lower(b.data[i])) return false;
    }
    return true;
}

static bool str_eq_cstr(Str a, const char *b, StrEqOpt flags) {
    return str_eq_ex(a, str_from_cstr(b), flags);
}

bool str_eq_any_slice(Str to_match, StrSlice matches) {
    for (size_t i = 0; i < matches.length; i++) {
        if (str_eq(to_match, matches.data[i])) {
            return true;
        }
    }
    return false;
}


static int64_t str_find_ex(Str haystack, Str needle, StrFindOpt flags) {
    if (needle.length == 0 && haystack.length == 0) return 0;

    if (flags & Find_AsChars) {
        int64_t first_match = INT64_MAX;
        int64_t last_match = INT64_MIN;
        for (size_t i = 0; i < needle.length; i++) {
            Str ch = (Str){.data = &needle.data[i], .length = 1};
            int64_t index = str_find_ex(haystack, ch, flags & ~Find_AsChars);
            last_match = max_of(last_match, index);
            first_match = min_of(first_match, index);
        }
        return (flags & Find_Reverse)? last_match: first_match;
    } 

    if (flags & Find_Reverse) {
        int64_t i = haystack.length - needle.length;
        for (; i >= 0; i--) {
            if (mem_eq_array(haystack.data + i, needle.data, needle.length)) {
                return i;
            }
        }
        return i;
    } else {
        for (int64_t i = 0; i <= (int64_t)haystack.length - (int64_t)needle.length; i++) {
            if (mem_eq_array(haystack.data + i, needle.data, needle.length)) {
                return i;
            }
        }
        return haystack.length;
    }
}


static Str str_slice(Str str, size_t start, size_t end) {
    start = clamp_top(start, str.length);
    end = clamp_top(end, str.length);
    return (Str){
        .data = str.data + start,
        .length = end - start
    };
}

static Str str_skip(Str str, size_t amount) {
    return str_slice(str, amount, str.length);
}

static Str str_take(Str str, size_t amount) {
    return str_slice(str, 0, amount);
}

static Str str_drop(Str str, size_t amount) {
    return str_slice(str, 0, str.length - amount);
}

static Str str_lift(Str str, size_t amount) {
    return str_slice(str, str.length - amount, str.length);
}

static int64_t str_find_suffix(Str str, Str suffix) {
    if (suffix.length > str.length) return -1;

    size_t start = str.length - suffix.length;
    bool match = str_eq(str_slice(str, start, str.length), suffix);
    if (match) {
        return start;
    } else {
        return -1;
    }
}

static bool str_starts_with(Str str, Str prefix) {
    if (prefix.length > str.length) return false;
    return str_eq(str_slice(str, 0, prefix.length), prefix);
}

static bool str_ends_with(Str str, Str suffix) {
    return str_find_suffix(str, suffix) != -1;
}

static Str str_chop_prefix(Str str, Str prefix) {
    if (!str_starts_with(str, prefix)) {
        return str;
    }
    return str_skip(str, prefix.length);
}

static Str str_chop_suffix(Str str, Str suffix) {
    return str_take(str, str_find_suffix(str, suffix));
}

static Str str_skip_while(Str str, str_skip_while_func *skip_char, void *data, SkipWhileOpt flags) {
    while (str.length > 0) {
        size_t skip_index = (flags & SkipWhile_Reverse)? str.length - 1: 0;
        if (!skip_char(str.data[skip_index], data)) break;

        // Only skip forward if not in reverse mode
        str.data += !(flags & SkipWhile_Reverse);
        str.length--;
    }
    return str;
}

// Special case for `str_skip_while`
// Probably the most common use for skipping in a string
static bool str__is_equal_chars(char ch, void *data) {
    Str chars = *(Str *)data;
    for (size_t i = 0; i < chars.length; i++) {
        if (ch == chars.data[i]) return true;
    }
    return false;
}

static Str str_skip_chars(Str str, Str chars, SkipWhileOpt flags) {
    return str_skip_while(str, str__is_equal_chars, &chars, flags);
}

// TODO: check for other whitespace characters
// https://stackoverflow.com/a/46637343
static Str str_trim_left(Str str) {
    return str_skip_chars(str, S(" \n\r\t"), 0);
}

static Str str_trim_right(Str str) {
    return str_skip_chars(str, S(" \n\r\t"), SkipWhile_Reverse);
}

static Str str_trim(Str str) {
    return str_trim_left(str_trim_right(str));
}

static Str str_to_lower(Arena *arena, Str str) {
    char *lower = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        if (str.data[i] >= 'A' && str.data[i] <= 'Z') {
            lower[i] = str.data[i] + 32;
        } else {
            lower[i] = str.data[i];
        }
    }
    return (Str){lower, str.length};
}

static Str str_to_upper(Arena *arena, Str str) {
    char *upper = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        if (str.data[i] >= 'a' && str.data[i] <= 'z') {
            upper[i] = str.data[i] - 32;
        } else {
            upper[i] = str.data[i];
        }
    }
    return (Str){upper, str.length};
}

static Str str_reverse(Arena *arena, Str str) {
    char *reversed = arena_push_nonzero(arena, char, str.length);
    for (size_t i = 0; i < str.length; i++) {
        reversed[str.length - i - 1] = str.data[i];
    }
    return (Str){reversed, str.length};
}


static Str str_replace(Arena *arena, Str str, Str find, Str replace_with) {
    size_t max_length = replace_with.length * (str.length + 2);
    char *replaced = arena_push_nonzero(arena, char, max_length);
    char *replaced_at = replaced;

    if (find.length == 0) {
        array_foreach(&str, const char, ch) {
            memcpy(replaced_at, replace_with.data, replace_with.length);
            replaced_at += replace_with.length;
            memcpy(replaced_at, ch, 1);
            replaced_at += 1;
        }
        memcpy(replaced_at, replace_with.data, replace_with.length);
        replaced_at += replace_with.length;
    } else {
        while (true) {
            size_t index = str_find(str, find);
            if (index == str.length) {
                memcpy(replaced_at, str.data, str.length);
                replaced_at += str.length;
                break;
            }

            memcpy(replaced_at, str.data, index);
            replaced_at += index;
            memcpy(replaced_at, replace_with.data, replace_with.length);
            replaced_at += replace_with.length;
            str = str_skip(str, index + find.length);
        }
    }


    size_t actual_length = replaced_at - replaced;
    arena_pop(arena, char, max_length - actual_length);
    return (Str){.data = replaced, .length = actual_length};
}


static StrCut str_cut_ex(Str str, Str cut_at, StrCutOpt flags) {
    StrCut cut = {0};
    StrFindOpt find_flags = (flags & Cut_Reverse)? Find_Reverse: 0;

    int64_t cut_index = 0;
    int64_t cut_length = 0;

    if (flags & Cut_AsChars) {
        cut_index = str_find_ex(str, cut_at, find_flags|Find_AsChars);
        cut_length = 1;
    } else {
        cut_index = str_find_ex(str, cut_at, find_flags);
        cut_length = cut_at.length;
    }

    if (flags & Cut_Reverse) {
        cut.head = str_skip(str, cut_index + cut_length);
        cut.tail = str_take(str, cut_index);
        cut.found = (str.length == 0 && cut_length == 0) || cut_index > -1;
    } else {
        cut.head = str_take(str, cut_index);
        cut.tail = str_skip(str, cut_index + cut_length);
        cut.found = (str.length == 0 && cut_length == 0) || (size_t)cut_index < str.length;
    }

    return cut;
}


static inline uint64_t str_hash_fnv(Str string, uint64_t seed) {
    uint64_t h = seed? seed: 0x100;
    for (size_t i = 0; i < string.length; i++) {
        h ^= string.data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

static inline uint64_t str_hash(Str string) {
    return str_hash_fnv(string, 0);
}

static Str str__format(Arena *arena, const char *fmt, va_list args) {
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
    return (Str){ .data = mem, .length = actual - 1 };
}

migi_printf_format(2, 3) static Str stringf(Arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str result = str__format(arena, fmt, args);
    va_end(args);
    return result;
}

#ifdef _WIN32
    typedef HANDLE File;
    #define FILE_ERROR INVALID_HANDLE_VALUE
#else
    typedef int File;
    #define FILE_ERROR -1
#endif

typedef struct {
    Str string;
    bool ok;
} StrResult;

static StrResult read_entire_file(Arena *arena, File file) {
    StrResult result = {0};
#ifdef _WIN32
    DWORD size_high = 0;
    DWORD size_low = GetFileSize(file, &size_high);
    LARGE_INTEGER filesize = {
        .LowPart = size_low,
        .HighPart = size_high
    };
    // file position cannot be negative at this point
    size_t length = filesize.QuadPart;
    char *buf = arena_push(arena, char, length);

    char *buf_start = buf;
    char *buf_end = buf_start + length;
    while (buf < buf_end) {
        DWORD n = 0;
        if (!ReadFile(file, buf, (DWORD)length, &n, NULL)) {
            arena_pop(arena, char, length);
            return result;
        }
        buf += n;
    }

    result.string = (Str){
        .data = buf_start,
        .length = length,
    };
    result.ok = true;
    return result;
}
#else
    off_t length = lseek(fd, 0, SEEK_END);
    if (length == -1) {
        return (Str){0};
    }
    lseek(fd, 0, SEEK_SET);

    // file position cannot be negative at this point
    char *buf = arena_push(arena, char, length);

    ssize_t n = 0;
    char *buf_at = buf;
    while (n < length) {
        ssize_t m = read(fd, buf_at, length);
        if (m == -1) {
            arena_pop(arena, char, length);
            return (Str){0};
        }
        n += m;
        buf_at += m;
    }

    return (Str){
        .data = buf,
        .length = length,
    };
}
#endif // #ifndef _WIN32

static bool write_entire_str(File file, Str str) {
#ifdef _WIN32
    while (str.length > 0) {
        DWORD n = 0;
        if (!WriteFile(file, str.data, (DWORD)str.length, &n, NULL)) {
            return false;
        }
        str = str_skip(str, n);
    }
    return true;
#else
    while (str.length > 0) {
        ssize_t n = write(fd, str.data, str.length);
        if (n == -1) {
            return false;
        }
        str = str_skip(str, n);
    }
    return true;
#endif // #ifndef _WIN32
}

// TODO: also add file_open

static bool file_close(File file) {
#ifdef _WIN32
    return CloseHandle(file);
#else
    // TODO: can close return an error?
    close(fd);
    return true;
#endif // ifdef _WIN32
}


static Str last_error_string(Arena *arena) {
#ifdef _WIN32
    DWORD err = GetLastError();

    DWORD max_length = 64*KB - 1;
    char *buf = arena_push_nonzero(arena, char, max_length);
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, max_length, NULL);

    arena_pop(arena, char, max_length - len - 2);

    Str err_string = (Str){
        .data = buf,
        .length = len
    };

    // remove `\r\n` from end
    if (str_ends_with(err_string, S("\r\n"))) {
        err_string = str_drop(err_string, 2);
    }
    return err_string;
#else
    const char *s = strerror(errno);
    return string_from_cstr(s);
#endif
}


// TODO: passing in a directory as filepath causes ftell to return LONG_MAX which overflows the arena
static Str str_from_file(Arena *arena, Str filepath) {
    Str str = {0};
    Temp tmp = arena_temp_excl(arena);

    File file;
#ifdef _WIN32
    file = CreateFileA(str_to_cstr(tmp.arena, filepath), 
            GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
    file = open(str_to_cstr(tmp.arena, filepath), O_RDONLY);
#endif // #ifdef _WIN32

    if (file == FILE_ERROR) {
        migi_log(Log_Error, "failed to open file `%.*s`: %.*s",
                SArg(filepath), SArg(last_error_string(tmp.arena)));
        arena_temp_release(tmp);
        return str;
    }

    StrResult result = read_entire_file(arena, file);
    if (!result.ok) {
        migi_log(Log_Error, "failed to read from file `%.*s`: %.*s",
                SArg(filepath), SArg(last_error_string(tmp.arena)));
    }
    str = result.string;

    file_close(file);
    arena_temp_release(tmp);
    return str;
}


static bool str_to_file(Str string, Str filepath) {
    Temp tmp = arena_temp();
    File file;
#ifdef _WIN32
    file = CreateFileA(str_to_cstr(tmp.arena, filepath),
            GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
#else
    file = open(str_to_cstr(tmp.arena, filepath),
                  O_WRONLY|O_CREAT|O_TRUNC,
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif // _WIN32

    if (file == FILE_ERROR) {
        migi_log(Log_Error, "failed to open file `%.*s`: %.*s",
                SArg(filepath), SArg(last_error_string(tmp.arena)));
        arena_temp_release(tmp);
        return false;
    }

    bool ok = write_entire_str(file, string);
    if (!ok) {
        migi_log(Log_Error, "failed to write to file `%.*s`: %.*s", 
                SArg(filepath), SArg(last_error_string(tmp.arena)));
    }

    file_close(file);
    arena_temp_release(tmp);
    return ok;
}

#endif // MIGI_STRING_H
