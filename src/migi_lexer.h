#ifndef MIGI_LEXER_H
#define MIGI_LEXER_H

#include <assert.h>
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

typedef enum {
    TOK_NONE = 0,
    TOK_EOF,
    TOK_OPEN_PAREN,
    TOK_CLOSE_PAREN,
    TOK_OPEN_BRACE,
    TOK_CLOSE_BRACE,
    TOK_OPEN_BRACKET,
    TOK_CLOSE_BRACKET,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_EQUALS,
    TOK_MINUS_MINUS,
    TOK_PLUS_PLUS,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_DOUBLEQUOTE,
    TOK_SINGLEQUOTE,
    TOK_BACKSLASH,
    TOK_DOT,
    TOK_STRING,
    TOK_FLOATING,
    TOK_INTEGER,
    TOK_IDENTIFIER,

    TOK_COUNT
} TokenType;


typedef struct {
    TokenType type;
    String string;
    union {
        double floating;
        uint64_t integer;
    };
} Token;

typedef struct {
    String string;
    size_t start;
    size_t end;
    Token token_buf[2];
} Lexer;

typedef enum {
    KEYWORD_ALIGNAS,
    KEYWORD_ALIGNOF,
    KEYWORD_AUTO,
    KEYWORD_BOOL,
    KEYWORD_BREAK,
    KEYWORD_CASE,
    KEYWORD_CHAR,
    KEYWORD_CONST,
    KEYWORD_CONSTEXPR,
    KEYWORD_CONTINUE,
    KEYWORD_DEFAULT,
    KEYWORD_DO,
    KEYWORD_DOUBLE,
    KEYWORD_ELSE,
    KEYWORD_ENUM,
    KEYWORD_EXTERN,
    KEYWORD_FALSE,
    KEYWORD_FLOAT,
    KEYWORD_FOR,
    KEYWORD_GOTO,
    KEYWORD_IF,
    KEYWORD_INLINE,
    KEYWORD_INT,
    KEYWORD_LONG,
    KEYWORD_NULLPTR,
    KEYWORD_REGISTER,
    KEYWORD_RESTRICT,
    KEYWORD_RETURN,
    KEYWORD_SHORT,
    KEYWORD_SIGNED,
    KEYWORD_SIZEOF,
    KEYWORD_STATIC,
    KEYWORD_STATIC_ASSERT,
    KEYWORD_STRUCT,
    KEYWORD_SWITCH,
    KEYWORD_THREAD_LOCAL,
    KEYWORD_TRUE,
    KEYWORD_TYPEDEF,
    KEYWORD_TYPEOF,
    KEYWORD_TYPEOF_UNQUAL,
    KEYWORD_UNION,
    KEYWORD_UNSIGNED,
    KEYWORD_VOID,
    KEYWORD_VOLATILE,
    KEYWORD_WHILE,
    KEYWORD__ALIGNAS,
    KEYWORD__ALIGNOF,
    KEYWORD__ATOMIC,
    KEYWORD__BITINT,
    KEYWORD__BOOL,
    KEYWORD__COMPLEX,
    KEYWORD__DECIMAL128,
    KEYWORD__DECIMAL32,
    KEYWORD__DECIMAL64,
    KEYWORD__GENERIC,
    KEYWORD__IMAGINARY,
    KEYWORD__NORETURN,
    KEYWORD__STATIC_ASSERT,
    KEYWORD__THREAD_LOCAL,

    KEYWORD_COUNT,
} KeywordType;

