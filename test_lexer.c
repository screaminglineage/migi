#include "src/migi_string.h"
#include "src/migi_lexer.h"

bool parse(Lexer *lexer) {
    Token tok = {0};
    while (true) {
        if (!next_token(lexer, &tok)) {
            return false;
        }

        switch (tok.type) {
            case TOK_EOF:         printf("got 'TOK_EOF'\n"); return true;
            case TOK_STRING:      printf("got 'TOK_STRING': `%.*s`\n", SV_FMT(tok.string)); break;
            case TOK_OPEN_PAREN:  printf("got 'TOK_OPEN_PAREN'\n"); break;
            case TOK_CLOSE_PAREN: printf("got 'TOK_CLOSE_PAREN'\n"); break;
            case TOK_OPEN_BRACE:  printf("got 'TOK_OPEN_BRACE'\n"); break;
            case TOK_CLOSE_BRACE: printf("got 'TOK_CLOSE_BRACE'\n"); break;
            case TOK_IDENTIFIER:  {
                Keyword keyword = {0};
                if (identifier_to_keyword(tok, &keyword)) {
                    printf("got 'TOK_KEYWORD': %.*s\n", SV_FMT(keyword.string));
                } else {
                    printf("got 'TOK_IDENTIFIER': %.*s\n", SV_FMT(tok.string));
                }
            } break;
            case TOK_PLUS:      printf("got 'TOK_PLUS'\n"); break;
            case TOK_MINUS:     printf("got 'TOK_MINUS'\n"); break;
            case TOK_EQUALS:    printf("got 'TOK_EQUALS'\n"); break;
            case TOK_COMMA:     printf("got 'TOK_COMMA'\n"); break;
            case TOK_INTEGER:   printf("got 'TOK_INTEGER': %lu\n", tok.integer); break;
            case TOK_FLOATING:  printf("got 'TOK_FLOATING': %f\n", tok.floating); break;
            case TOK_SEMICOLON: printf("got 'TOK_SEMICOLON'\n"); break;
            case TOK_PLUS_PLUS: printf("got 'TOK_PLUS_PLUS'\n"); break;
            case TOK_MINUS_MINUS: printf("got 'TOK_MINUS_MINUS'\n"); break;
            default: {
                fprintf(stderr, "error: unexpected token, `%.*s`, at: %zu\n", SV_FMT(tok.string), lexer->end - tok.string.length);
                return false;
            } break;
        }
    }
}

int main() {
    StringBuilder sb = {0};
    read_file(&sb, SV("tokens.c"));

    Lexer l = {.string = sb_to_string(&sb)};
    parse(&l);
    return 0;
}
