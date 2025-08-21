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
    Tok_None = 0,
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
    Tok_PlusPlus,
    Tok_Question,
    Tok_Colon,
    Tok_Semicolon,
    Tok_Comma,
    Tok_Doublequote,
    Tok_Singlequote,
    Tok_Backslash,
    Tok_Dot,
    Tok_String,
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
    [Tok_None]                = SV("none"),
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
    [Tok_PlusPlus]            = SV("++"),
    [Tok_Question]            = SV("?"),
    [Tok_Colon]               = SV(":"),
    [Tok_Semicolon]           = SV(";"),
    [Tok_Comma]               = SV("),"),
    [Tok_Doublequote]         = SV("\""),
    [Tok_Singlequote]         = SV("\'"),
    [Tok_Backslash]           = SV("\\"),
    [Tok_Dot]                 = SV("."),
    [Tok_String]              = SV("string literal"),
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

typedef struct {
    String string;
    size_t start;
    size_t end;
    Token token_buf[2];
} Lexer;

typedef enum {
    Keyword_Alignas,
    Keyword_Alignof,
    Keyword_Auto,
    Keyword_Bool,
    Keyword_Break,
    Keyword_Case,
    Keyword_Char,
    Keyword_Const,
    Keyword_Constexpr,
    Keyword_Continue,
    Keyword_Default,
    Keyword_Do,
    Keyword_Double,
    Keyword_Else,
    Keyword_Enum,
    Keyword_Extern,
    Keyword_False,
    Keyword_Float,
    Keyword_For,
    Keyword_Goto,
    Keyword_If,
    Keyword_Inline,
    Keyword_Int,
    Keyword_Long,
    Keyword_Nullptr,
    Keyword_Register,
    Keyword_Restrict,
    Keyword_Return,
    Keyword_Short,
    Keyword_Signed,
    Keyword_Sizeof,
    Keyword_Static,
    Keyword_StaticAssert,
    Keyword_Struct,
    Keyword_Switch,
    Keyword_ThreadLocal,
    Keyword_True,
    Keyword_Typedef,
    Keyword_Typeof,
    Keyword_TypeofUnqual,
    Keyword_Union,
    Keyword_Unsigned,
    Keyword_Void,
    Keyword_Volatile,
    Keyword_While,
    Keyword__Alignas,
    Keyword__Alignof,
    Keyword__Atomic,
    Keyword__Bitint,
    Keyword__Bool,
    Keyword__Complex,
    Keyword__Decimal128,
    Keyword__Decimal32,
    Keyword__Decimal64,
    Keyword__Generic,
    Keyword__Imaginary,
    Keyword__Noreturn,
    Keyword__StaticAssert,
    Keyword__ThreadLocal,

    Keyword_Count,
} KeywordType;

static String KEYWORD_STRINGS[Keyword_Count] = {
    [Keyword_Alignas]               = SV("alignas"),
    [Keyword_Alignof]               = SV("alignof"),
    [Keyword_Auto]                  = SV("auto"),
    [Keyword_Bool]                  = SV("bool"),
    [Keyword_Break]                 = SV("break"),
    [Keyword_Case]                  = SV("case"),
    [Keyword_Char]                  = SV("char"),
    [Keyword_Const]                 = SV("const"),
    [Keyword_Constexpr]             = SV("constexpr"),
    [Keyword_Continue]              = SV("continue"),
    [Keyword_Default]               = SV("default"),
    [Keyword_Do]                    = SV("do"),
    [Keyword_Double]                = SV("double"),
    [Keyword_Else]                  = SV("else"),
    [Keyword_Enum]                  = SV("enum"),
    [Keyword_Extern]                = SV("extern"),
    [Keyword_False]                 = SV("false"),
    [Keyword_Float]                 = SV("float"),
    [Keyword_For]                   = SV("for"),
    [Keyword_Goto]                  = SV("goto"),
    [Keyword_If]                    = SV("if"),
    [Keyword_Inline]                = SV("inline"),
    [Keyword_Int]                   = SV("int"),
    [Keyword_Long]                  = SV("long"),
    [Keyword_Nullptr]               = SV("nullptr"),
    [Keyword_Register]              = SV("register"),
    [Keyword_Restrict]              = SV("restrict"),
    [Keyword_Return]                = SV("return"),
    [Keyword_Short]                 = SV("short"),
    [Keyword_Signed]                = SV("signed"),
    [Keyword_Sizeof]                = SV("sizeof"),
    [Keyword_Static]                = SV("static"),
    [Keyword_StaticAssert]          = SV("static_assert"),
    [Keyword_Struct]                = SV("struct"),
    [Keyword_Switch]                = SV("switch"),
    [Keyword_ThreadLocal]           = SV("thread_local"),
    [Keyword_True]                  = SV("true"),
    [Keyword_Typedef]               = SV("typedef"),
    [Keyword_Typeof]                = SV("typeof"),
    [Keyword_TypeofUnqual]          = SV("typeof_unqual"),
    [Keyword_Union]                 = SV("union"),
    [Keyword_Unsigned]              = SV("unsigned"),
    [Keyword_Void]                  = SV("void"),
    [Keyword_Volatile]              = SV("volatile"),
    [Keyword_While]                 = SV("while"),
    [Keyword__Alignas]              = SV("_Alignas"),
    [Keyword__Alignof]              = SV("_Alignof"),
    [Keyword__Atomic]               = SV("_Atomic"),
    [Keyword__Bitint]               = SV("_BitInt"),
    [Keyword__Bool]                 = SV("_Bool"),
    [Keyword__Complex]              = SV("_Complex"),
    [Keyword__Decimal128]           = SV("_Decimal128"),
    [Keyword__Decimal32]            = SV("_Decimal32"),
    [Keyword__Decimal64]            = SV("_Decimal64"),
    [Keyword__Generic]              = SV("_Generic"),
    [Keyword__Imaginary]            = SV("_Imaginary"),
    [Keyword__Noreturn]             = SV("_Noreturn"),
    [Keyword__StaticAssert]         = SV("_Static_assert"),
    [Keyword__ThreadLocal]          = SV("_Thread_local"),
};

