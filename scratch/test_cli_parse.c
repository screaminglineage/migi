#include "migi.h"
#include "cli_parse.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    Arena *arena = arena_init();
    CmdLn cli = cli_parse_args(arena, argc, argv);

    printf("Executable: %.*s\n", SV_FMT(cli.executable));

    printf("\nFlag Arguments:\n");
    flag_foreach(cli, flag) {
        if (flag->values.length > 0) {
            printf("%.*s: ", SV_FMT(flag->key));
            list_foreach(flag->values.head, StringNode, value) {
                printf("%.*s, ", SV_FMT(value->string));
            }
            printf("\n");
        } else {
            printf("%.*s: %.*s\n", SV_FMT(flag->key), SV_FMT(flag->value));
        }
    }

    printf("\nPositional Arguments:\n");
    flag_args_foreach(cli, arg) {
        printf("%.*s\n", SV_FMT(arg->string));
    }

    printf("\n-----------------------\n");

    printf("foo: ");
    StringList foo = flag_as_strlist(&cli, SV("foo"));
    list_foreach(foo.head, StringNode, value) {
        printf("%.*s, ", SV_FMT(value->string));
    }
    printf("\n");

    printf("bar: %lld\n", flag_as_i64(&cli, SV("bar"), 10));

    printf("baz: %f\n", flag_as_f64(&cli, SV("baz"), 0));

    printf("help: %d\n", flag_exists(&cli, SV("help")));
    printf("v: %d\n", flag_exists(&cli, SV("v")));

    printf("color: %d\n", flag_as_bool(&cli, SV("color")));
    printf("t: %d\n", flag_as_bool(&cli, SV("t")));

    return 0;
}

