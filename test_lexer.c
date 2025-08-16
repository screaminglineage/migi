#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "src/migi.h"
#include "src/migi_string.h"
#include "src/migi_lexer.h"
#pragma GCC diagnostic pop

bool dump_tokens(Lexer *lexer) {
    Token tok = {0};
    while (true) {
        if (!consume_token(lexer, &tok)) {
            return false;
        }

        switch (tok.type) {
            case TOK_IDENTIFIER:  {
                Keyword keyword = {0};
                if (identifier_to_keyword(tok, &keyword)) {
                    printf("got 'keyword': %.*s\n", SV_FMT(tok.string)); break;
                } else {
                    printf("got '%.*s': %.*s\n", SV_FMT(TOKEN_STRINGS[tok.type]), SV_FMT(tok.string)); break;
                }
            } break;
            case TOK_INTEGER:   printf("got '%.*s': %lu\n", SV_FMT(TOKEN_STRINGS[tok.type]), tok.integer); break;
            case TOK_FLOATING:  printf("got '%.*s': %f\n", SV_FMT(TOKEN_STRINGS[tok.type]), tok.floating); break;

            case TOK_COUNT:
            case TOK_NONE: {
                printf("error: unexpected token, `%.*s`, at: %zu\n",
                        SV_FMT(TOKEN_STRINGS[tok.type]), lexer->end - tok.string.length);
                return false;
            } break;
            default: {
                printf("got '%.*s': %.*s\n", SV_FMT(TOKEN_STRINGS[tok.type]), SV_FMT(tok.string));
                if (tok.type == TOK_EOF) return true;
            } break;
        }
    }
}

int main() {
    StringBuilder sb = {0};
    read_file(&sb, SV("tokens.c"));

    Lexer l = {.string = sb_to_string(&sb)};

    while (match_token_any(&l, (TokenType[]){TOK_IDENTIFIER, TOK_OPEN_PAREN, TOK_CLOSE_PAREN, TOK_OPEN_BRACE})) {
        Token tok = {0};
        consume_token(&l, &tok);
        printf("%.*s\n", SV_FMT(tok.string));
    }
    l.start = l.end = 0;

    return_val_if_false(expect_token_str(&l, TOK_IDENTIFIER, SV("int")), 1);
    return_val_if_false(expect_token_str(&l, TOK_IDENTIFIER, SV("main")), 1);
    return_val_if_false(expect_token(&l, TOK_OPEN_PAREN), 1);
    return_val_if_false(expect_token(&l, TOK_CLOSE_PAREN), 1);
    return_val_if_false(expect_token(&l, TOK_OPEN_BRACE), 1);
    return_val_if_false(expect_token_str(&l, TOK_IDENTIFIER, SV("return")), 1);
    return_val_if_false(expect_token(&l, TOK_MINUS), 1);
    return_val_if_false(expect_token(&l, TOK_FLOATING), 1);
    return_val_if_false(expect_token(&l, TOK_SEMICOLON), 1);
    return_val_if_false(expect_token(&l, TOK_CLOSE_BRACE), 1);
    dump_tokens(&l);
    return 0;
}
