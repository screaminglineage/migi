#include "migi.h"

typedef struct {
    uint32_t *data;
    int64_t length;
} Str32;

typedef struct {
    uint16_t *data;
    int64_t length;
} Str16;

Str str_from_str16(Arena *arena, Str16 utf16);
Str str_from_str32(Arena *arena, Str32 utf32);

Str16 str16_from(uint16_t *data, int64_t length);
Str16 str16_from_str(Arena *arena, Str utf8);
Str16 str16_from_str32(Arena *arena, Str32 utf32);

Str32 str32_from(uint32_t *data, int64_t length);
Str32 str32_from_str(Arena *arena, Str utf8);
Str32 str32_from_str16(Arena *arena, Str16 utf16);

// UTF-8 string functions
int64_t utf8_length(Str utf8);

typedef enum {
    Utf8Find_Reverse = bit(0),      // Return the last match

    // TODO: implement this
    Utf8Find_Any     = bit(1),      // Search for each codepoint in needle separately
} Utf8FindOpt;

int64_t utf8_find_opt(Str haystack, Str needle, Utf8FindOpt flags);
#define utf8_find(haystack, needle) utf8_find_opt((haystack), (needle), 0)

// Iterate over UTF-8 codepoints
#define utf8_foreach(utf8, it)            \
    for (Utf8Iter it = utf8_next((utf8)); \
         !it.end && !it.error;            \
         it = utf8_next(it.rest))



// Taken from: https://bjoern.hoehrmann.de/utf-8/decoder/dfa/
//
// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
static const uint8_t utf8d[] = {
  // The first part of the table maps bytes to character classes that
  // to reduce the size of the transition table and create bitmasks.
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

  // The second part is a transition table that maps a combination
  // of a state of the automaton and a character class to a state.
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12,
};

// States of the DFA
#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static uint32_t utf8__decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}



typedef struct {
    Str rest;
    uint32_t codepoint;
    bool error;
    bool end;
} Utf8Iter;

Utf8Iter utf8_next(Str utf8) {
    if (utf8.length == 0) return (Utf8Iter){.end=true};
    Utf8Iter result = {0};

    uint32_t state = UTF8_ACCEPT;
    int i = 0;
    do {
        assert(i < (int)utf8.length);
        utf8__decode(&state , &result.codepoint, (uint8_t)utf8.data[i++]);
    } while (state > UTF8_REJECT); // loops until either ACCEPT/REJECT is reached

    if (state == UTF8_REJECT) {
        result.rest  = utf8;
        result.error = true;
        return result;
    }

    int consumed;
    if (result.codepoint < 0x7F) {
        consumed = 1;
    } else if (result.codepoint < 0x7FF) {
        consumed = 2;
    } else if (result.codepoint < 0xFFFF) {
        consumed = 3;
    } else if (result.codepoint < 0x10FFFF) {
        consumed = 4;
    }
    result.rest = str_skip(utf8, consumed);
    return result;
}


Str16 str16_from(uint16_t *data, int64_t length) {
    return (Str16){
        .data   = data,
        .length = length
    };
}

Str32 str32_from(uint32_t *data, int64_t length) {
    return (Str32){
        .data   = data,
        .length = length
    };
}


Str32 str32_from_str(Arena *arena, Str utf8) {
    size_t push_size = utf8.length*4;
    uint32_t *mem = arena_push(arena, uint32_t, push_size);
    int64_t length = 0;

    uint32_t *utf32 = mem;
    uint32_t state = UTF8_ACCEPT;
    for (size_t i = 0; i < utf8.length; i++) {
        if (utf8__decode(&state, utf32, (uint8_t)utf8.data[i]) == UTF8_ACCEPT) {
            utf32++;
        }
    }

    if (state == UTF8_REJECT) {
        arena_pop(arena, uint32_t, push_size);
        return (Str32){0};
    } else {
        arena_pop(arena, uint32_t, push_size - length);
        return str32_from(mem, utf32 - mem);
    }
}

Str32 str32_from_str16(Arena *arena, Str16 utf16) {
    unused(arena);
    unused(utf16);
    todo();
}

Str16 str16_from_str(Arena *arena, Str utf8) {
    Str32 utf32 = str32_from_str(arena, utf8);
    return str16_from_str32(arena, utf32);
}

Str16 str16_from_str32(Arena *arena, Str32 utf32) {
    unused(arena);
    unused(utf32);
    todo();
}

Str str_from_str16(Arena *arena, Str16 utf16) {
    Str32 utf32 = str32_from_str16(arena, utf16);
    return str_from_str32(arena, utf32);
}

Str str_from_str32(Arena *arena, Str32 utf32) {
    unused(arena);
    unused(utf32);
    todo();
}


int64_t utf8_length(Str utf8) {
    int64_t count = 0;
    Utf8Iter it = utf8_next(utf8);
    while (!it.end && !it.error) {
        it = utf8_next(it.rest);
        count++;
    }

    if (it.error) count = -1;
    return count;
}

int64_t utf8_find_opt(Str haystack, Str needle, Utf8FindOpt flags) {
    if (flags & Find_Reverse) {
        int64_t i = haystack.length - 1;

        int64_t count = 0;
        while (i >= 0) {
            while ((haystack.data[i] & 0xC0) == 0x80) {
                i--;
            }
            if (str_starts_with(str_skip(haystack, i), needle)) {
                // TODO: is there a better way to do this?
                return utf8_length(haystack) - count - 1;
            }
            count++;
            i--;
        }
        return -1;
    } else {
        if (str_starts_with(haystack, needle)) return 0;
        int64_t count = 0;
        Utf8Iter it = utf8_next(haystack);
        while (!it.end && !it.error) {
            count++;
            if (str_starts_with(it.rest, needle)) return count;

            it = utf8_next(it.rest);
        }
        return count;
    }
    migi_unreachable();
}

int main() {
    Arena *a = arena_init();
    Str str = S("ÆaあÆ𒀖");
    // utf8_foreach(str, it) {
    //     printf("U+%04X\n", it.codepoint);
    // }
    printf("\nlength : %ld\n", utf8_length(str));
    printf("find(Æ): %ld\n", utf8_find(str, S("Æ")));
    printf("find(Æ): %ld\n\n", utf8_find_opt(str, S("Æ"), Utf8Find_Reverse));

    Str32 str32 = str32_from_str(a, str);
    for (int i = 0; i < str32.length; i++) {
        printf("U+%04X\n", str32.data[i]);
    }
    return 0;
}