static_assert(array_len(KEYWORD_STRINGS) == Keyword_Count, "Keyword count has changed");

typedef struct {
    KeywordType type;
    String string;
} Keyword;


// Consume the next token
static inline bool consume_token(Lexer *lexer, Token *tok);

// Advance lexer to next token, asserting that it is valid
static inline Token next_token(Lexer *lexer);

// Get the next token without consuming it
static inline bool peek_token(Lexer *lexer, Token *tok);

// Check if the next token is the same as expected and consume it
// Doesn't consume the token if its different
static inline bool expect_token(Lexer *lexer, TokenType expected);
static inline bool expect_token_str(Lexer *lexer, TokenType expected, String str);

// Check if the next token is the same as expected
static inline bool match_token(Lexer *lexer, TokenType expected);
static inline bool match_token_str(Lexer *lexer, TokenType expected, String token_str);

// Check if the next token is one of the passed in tokens
// bool match_token_any(Lexer *lexer, (TokenType[]){ ... })
#define match_token_any(lexer, ...) \
    (_match_token_any((lexer), __VA_ARGS__, sizeof((__VA_ARGS__))/sizeof(*(__VA_ARGS__))))

// Try to convert an indentifier to a keyword
static bool identifier_to_keyword(Token identifier, Keyword *keyword);