static String keywords[KEYWORD_COUNT] = {
    [KEYWORD_ALIGNAS]        = SV("alignas"),
    [KEYWORD_ALIGNOF]        = SV("alignof"),
    [KEYWORD_AUTO]           = SV("auto"),
    [KEYWORD_BOOL]           = SV("bool"),
    [KEYWORD_BREAK]          = SV("break"),
    [KEYWORD_CASE]           = SV("case"),
    [KEYWORD_CHAR]           = SV("char"),
    [KEYWORD_CONST]          = SV("const"),
    [KEYWORD_CONSTEXPR]      = SV("constexpr"),
    [KEYWORD_CONTINUE]       = SV("continue"),
    [KEYWORD_DEFAULT]        = SV("default"),
    [KEYWORD_DO]             = SV("do"),
    [KEYWORD_DOUBLE]         = SV("double"),
    [KEYWORD_ELSE]           = SV("else"),
    [KEYWORD_ENUM]           = SV("enum"),
    [KEYWORD_EXTERN]         = SV("extern"),
    [KEYWORD_FALSE]          = SV("false"),
    [KEYWORD_FLOAT]          = SV("float"),
    [KEYWORD_FOR]            = SV("for"),
    [KEYWORD_GOTO]           = SV("goto"),
    [KEYWORD_IF]             = SV("if"),
    [KEYWORD_INLINE]         = SV("inline"),
    [KEYWORD_INT]            = SV("int"),
    [KEYWORD_LONG]           = SV("long"),
    [KEYWORD_NULLPTR]        = SV("nullptr"),
    [KEYWORD_REGISTER]       = SV("register"),
    [KEYWORD_RESTRICT]       = SV("restrict"),
    [KEYWORD_RETURN]         = SV("return"),
    [KEYWORD_SHORT]          = SV("short"),
    [KEYWORD_SIGNED]         = SV("signed"),
    [KEYWORD_SIZEOF]         = SV("sizeof"),
    [KEYWORD_STATIC]         = SV("static"),
    [KEYWORD_STATIC_ASSERT]  = SV("static_assert"),
    [KEYWORD_STRUCT]         = SV("struct"),
    [KEYWORD_SWITCH]         = SV("switch"),
    [KEYWORD_THREAD_LOCAL]   = SV("thread_local"),
    [KEYWORD_TRUE]           = SV("true"),
    [KEYWORD_TYPEDEF]        = SV("typedef"),
    [KEYWORD_TYPEOF]         = SV("typeof"),
    [KEYWORD_TYPEOF_UNQUAL]  = SV("typeof_unqual"),
    [KEYWORD_UNION]          = SV("union"),
    [KEYWORD_UNSIGNED]       = SV("unsigned"),
    [KEYWORD_VOID]           = SV("void"),
    [KEYWORD_VOLATILE]       = SV("volatile"),
    [KEYWORD_WHILE]          = SV("while"),
    [KEYWORD__ALIGNAS]       = SV("_Alignas"),
    [KEYWORD__ALIGNOF]       = SV("_Alignof"),
    [KEYWORD__ATOMIC]        = SV("_Atomic"),
    [KEYWORD__BITINT]        = SV("_BitInt"),
    [KEYWORD__BOOL]          = SV("_Bool"),
    [KEYWORD__COMPLEX]       = SV("_Complex"),
    [KEYWORD__DECIMAL128]    = SV("_Decimal128"),
    [KEYWORD__DECIMAL32]     = SV("_Decimal32"),
    [KEYWORD__DECIMAL64]     = SV("_Decimal64"),
    [KEYWORD__GENERIC]       = SV("_Generic"),
    [KEYWORD__IMAGINARY]     = SV("_Imaginary"),
    [KEYWORD__NORETURN]      = SV("_Noreturn"),
    [KEYWORD__STATIC_ASSERT] = SV("_Static_assert"),
    [KEYWORD__THREAD_LOCAL]  = SV("_Thread_local"),
};

static_assert(array_len(keywords) == KEYWORD_COUNT, "Keyword count has changed");

typedef struct {
    KeywordType type;
    String string;
} Keyword;


static inline bool next_token(Lexer *lexer, Token *tok);
static inline bool peek_token(Lexer *lexer, Token *tok);
static inline bool match_token(Lexer *lexer, TokenType expected);
static bool identifier_to_keyword(Token identifier, Keyword *keyword);


static bool identifier_to_keyword(Token identifier, Keyword *keyword) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (string_eq(identifier.string, keywords[i])) {
            *keyword = (Keyword){
                .type = i,
                .string = keywords[i],
            };
            return true;
        }
    }
    return false;
}


#define TOK_NEW(lexer, tok_type)                      \
    ((Token){                                         \
     .type   = (tok_type),                            \
     .string = (String){                              \
         .data = &lexer->string.data[(lexer)->start], \
         .length = lexer->end - (lexer)->start        \
     }, {0}})


// TODO: rename this to something else (GET_CHAR (?))
#define LEXER_GET(lexer) (lexer)->string.data[(lexer)->end]

static bool tokenize_string(Lexer *lexer) {
    TIME_FUNCTION;

    bool closed = false;
    size_t start_index = lexer->end;
    while (lexer->end < lexer->string.length) {
        char ch = LEXER_GET(lexer);
        if (ch == '"') {
            closed = true;
            break;
        }
        lexer->end++;
    }
    if (!closed) {
        fprintf(stderr, "error: unmatched '\"' at index: %zu\n", start_index);
        return false;
    }

    lexer->token_buf[1] = (Token){
        .type = TOK_STRING,
        .string = (String){
            .data = &lexer->string.data[start_index],
            .length = lexer->end - start_index
        }
    };
    return true;
}

