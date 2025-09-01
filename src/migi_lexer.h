#ifndef MIGI_LEXER_H
#define MIGI_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include "migi.h"
#include "migi_string.h"

#define PROFILER_H_IMPLEMENTATION
#include "profiler.h"

// TODO: add line and column count tracking
// TODO: buffer errors as well rather than returning, otherwise it feels
// weird when the error occurs even though the token has not yet been seen
// TODO: handle escape characters within strings
// TODO: check for \r\n newlines while parsing as well (only for skipping comments for now)

typedef enum {
    Tok_Invalid = 0,
    Tok_Eof,
    Tok_OpenParen,
    Tok_CloseParen,
    Tok_OpenBrace,
    Tok_CloseBrace,
    Tok_OpenBracket,
    Tok_CloseBracket,
    Tok_Plus,
    Tok_Minus,
    Tok_Star,
    Tok_Slash,
    Tok_Lesser,
    Tok_Greater,
    Tok_Equals,
    Tok_MinusMinus,
    Tok_MinusEquals,
    Tok_PlusPlus,
    Tok_PlusEquals,
    Tok_Question,
    Tok_Colon,
    Tok_Semicolon,
    Tok_Comma,
    Tok_Backslash,
    Tok_Dot,
    Tok_String,
    Tok_Char,
    Tok_Floating,
    Tok_Integer,
    Tok_Identifier,
    Tok_Hash,
    Tok_Bang,
    Tok_And,
    Tok_Or,
    Tok_BitAnd,
    Tok_BitOr,
    Tok_Modulo,

    Tok_Count
} TokenType;

static String TOKEN_STRINGS[] = {
    [Tok_Invalid]             = SV("invalid token"),
    [Tok_Eof]                 = SV("end of file"),
    [Tok_OpenParen]           = SV("("),
    [Tok_CloseParen]          = SV(")"),
    [Tok_OpenBrace]           = SV("{"),
    [Tok_CloseBrace]          = SV("}"),
    [Tok_OpenBracket]         = SV("["),
    [Tok_CloseBracket]        = SV("]"),
    [Tok_Plus]                = SV("+"),
    [Tok_Minus]               = SV("-"),
    [Tok_Star]                = SV("*"),
    [Tok_Slash]               = SV("/"),
    [Tok_Lesser]              = SV("<"),
    [Tok_Greater]             = SV(">"),
    [Tok_Equals]              = SV("="),
    [Tok_MinusMinus]          = SV("--"),
    [Tok_MinusEquals]         = SV("-="),
    [Tok_PlusPlus]            = SV("++"),
    [Tok_PlusEquals]          = SV("+="),
    [Tok_Question]            = SV("?"),
    [Tok_Colon]               = SV(":"),
    [Tok_Semicolon]           = SV(";"),
    [Tok_Comma]               = SV("),"),
    [Tok_Backslash]           = SV("\\"),
    [Tok_Dot]                 = SV("."),
    [Tok_String]              = SV("string literal"),
    [Tok_Char]                = SV("character literal"),
    [Tok_Floating]            = SV("floating point literal"),
    [Tok_Integer]             = SV("integer literal"),
    [Tok_Identifier]          = SV("identifier"),
    [Tok_Hash]                = SV("#"),
    [Tok_Bang]                = SV("!"),
    [Tok_And]                 = SV("&&"),
    [Tok_Or]                  = SV("||"),
    [Tok_BitAnd]              = SV("&"),
    [Tok_BitOr]               = SV("|"),
    [Tok_Modulo]              = SV("%"),
};

static_assert(array_len(TOKEN_STRINGS) == Tok_Count, "Token Strings is not the same size as the number of tokens");

typedef struct {
    TokenType type;
    String string;
    union {
        double floating;
        uint64_t integer;
    };
} Token;

// TODO: add filename in Lexer
typedef struct {
    String string;
    size_t start;
    size_t end;
    Token token_buf[2];
} Lexer;

// TODO: prefix functions with `lexer_` or something similar

// Consume the next token
static inline bool consume_token(Lexer *lexer, Token *tok);

// Consume the next token, returning a zeroed token if it fails
static inline Token next_token(Lexer *lexer);

// Get the next token without consuming it
static inline bool peek_token(Lexer *lexer, Token *tok);



