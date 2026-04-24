#include <stdio.h>
#include "migi.h"
#include "cli_parse_new.h"

void print_arg(CliArg *arg) {
    printf("%.*s => ", SArg(arg->name));
    switch (arg->type) {
        case CliArg_Str: {
            printf("'%.*s'\n", SArg(arg->as_str));
        } break;
        case CliArg_Int: {
            printf("%ld\n", arg->as_int);
        } break;
        case CliArg_Bool: {
            printf("%.*s\n", SArg(bool_to_str(arg->as_bool)));
        } break;
        case CliArg_Double: {
            printf("%f\n", arg->as_double);
        } break;
        case CliArg_List: {
            printf("[ ");
            strlist_foreach(&arg->as_list, str) {
                printf("%.*s, ", SArg(str->string));
            }
            printf("]\n");
        } break;
        case CliArg_None: {
            migi_unreachable();
        } break;
    }
}

int main(int argc, char **argv) {
    Temp tmp = arena_temp();

    // NOTE: the arenas in the parse_args and cli_* functions may or may not be provided
    // They can even be separate arenas if required
    Cli cli = {0};
    Str *str       = cli_add_str (S("str"),  S("help: str"),                    .ctx = &cli, /*.arena = tmp.arena*/);
    int64_t *num   = cli_add_i64 (S("num"),  S("help: num"),  .required = true, .ctx = &cli, /*.arena = tmp.arena*/);
    bool *flag     = cli_add_bool(S("flag"), S("help: flag"),                   .ctx = &cli, /*.arena = tmp.arena*/);
    double *real   = cli_add_f64 (S("real"), S("help: real"),                   .ctx = &cli, /*.arena = tmp.arena*/);

    // NOTE: bools take no args by default which can be changed to 1 through the `nargs` argument
    // The supported values are: `1`, `0`, `true`, `false` (case doesnt matter)
    bool *check     = cli_add_bool(S("check"), S("help: check"), .nargs = 1, .ctx = &cli, /*.arena = tmp.arena*/);
    StrList *list   = cli_add_list(S("list"),  S("help: list"),  .nargs = 3, .ctx = &cli, /*.arena = tmp.arena*/);

    if (!cli_parse_args(argc, argv, .help = S("help: prog"), .nargs_atleast = 2, .ctx = &cli, /* .arena = tmp.arena */)) return 1;

    printf("Executable: '%.*s'\n\n", SArg(cli.executable));
    clic_foreach(&cli, arg) {
        print_arg(arg);
    }
    printf("\n");

    printf("Positional Arguments: \n");
    clic_args_foreach(&cli, arg) {
        printf("'%.*s'\n", SArg(arg->string));
    }
    printf("\n");

    printf("Meta Arguments: \n");
    clic_meta_args_foreach(&cli, arg) {
        printf("'%.*s'\n", SArg(arg->string));
    }
    printf("\n");

    arena_temp_release(tmp);
    return 0;
}

int main1(int argc, char **argv) {
    Temp tmp = arena_temp();

    Str *str       = cli_add_str (S("str"),  S("help text for str"),  .value = S("foo"), .alias = S("s"));
    int64_t *num   = cli_add_i64 (S("num"),  S("help text for num"),  .value = 25,       .alias = S("n"));
    bool *flag     = cli_add_bool(S("flag"), S("help text for flag"), .value = false,    .alias = S("f"));
    double *real   = cli_add_f64 (S("real"), S("help text for real"), .value = 3.1415,   .alias = S("r"));

    // bool *help    = cli_add_bool(S("help"), S("help text for help"));
    if (!cli_parse_args(argc, argv, .help = S("Does some stuff"))) return 1;

    printf("Executable: '%.*s'\n\n", SArg(cli_executable()));
    cli_foreach(arg) {
        print_arg(arg);
    }
    printf("\n");

    printf("Positional Arguments: \n");
    cli_args_foreach(arg) {
        printf("'%.*s'\n", SArg(arg->string));
    }
    printf("\n");

    printf("Meta Arguments: \n");
    cli_meta_args_foreach(arg) {
        printf("'%.*s'\n", SArg(arg->string));
    }
    printf("\n");

    arena_temp_release(tmp);
    return 0;
}
