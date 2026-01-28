#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "migi.h"
#include "migi_string.h"
#include "string_builder.h"
#include "migi_lexer.h"
#pragma GCC diagnostic pop

bool dump_tokens(Lexer *lexer) {
    Token tok = {0};
    while (true) {
        if (!consume_token(lexer, &tok)) {
            return false;
        }

        switch (tok.type) {
            case Tok_Identifier:  {
                printf("got '%.*s': %.*s\n", SArg(TOKEN_STRINGS[tok.type]), SArg(tok.string)); break;
            } break;
            case Tok_Integer:   printf("got '%.*s': %lu\n", SArg(TOKEN_STRINGS[tok.type]), tok.integer); break;
            case Tok_Floating:  printf("got '%.*s': %f\n", SArg(TOKEN_STRINGS[tok.type]), tok.floating); break;

            case Tok_Count:
            case Tok_Invalid: {
                printf("error: unexpected token, `%.*s`, at: %zu\n",
                        SArg(TOKEN_STRINGS[tok.type]), lexer->end - tok.string.length);
                return false;
            } break;
            default: {
                printf("got '%.*s': %.*s\n", SArg(TOKEN_STRINGS[tok.type]), SArg(tok.string));
                if (tok.type == Tok_Eof) return true;
            } break;
        }
    }
}

int main() {
    StringBuilder sb = sb_init();
    sb_push_file(&sb, S("scratch/test_lexer.c"));

    Lexer l = {.string = sb_to_string(&sb)};

    while (match_token_any(&l, (TokenType[]){Tok_Identifier, Tok_OpenParen, Tok_CloseParen, Tok_OpenBrace})) {
        Token tok = {0};
        consume_token(&l, &tok);
        printf("%.*s\n", SArg(tok.string));
    }
    mem_clear(&l);
    l.string = sb_to_string(&sb);

    // return_val_if_false(expect_token_str(&l, Tok_Identifier, S("int")), 1);
    // return_val_if_false(expect_token_str(&l, Tok_Identifier, S("main")), 1);
    // return_val_if_false(expect_token(&l, Tok_OpenParen), 1);
    // return_val_if_false(expect_token(&l, Tok_CloseParen), 1);
    // return_val_if_false(expect_token(&l, Tok_OpenBrace), 1);
    // return_val_if_false(expect_token_str(&l, Tok_Identifier, S("return")), 1);
    // return_val_if_false(expect_token(&l, Tok_Minus), 1);
    // return_val_if_false(expect_token(&l, Tok_Floating), 1);
    // return_val_if_false(expect_token(&l, Tok_Semicolon), 1);
    // return_val_if_false(expect_token(&l, Tok_CloseBrace), 1);
    dump_tokens(&l);
    return 0;
}
