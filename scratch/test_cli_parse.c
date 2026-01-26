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
            strlist_foreach(&flag->values, value) {
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
    StringList foo = flag_as_strlist(&cli, S("foo"));
    strlist_foreach(&foo, value) {
        printf("%.*s, ", SV_FMT(value->string));
    }
    printf("\n");

    printf("bar: %lld\n", flag_as_i64(&cli, S("bar"), 10));

    printf("baz: %f\n", flag_as_f64(&cli, S("baz"), 0));

    printf("help: %d\n", flag_exists(&cli, S("help")));
    printf("v: %d\n", flag_exists(&cli, S("v")));

    printf("color: %d\n", flag_as_bool(&cli, S("color")));
    printf("t: %d\n", flag_as_bool(&cli, S("t")));

    return 0;
}

