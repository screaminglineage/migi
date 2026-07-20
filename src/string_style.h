#ifndef STR_STYLE_H
#define STR_STYLE_H

#include "migi_string.h"
#include "migi_list.h"

// Convert text to different styles
static Str str_style_snake(Arena *arena, Str str);          // foo_bar_baz
static Str str_style_snake_caps(Arena *arena, Str str);     // Foo_Bar_Baz
static Str str_style_snake_upper(Arena *arena, Str str);    // FOO_BAR_BAZ
static Str str_style_camel(Arena *arena, Str str);          // fooBarBaz
static Str str_style_pascal(Arena *arena, Str str);         // FooBarBaz

// TODO: make sure that the constant `capitalize` is optimized away
// since the functions are always called with either true or false
static Str str_style__snake_like(Arena *arena, Str str, bool capitalize) {
    Str result = {0};
    if (str.length == 0) return result;

    Temp tmp = arena_temp_excl(arena);

    // TODO: first try to split by _ instead of always assuming that it is in camelCase/PascalCase
    // See scratch/test_string_style.c for the examples that dont work
    StrList words = {0};
    char first = capitalize? char_to_upper(str.data[0]): char_to_lower(str.data[0]);
    strlist_push_char(tmp.arena, &words, first);

    size_t start = 0;
    for (size_t i = start + 1; i < str.length; i++) {
        if (!(str.data[i] == '_' || char_is_upper(str.data[i]))) continue;

        // For snake case only consider an uppercase letter that follows an underscore
        // For capital snake case, consider any non-underscore character that follows an underscore
        bool dont_skip = capitalize
            ? i + 1 < str.length && str.data[i + 1] != '_'
            : i + 1 < str.length && char_is_upper(str.data[i + 1]);

        bool is_underscore = str.data[i] == '_';
        if (is_underscore && !dont_skip) continue;

        Str part = str_slice(str, start + 1, i);

        // Handling the other variant of snake case by skipping the underscore
        // For capitalized snake case, regular snake case is handled
        // For regular snake case, capitalized snake case is handled
        if (is_underscore) {
            char ch = capitalize? char_to_upper(str.data[i + 1]): char_to_lower(str.data[i + 1]);
            strlist_pushf(tmp.arena, &words, "%.*s_%c", SArg(part), ch);
            start = i + 1;
            i++;
        } else {
            char ch = capitalize? char_to_upper(str.data[i]): char_to_lower(str.data[i]);
            strlist_pushf(tmp.arena, &words, "%.*s_%c", SArg(part), ch);
            start = i;
        }
    }
    strlist_push(tmp.arena, &words, str_skip(str, start + 1));

    arena_temp_release(tmp);

    return strlist_to_str(arena, &words);
}


static Str str_style__camel_like(Arena *arena, Str str, bool first_caps) {
    if (str.length == 0) return str;

    Temp tmp = arena_temp_excl(arena);

    StrList sb = {0};
    int64_t first_word_end = str_find(str, S("_"));
    int64_t str_len = str.length;
    Str first_word = str_advance(&str, first_word_end + 1);

    bool found = first_word_end < str_len;
    first_word = str_drop(first_word, found);

    char first = first_caps
        ? char_to_upper(first_word.data[0])
        : char_to_lower(first_word.data[0]);

    Str rest;
    if (found) {
        // Is in snake case, so the word boundaries are known, which means
        // that the entire rest of the word can be converted to lower case
        rest = str_to_lower(tmp.arena, str_skip(first_word, 1));
    } else {
        // Is not in snake case, so the word boundaries are unknown
        // In this case the word is kept as is
        rest = str_skip(first_word, 1);
    }
    strlist_pushf(tmp.arena, &sb, "%c%.*s", first, SArg(rest));

    // TODO: this currently skips trailing underscores
    // Is it worth it to use a custom parser instead of strcut_foreach just
    // to support that edge case?
    strcut_foreach(str, S("_"), cut) {
        if (cut.split.length == 0) continue;
        Str rest = str_to_lower(tmp.arena, str_skip(cut.split, 1));
        strlist_pushf(tmp.arena, &sb, "%c%.*s", char_to_upper(cut.split.data[0]), SArg(rest));
    }

    arena_temp_release(tmp);
    return strlist_to_str(arena, &sb);
}


// foo_bar_baz
static Str str_style_snake(Arena *arena, Str str) {
    return str_style__snake_like(arena, str, false);
}

// Foo_Bar_Baz
static Str str_style_snake_caps(Arena *arena, Str str) {
    return str_style__snake_like(arena, str, true);
}

// FOO_BAR_BAZ
static Str str_style_snake_upper(Arena *arena, Str str) {
    Str result = str_style__snake_like(arena, str, true);
    return str_to_upper_inplace(&result);
}

// fooBarBaz
static Str str_style_camel(Arena *arena, Str str) {
    return str_style__camel_like(arena, str, false);
}

// FooBarBaz
static Str str_style_pascal(Arena *arena, Str str) {
    return str_style__camel_like(arena, str, true);
}

#endif // ifndef STR_STYLE_H
