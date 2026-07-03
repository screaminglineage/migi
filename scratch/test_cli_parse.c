#include <stdio.h>
#include <inttypes.h>
#include "migi.h"
#include "cli_parse.h"

void print_arg(CliArg *arg) {
    printf("%.*s => ", SArg(arg->name));
    switch (arg->type) {
        case CliArg_Str: {
            printf("'%.*s'", SArg(arg->as_str));
        } break;
        case CliArg_Int: {
            printf("%"PRId64"", arg->as_int);
        } break;
        case CliArg_Bool: {
            printf("%.*s", SArg(bool_to_str(arg->as_bool)));
        } break;
        case CliArg_Double: {
            printf("%f", arg->as_double);
        } break;
        case CliArg_List: {
            printf("[ ");
            strlist_foreach(&arg->as_list, str) {
                printf("'%.*s', ", SArg(str->string));
            }
            printf("]");
        } break;
        case CliArg_None: {
            migi_unreachable();
        } break;
    }
    printf("\n");
}


#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
#endif // ifdef _GNU_C

int main(int argc, char **argv) {
    Temp tmp = arena_temp();

    Cli cli = {.arena=tmp.arena};
    Str *str       = cli_add_str   (S("str"),  S("help: str"),                    .cli = &cli);
    int64_t *num   = cli_add_i64   (S("num"),  S("help: num"),  .required = true, .cli = &cli);
    bool *flag     = cli_add_bool  (S("flag"), S("help: flag"),                   .cli = &cli);
    double *real   = cli_add_double(S("real"), S("help: real"),                   .cli = &cli);

    // NOTE: bools can be passed an argument if `takes_arg` is true
    bool *check     = cli_add_bool(S("check"), S("help: check"), .takes_arg = true,          .cli = &cli);
    StrList *list   = cli_add_list(S("list"),  S("help: list"),  .required=true, .nargs = 3, .cli = &cli);

    if (!cli_parse_args(argc, argv, .help = S("help: prog"), .cli = &cli)) return 1;

    printf("Executable: '%.*s'\n\n", SArg(cli.executable));
    clic_foreach(&cli, arg) {
        print_arg(arg);
    }
    printf("\n");

    printf("Is Set:\n");
    printf("-str:   %.*s\n",  SArg(bool_to_str(cli_var_was_set(str))));
    printf("-num:   %.*s\n",  SArg(bool_to_str(cli_var_was_set(num))));
    printf("-flag:  %.*s\n",  SArg(bool_to_str(cli_var_was_set(flag))));
    printf("-real:  %.*s\n",  SArg(bool_to_str(cli_var_was_set(real))));
    printf("-list:  %.*s\n",  SArg(bool_to_str(cli_var_was_set(list))));
    printf("-check: %.*s\n",  SArg(bool_to_str(cli_var_was_set(check))));
    printf("\n\n");

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

    Str *str       = cli_add_str   (S("str"),  S("help text for str"),  .value = S("foo"), .aliases = str_span(S("s")));
    int64_t *num   = cli_add_i64   (S("num"),  S("help text for num"),  .value = 25,       .aliases = str_span(S("n")));
    bool *flag     = cli_add_bool  (S("flag"), S("help text for flag"), .value = true,     .aliases = str_span(S("f")));
    double *real   = cli_add_double(S("real"), S("help text for real"), .value = 3.1415,   .aliases = str_span(S("r")));

    bool *help    = cli_add_bool(S("help"), S("help text for help"), .aliases = str_span(S("h"), S("?")));
    if (!cli_parse_args(argc, argv, .help = S("Does some stuff")) || *help) {
        fprintf(stderr, "%.*s", SArg(cli_help_text(tmp.arena)));
        return 1;
    }

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


#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