static bool identifier_to_keyword(Token identifier, Keyword *keyword) {
    for (size_t i = 0; i < Keyword_Count; i++) {
        if (string_eq(identifier.string, KEYWORD_STRINGS[i])) {
            *keyword = (Keyword){
                .type = i,
                .string = KEYWORD_STRINGS[i],
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

static bool tokenize_string(Lexer *lexer) {
    TIME_FUNCTION;

    size_t start_index = lexer->start;
    char ch = '\0';
    while (true) {
        ch = lexer_peek_char(lexer);
        if (ch == '"' || ch == '\0') break;
        lexer->end++;
    }
    if (ch == '\0') {
        fprintf(stderr, "error: unmatched '\"' at index: %zu\n", start_index);
        return false;
    }

    lexer->token_buf[1] = (Token){
        .type = Tok_String,
        .string = (String){
            .data = &lexer->string.data[start_index],
            .length = lexer->end - start_index
        }
    };
    return true;
}

static bool tokenize_number(Lexer *lexer) {
    TIME_FUNCTION;

    size_t number_start = lexer->start;
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

            String number_str = string_slice(lexer->string, number_start, lexer->end);
            // TODO: implement strtod rather than depending on it
            // NOTE: Since the next character after the end of the floating point literal
            // is guaranteed to be something not part of the literal (e, E, ., number),
            // using strtod with a non null-terminated string should be fine here
            char *end_ptr;
            double number = strtod(number_str.data, &end_ptr);

            if (end_ptr != number_str.data + number_str.length) {
                fprintf(stderr, "error: invalid floating point constant, `%.*s`, at: %zu\n",
                        SV_FMT(number_str), number_start);
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
    String num = string_slice(lexer->string, number_start, lexer->end);
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


static bool next_token_impl(Lexer *lexer, Token *tok)  {
    TIME_FUNCTION;

    lexer->token_buf[0] = lexer->token_buf[1];
    while (true) {
        if (!isspace(lexer_peek_char(lexer))) break;
        lexer->end++;
    }
    lexer->start = lexer->end;

    char ch = lexer_consume_char(lexer);
    // TODO: do C lexing here
    switch(ch) {
        case '\0': lexer->token_buf[1] = TOK_NEW(lexer, Tok_Eof);            break;
        case '(':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_OpenParen);     break;
        case ')':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_CloseParen);    break;
        case '{':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_OpenBrace);     break;
        case '}':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_CloseBrace);    break;
        case '[':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_OpenBracket);   break;
        case ']':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_CloseBracket);  break;
        case ',':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Comma);          break;
        case '\'': lexer->token_buf[1] = TOK_NEW(lexer, Tok_Singlequote);    break;
        case '\\': lexer->token_buf[1] = TOK_NEW(lexer, Tok_Backslash);      break;
        case '.':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Dot);            break;
        case '?':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Question);       break;
        case ':':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Colon);          break;
        case ';':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Semicolon);      break;
        case '#':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Hash);           break;
        case '-':  {
            if (lexer_peek_char(lexer) == '-') {
               lexer->end++;
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_MinusMinus);
            } else {
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_Minus);
            }
        } break;
        case '+':  {
            if (lexer_peek_char(lexer) == '+') {
                lexer->end++;
                lexer->token_buf[1] = TOK_NEW(lexer, Tok_PlusPlus);
            } else {
                lexer->token_buf[1] = TOK_NEW(lexer, Tok_Plus);
            }
        } break;
        case '*': lexer->token_buf[1] = TOK_NEW(lexer, Tok_Star); break;
        case '/': {
            if (lexer_peek_char(lexer) == '/') {
                while (lexer_peek_char(lexer) != '\n') lexer->end++;
                lexer->start = lexer->end;
                // TODO: remove recursive lexing, can overflow if too many comments follow each other
                if (!next_token_impl(lexer, tok)) return false;

            } else if (lexer_peek_char(lexer) == '*') {
                while (!(lexer_peek_char(lexer) == '*'
                        && lexer_peek_next_char(lexer) == '/'))
                    lexer->end++;

                lexer->end++;
                lexer->end++;
                lexer->start = lexer->end;
                // TODO: remove recursive lexing, can overflow if too many comments follow each other
                if (!next_token_impl(lexer, tok)) return false;
            } else {
                lexer->token_buf[1] = TOK_NEW(lexer, Tok_Slash);
            }
        } break;
        case '<':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Lesser); break;
        case '>':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Greater); break;
        case '=':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Equals); break;
        case '!':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Bang); break;
        case '%':  lexer->token_buf[1] = TOK_NEW(lexer, Tok_Modulo); break;
        case '&':  {
            if (lexer_peek_char(lexer) == '&') {
               lexer->end++;
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_And);
            } else {
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_BitAnd);
            }
        } break;
        case '|':  {
            if (lexer_peek_char(lexer) == '|') {
               lexer->end++;
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_Or);
            } else {
               lexer->token_buf[1] = TOK_NEW(lexer, Tok_BitOr);
            }
        } break;
        case '_': {
            if (!tokenize_identifier(lexer)) return false;
        } break;
        case '"': {
            lexer->start = lexer->end; // resetting lexer to not include quote in string
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
    if (lexer->token_buf[1].type == Tok_None) {
        if (!next_token_impl(lexer, tok)) return false;
    }
    return next_token_impl(lexer, tok);
}

static inline Token next_token(Lexer *lexer) {
    Token tok = {0};
    bool valid = consume_token(lexer, &tok);
    assertf(valid, "%s: next_token() failed", __func__);
    return tok;
}

static inline bool peek_token(Lexer *lexer, Token *tok) {
    if (lexer->token_buf[1].type == Tok_None) {
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
    if (!match_token(lexer, expected)) return false;
    Token tok = {0};
    return consume_token(lexer, &tok);
}

static inline bool expect_token_str(Lexer *lexer, TokenType expected, String str) {
    if (!match_token_str(lexer, expected, str)) return false;
    Token tok = {0};
    return consume_token(lexer, &tok);
}

#endif // !MIGI_LEXER_H