// TODO: make the `expect_token_*` function only consume the next
// token if it matches This will allow reporting errors by calling 
// next_token(), and printing what you actually got instead.
// Will need changing a bunch of stuff in tools/struct_printer.c though
//
// Check if the next token is the same as expected and consume it
// The next token is always consumed even if it doesn't match
static inline bool expect_token(Lexer *lexer, TokenType expected);
static inline bool expect_token_str(Lexer *lexer, TokenType expected, String str);

// Check if the next token is the same as expected
static inline bool match_token(Lexer *lexer, TokenType expected);
static inline bool match_token_str(Lexer *lexer, TokenType expected, String token_str);

// Check if the next token is one of the passed in tokens
// bool match_token_any(Lexer *lexer, (TokenType[]){ ... })
#define match_token_any(lexer, ...) \
    (_match_token_any((lexer), __VA_ARGS__, sizeof((__VA_ARGS__))/sizeof(*(__VA_ARGS__))))

#define lexer_new_token(lexer, tok_type)              \
    ((Token){                                         \
     .type   = (tok_type),                            \
     .string = (String){                              \
         .data = &lexer->string.data[(lexer)->start], \
         .length = lexer->end - (lexer)->start        \
     }, {0}})


// #define LEXER_GET(lexer) (lexer)->string.data[(lexer)->end]

static inline char lexer_peek_char(Lexer *lexer) {
    if (lexer->end >= lexer->string.length) {
        return 0;
    }
    return lexer->string.data[lexer->end];
}

static inline char lexer_peek_next_char(Lexer *lexer) {
    if (lexer->end + 1 >= lexer->string.length) {
        return 0;
    }
    return lexer->string.data[lexer->end + 1];
}

static inline char lexer_consume_char(Lexer *lexer) {
    if (lexer->end >= lexer->string.length) {
        return 0;
    }
    return lexer->string.data[lexer->end++];
}

char escape_char(char ch) {
    switch (ch) {
        case 'n':   return '\n';
        case 'r':   return '\r';
        case 't':   return '\t';
        case 'v':   return '\v';
        case 'f':   return '\f';
        case 'b':   return '\b';
        case '0':   return '\0';
        case '\\':  return '\\';
        case '\'':  return '\'';
        default: todof("unknown escape character, `%c`", ch);
    }
}

// TODO: handle `\x` and other multi-character escape sequences
static bool tokenize_char(Lexer *lexer) {
    Token tok = {.type = Tok_Char};
    char ch = lexer_consume_char(lexer);

    if (ch == '\0') {
        fprintf(stderr, "error: unmatched '\'' at index: %zu\n", lexer->start);
        return false;
    }

    if (ch == '\\') {
        tok.integer = escape_char(lexer_peek_char(lexer));
        lexer->end++;
    } else {
        tok.integer = ch;
    } 

    if (lexer_peek_char(lexer) != '\'') {
        fprintf(stderr, "error: character literal with multiple characters at index: %zu\n", lexer->start);
        return false;
    }

    tok.string = string_slice(lexer->string, lexer->start, lexer->end);
    lexer->token_buf[1] = tok;
    return true;
}

static bool tokenize_string(Lexer *lexer) {
    TIME_FUNCTION;

    // TODO: parse escape sequences
    char ch = '\0';
    while (true) {
        ch = lexer_peek_char(lexer);
        if (ch == '"' || ch == '\0') break;
        lexer->end++;
    }
    if (ch == '\0') {
        fprintf(stderr, "error: unmatched '\"' at index: %zu\n", lexer->start);
        return false;
    }

    lexer->token_buf[1] = (Token){
        .type = Tok_String,
        .string = (String){
            .data = &lexer->string.data[lexer->start],
            .length = lexer->end - lexer->start
        }
    };
    return true;
}

