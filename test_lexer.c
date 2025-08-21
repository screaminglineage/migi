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
            case Tok_Identifier:  {
                printf("got '%.*s': %.*s\n", SV_FMT(TOKEN_STRINGS[tok.type]), SV_FMT(tok.string)); break;
            } break;
            case Tok_Integer:   printf("got '%.*s': %lu\n", SV_FMT(TOKEN_STRINGS[tok.type]), tok.integer); break;
            case Tok_Floating:  printf("got '%.*s': %f\n", SV_FMT(TOKEN_STRINGS[tok.type]), tok.floating); break;

            case Tok_Count:
            case Tok_Invalid: {
                printf("error: unexpected token, `%.*s`, at: %zu\n",
                        SV_FMT(TOKEN_STRINGS[tok.type]), lexer->end - tok.string.length);
                return false;
            } break;
            default: {
                printf("got '%.*s': %.*s\n", SV_FMT(TOKEN_STRINGS[tok.type]), SV_FMT(tok.string));
                if (tok.type == Tok_Eof) return true;
            } break;
        }
    }
}

int main() {
    StringBuilder sb = {0};
    read_file(&sb, SV("tokens.c"));

    Lexer l = {.string = sb_to_string(&sb)};

    while (match_token_any(&l, (TokenType[]){Tok_Identifier, Tok_OpenParen, Tok_CloseParen, Tok_OpenBrace})) {
        Token tok = {0};
        consume_token(&l, &tok);
        printf("%.*s\n", SV_FMT(tok.string));
    }
    mem_clear_single(&l);
    l.string = sb_to_string(&sb);

    return_val_if_false(expect_token_str(&l, Tok_Identifier, SV("int")), 1);
    return_val_if_false(expect_token_str(&l, Tok_Identifier, SV("main")), 1);
    return_val_if_false(expect_token(&l, Tok_OpenParen), 1);
    return_val_if_false(expect_token(&l, Tok_CloseParen), 1);
    return_val_if_false(expect_token(&l, Tok_OpenBrace), 1);
    return_val_if_false(expect_token_str(&l, Tok_Identifier, SV("return")), 1);
    return_val_if_false(expect_token(&l, Tok_Minus), 1);
    return_val_if_false(expect_token(&l, Tok_Floating), 1);
    return_val_if_false(expect_token(&l, Tok_Semicolon), 1);
    return_val_if_false(expect_token(&l, Tok_CloseBrace), 1);
    dump_tokens(&l);
    return 0;
}