static bool tokenize_number(Lexer *lexer) {
    TIME_FUNCTION;

    size_t number_start = lexer->end;
    while (lexer->end < lexer->string.length) {
        char ch = LEXER_GET(lexer);
        if (!between(ch, '0', '9')) {
            break;
        }
        lexer->end++;
    }

    if (lexer->end < lexer->string.length) {
        char ch = LEXER_GET(lexer);

        // parsing as floating point
        if (ch == 'e' || ch == 'E' || ch == '.') {
            while (lexer->end < lexer->string.length) {
                char ch = LEXER_GET(lexer);
                if (!(ch == 'e' || ch == 'E' || ch == '.' || between(ch, '0', '9'))) {
                    break;
                }
                lexer->end++;
            }

            String number_str = string_slice(lexer->string, number_start, lexer->end);
            // TODO: implement strtod rather than depending on it
            // NOTE: Since the next character after the end of the floating point literal
            // is guaranteed to be something not part of the literal (e, E, ., number),
            // using strtod with a non null-terminated string should be fine here
            char *end_ptr;
            double number = strtod(number_str.data, &end_ptr);

            if (end_ptr != number_str.data + number_str.length) {
                fprintf(stderr, "error: invalid floating point constant, `%.*s`, at: %zu\n",
                        SV_FMT(number_str), lexer->end);
                return false;
            }

            lexer->token_buf[1] = (Token){
                .type = TOK_FLOATING,
                .string = number_str,
                .floating = number
            };
            return true;
        } 
    }

    // parsing as integer
    String num = string_slice(lexer->string, number_start, lexer->end);
    lexer->token_buf[1] = (Token){
        .type = TOK_INTEGER,
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

    size_t identifier_start = lexer->end;
    while (lexer->end < lexer->string.length) {
        char ch = LEXER_GET(lexer);
        if (!(between(ch, 'a', 'z')
            || between(ch, 'A', 'Z')
            || between(ch, '0', '9')
            || ch == '_'))
                break;
        lexer->end++;
    }

    lexer->token_buf[1] = (Token){
        .type = TOK_IDENTIFIER,
        .string = string_slice(lexer->string, identifier_start, lexer->end),
    };
    return true;
}

static inline char peek(Lexer *lexer) {
    if (lexer->end >= lexer->string.length) {
        return 0;
    }
    return lexer->string.data[lexer->end + 1];
}

static bool next_token_impl(Lexer *lexer, Token *tok)  {
    TIME_FUNCTION;

    lexer->token_buf[0] = lexer->token_buf[1];
    while (lexer->end < lexer->string.length) {
        if (!isspace(LEXER_GET(lexer))) break;
        lexer->end++;
    }

    if (lexer->end >= lexer->string.length) {
        lexer->token_buf[1] = TOK_NEW(lexer, TOK_EOF);
    } else {
        char ch = LEXER_GET(lexer);
        // TODO: do C lexing here
        switch(ch) {
            case '(':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_OPEN_PAREN);     break;
            case ')':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_CLOSE_PAREN);    break;
            case '{':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_OPEN_BRACE);     break;
            case '}':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_CLOSE_BRACE);    break;
            case '[':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_OPEN_BRACKET);   break;
            case ']':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_CLOSE_BRACKET);  break;
            case ',':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_COMMA);          break;
            case '\'': lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_SINGLEQUOTE);    break;
            case '\\': lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_BACKSLASH);      break;
            case '.':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_DOT);            break;
            case ':':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_COLON);          break;
            case ';':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_SEMICOLON);      break;
            case '-':  {
                lexer->end++;
                if (lexer->end < lexer->string.length && LEXER_GET(lexer) == '-') {
                    lexer->end++;
                    lexer->token_buf[1] = TOK_NEW(lexer, TOK_MINUS_MINUS);
                } else {
                    lexer->token_buf[1] = TOK_NEW(lexer, TOK_MINUS);
                }
            } break;
            case '+':  {
                lexer->end++;
                if (lexer->end < lexer->string.length && LEXER_GET(lexer) == '+') {
                    lexer->end++;
                    lexer->token_buf[1] = TOK_NEW(lexer, TOK_PLUS_PLUS);
                } else {
                    lexer->token_buf[1] = TOK_NEW(lexer, TOK_PLUS);
                }
            } break;
            case '=':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_EQUALS);         break;
            case '*':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_STAR);           break;
            case '/':  lexer->end++; lexer->token_buf[1] = TOK_NEW(lexer, TOK_SLASH);          break;
            case '_': {
                if (!tokenize_identifier(lexer)) return false;
            } break;
            case '"': {
                lexer->end++;
                if (!tokenize_string(lexer)) return false;
                assertf(lexer->string.data[lexer->end] == '\"', "string literal is terminated");
                lexer->end++;
            } break;
            default: {
                if (between(ch, '0', '9')) {
                    if (!tokenize_number(lexer)) return false;
                } else if (isalpha(ch)) {
                    if (!tokenize_identifier(lexer)) return false;
                } else if (!isspace(ch)) {
                    fprintf(stderr, "error: unexpected token, `%c` at: %zu\n", ch, lexer->end);
                    return false;
                }
            } break;
        }
    }

    *tok = lexer->token_buf[0];
    lexer->start = lexer->end;
    return true;
}


static inline bool next_token(Lexer *lexer, Token *tok)  {
    if (lexer->token_buf[1].type == TOK_NONE) {
        if (!next_token_impl(lexer, tok)) return false;
    }
    return next_token_impl(lexer, tok);
}


static inline bool peek_token(Lexer *lexer, Token *tok) {
    *tok = lexer->token_buf[1];
    return lexer->token_buf[1].type != TOK_EOF;
}

static inline bool match_token(Lexer *lexer, TokenType expected) {
    Token tok = {0};
    return peek_token(lexer, &tok) && tok.type == expected;
}


#endif // !MIGI_LEXER_H