static bool tokenize_number(Lexer *lexer) {
    TIME_FUNCTION;

    char ch = '\0';
    while (true) {
        ch = lexer_peek_char(lexer);
        if (ch == '\0' || !between(ch, '0', '9')) {
            break;
        }
        lexer->end++;
    }

    if (ch != '\0') {
        // parsing as floating point
        if (ch == 'e' || ch == 'E' || ch == '.') {
            while (true) {
                char ch = lexer_peek_char(lexer);
                if (!(ch == 'e' || ch == 'E' || ch == '.' || between(ch, '0', '9'))) {
                    break;
                }
                lexer->end++;
            }

            String number_str = string_slice(lexer->string, lexer->start, lexer->end);
            // TODO: implement strtod rather than depending on it
            // NOTE: Since the next character after the end of the floating point literal
            // is guaranteed to be something not part of the literal (e, E, ., number),
            // using strtod with a non null-terminated string should be fine here
            char *end_ptr;
            double number = strtod(number_str.data, &end_ptr);

            if (end_ptr != number_str.data + number_str.length) {
                fprintf(stderr, "error: invalid floating point constant, `%.*s`, at: %zu\n",
                        SV_FMT(number_str), lexer->start);
                return false;
            }

            lexer->token_buf[1] = (Token){
                .type = Tok_Floating,
                .string = number_str,
                .floating = number
            };
            return true;
        }
    }

    // parsing as integer
    String num = string_slice(lexer->string, lexer->start, lexer->end);
    lexer->token_buf[1] = (Token){
        .type = Tok_Integer,
        .string = num,
        .integer = 0
    };
    for (size_t i = 0; i < num.length; i++) {
        int digit = num.data[i] - '0';
        lexer->token_buf[1].integer *= 10;
        lexer->token_buf[1].integer += digit;
    }
    return true;
}

static bool tokenize_identifier(Lexer *lexer) {
    TIME_FUNCTION;

    size_t identifier_start = lexer->start;
    while (true) {
        char ch = lexer_peek_char(lexer);
        if (!(between(ch, 'a', 'z')
            || between(ch, 'A', 'Z')
            || between(ch, '0', '9')
            || ch == '_'))
                break;
        lexer->end++;
    }

    lexer->token_buf[1] = (Token){
        .type = Tok_Identifier,
        .string = string_slice(lexer->string, identifier_start, lexer->end),
    };
    return true;
}

// BUG: will go into an infinite loop when it encounters a `/`
static void lexer_skip_whitespace(Lexer *lexer) {
    while (true) {
        if (isspace(lexer_peek_char(lexer))) {
            while (isspace(lexer_peek_char(lexer)))  lexer->end++;
        } else if (lexer_peek_char(lexer) == '/') {
            if (lexer_peek_next_char(lexer) == '/') {
                while (lexer_peek_char(lexer) != '\n') lexer->end++;
            } else if (lexer_peek_next_char(lexer) == '*') {
                while (!(lexer_peek_char(lexer) == '*' && lexer_peek_next_char(lexer) == '/')) {
                    lexer->end++;
                }
                lexer->end++;
                lexer->end++;
            }
        } else {
            break;
        }
    }
}

