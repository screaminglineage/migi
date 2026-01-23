#include "migi.h"
#include "../scratch/cli_parse.h"

int main(int argc, char *argv[]) {
    Arena *arena = arena_init();
    FlagTable flags = cli_parse_args(arena, argc, argv);

    printf("Executable: %.*s\n", SV_FMT(flags.executable));

    printf("\nFlag Arguments:\n");
    flag_foreach(flags, flag) {
        printf("%.*s: %.*s\n", SV_FMT(flag->key), SV_FMT(flag->value));
    }

    printf("\nPositional Arguments:\n");
    flag_args_foreach(flags, arg) {
        printf("%.*s\n", SV_FMT(arg->string));
    }

    printf("\n-----------------------\n");
    printf("foo: %ld\n", flag_as_i64(&flags, SV("foo"), 0));
    printf("bar: %ld\n", flag_as_i64(&flags, SV("bar"), 10));

    printf("baz: %f\n", flag_as_f64(&flags, SV("baz"), 0));

    printf("help: %d\n", flag_exists(&flags, SV("help")));
    printf("v: %d\n", flag_exists(&flags, SV("v")));

    printf("color: %d\n", flag_as_bool(&flags, SV("color")));
    printf("t: %d\n", flag_as_bool(&flags, SV("t")));

    return 0;
}