static bool next_token_impl(Lexer *lexer, Token *tok)  {
    TIME_FUNCTION;

    lexer->token_buf[0] = lexer->token_buf[1];
    lexer_skip_whitespace(lexer);
    lexer->start = lexer->end;

    char ch = lexer_consume_char(lexer);
    switch(ch) {
        case '\0': lexer->token_buf[1] = lexer_new_token(lexer, Tok_Eof);          break;
        case '(':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_OpenParen);    break;
        case ')':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_CloseParen);   break;
        case '{':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_OpenBrace);    break;
        case '}':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_CloseBrace);   break;
        case '[':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_OpenBracket);  break;
        case ']':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_CloseBracket); break;
        case ',':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Comma);        break;
        case '\\': lexer->token_buf[1] = lexer_new_token(lexer, Tok_Backslash);    break;
        case '?':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Question);     break;
        case ':':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Colon);        break;
        case ';':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Semicolon);    break;
        case '#':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Hash);         break;
        case '-':  {
            if (lexer_peek_char(lexer) == '-') {
               lexer->end++;
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_MinusMinus);
            } else if (lexer_peek_char(lexer) == '=') {
                lexer->end++;
                lexer->token_buf[1] = lexer_new_token(lexer, Tok_MinusEquals);
            } else {
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_Minus);
            }
        } break;
        case '+':  {
            if (lexer_peek_char(lexer) == '+') {
                lexer->end++;
                lexer->token_buf[1] = lexer_new_token(lexer, Tok_PlusPlus);
            } else if (lexer_peek_char(lexer) == '=') {
                lexer->end++;
                lexer->token_buf[1] = lexer_new_token(lexer, Tok_PlusEquals);
            } else {
                lexer->token_buf[1] = lexer_new_token(lexer, Tok_Plus);
            }
        } break;
        case '*':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Star);    break;
        case '/':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Slash);   break;
        case '<':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Lesser);  break;
        case '>':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Greater); break;
        case '=':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Equals);  break;
        case '!':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Bang);    break;
        case '%':  lexer->token_buf[1] = lexer_new_token(lexer, Tok_Modulo);  break;
        case '&':  {
            if (lexer_peek_char(lexer) == '&') {
               lexer->end++;
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_And);
            } else {
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_BitAnd);
            }
        } break;
        case '|':  {
            if (lexer_peek_char(lexer) == '|') {
               lexer->end++;
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_Or);
            } else {
               lexer->token_buf[1] = lexer_new_token(lexer, Tok_BitOr);
            }
        } break;
        case '_': {
            if (!tokenize_identifier(lexer)) return false;
        } break;
        case '\'': {
            lexer->start = lexer->end; // resetting lexer to not include quote in character literal
            if (!tokenize_char(lexer)) return false;
            assertf(lexer->string.data[lexer->end] == '\'', "char literal is terminated");
            lexer->end++;
        } break;
        case '"': {
            lexer->start = lexer->end; // resetting lexer to not include quote in string
            if (!tokenize_string(lexer)) return false;
            assertf(lexer->string.data[lexer->end] == '"', "string literal is terminated");
            lexer->end++;
        } break;
        case '.': {
            if (between(lexer_peek_char(lexer), '0', '9')) {
                lexer->end--;  // resetting lexer before `.` was consumed
                if (!tokenize_number(lexer)) return false;
            } else {
                lexer->token_buf[1] = lexer_new_token(lexer, Tok_Dot);
            }
        } break;
        default: {
            if (between(ch, '0', '9')) {
                if (!tokenize_number(lexer)) return false;
            } else if (isalpha(ch)) {
                if (!tokenize_identifier(lexer)) return false;
            } else if (!isspace(ch)) {
                fprintf(stderr, "error: unexpected character, `%c` at: %zu\n", ch, lexer->end);
                return false;
            }
        } break;
    }

    *tok = lexer->token_buf[0];
    lexer->start = lexer->end;
    return true;
}


static inline bool consume_token(Lexer *lexer, Token *tok)  {
    if (lexer->token_buf[1].type == Tok_Invalid) {
        if (!next_token_impl(lexer, tok)) return false;
    }
    return next_token_impl(lexer, tok);
}

static inline Token next_token(Lexer *lexer) {
    Token tok = {0};
    consume_token(lexer, &tok);
    return tok;
}

static inline bool peek_token(Lexer *lexer, Token *tok) {
    if (lexer->token_buf[1].type == Tok_Invalid) {
        if (!next_token_impl(lexer, tok)) return false;
    }
    *tok = lexer->token_buf[1];
    return lexer->token_buf[1].type != Tok_Eof;
}

static inline bool match_token(Lexer *lexer, TokenType expected) {
    Token tok = {0};
    if (!peek_token(lexer, &tok)) return false;
    if (tok.type != expected) {
        return false;
    }
    return true;
}

static inline bool _match_token_any(Lexer *lexer, TokenType *expected, size_t expected_len) {
    Token tok = {0};
    if (!peek_token(lexer, &tok)) return false;
    for (size_t i = 0; i < expected_len; i++) {
        if (tok.type == expected[i]) {
            return true;
        }
    }
    return false;
}

static inline bool match_token_str(Lexer *lexer, TokenType expected, String token_str) {
    Token tok = {0};
    if (!peek_token(lexer, &tok)) return false;
    if (tok.type != expected) return false;
    if (!string_eq(tok.string, token_str)) return false;
    return true;
}

static inline bool expect_token(Lexer *lexer, TokenType expected) {
    bool matches = match_token(lexer, expected);
    next_token(lexer);
    return matches;
}

static inline bool expect_token_str(Lexer *lexer, TokenType expected, String str) {
    bool matches = match_token_str(lexer, expected, str);
    next_token(lexer);
    return matches;
}

#endif // !MIGI_LEXER_H
