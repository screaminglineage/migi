#include <stdio.h>
#include "migi.h"

// Maximum number of flags supported
// Can be changed by #defining the constant before including the header
#ifndef CLI_MAX_FLAGS
    #define CLI_MAX_FLAGS 256
#endif

typedef enum {
    CliArg_None,
    CliArg_Str,
    CliArg_Int,
    CliArg_Bool,
    CliArg_Double,
} CliArgType;

typedef struct {
    CliArgType type;
    union {
        Str as_str;
        int64_t as_int;
        bool as_bool;
        double as_double;
    };
    Str help;
    Str name;
    Str short_opt;
} CliArg;

typedef struct {
    Str arg_name;
    int32_t arg_index;
} CliSlot;

typedef struct {
    CliArg *args;
    uint32_t args_length;

    CliSlot *slots;
    uint32_t slots_length;
    int exp;

    StrList meta_args;    // arguments following a `--`, usually passed to the program being called by this program
    Str executable;
    Str help;
} Cli;

threadvar Cli global_cli = {0};
threadvar Temp global_cli_temp = {0};

static void cli__init(Arena *arena, Cli *cli) {
    cli->exp = 3;
    for (; (1 << cli->exp) - (1 << (cli->exp - 3)) < CLI_MAX_FLAGS; cli->exp++) {}
    cli->slots = arena_push(arena, CliSlot, 1LL << cli->exp);
    cli->args = arena_push(arena, CliArg, CLI_MAX_FLAGS);
}

// Doesnt actually free the memory as that is stored on an arena
static void cli_free(Cli *cli) {
    mem_clear(cli);
}

static int32_t cli__iter_step(uint64_t hash, int exp, int32_t index) {
    uint32_t mask = ((uint32_t)1 << exp) - 1;
    uint32_t step = (uint32_t)(hash >> (64 - exp)) | 1;
    return (index + step) & mask;
}

static void cli__insert(Arena *arena, Cli *cli, Str key, uint32_t value) {
    if (cli->slots == NULL) {
        cli__init(arena, cli);
    }
    assertf(cli->slots_length + 1 < (uint32_t)(1 << cli->exp), "cli__insert: flag table capacity exceeded!");

    uint64_t hash = str_hash(key);
    for (uint32_t i = (uint32_t)hash;;) {
        i = cli__iter_step(hash, cli->exp, i);
        if (cli->slots[i].arg_name.length == 0) {
            cli->slots[i] = (CliSlot){
                .arg_name = key,
                .arg_index = value
            };
            cli->slots_length += 1;
            break;
        }
    }
}

static int32_t cli__push_arg(Arena *arena, Cli *cli, CliArg arg) {
    if (cli->args == NULL) {
        cli__init(arena, cli);
    }

    assertf(cli->slots_length < CLI_MAX_FLAGS, "cli__push_arg: args array capacity exceeded!");
    cli->args[cli->args_length++] = arg;
    return cli->args_length - 1;
}

static int32_t *cli__lookup(Cli *flags, Str flag) {
    uint64_t hash = str_hash(flag);
    for (uint32_t i = (uint32_t)hash;;) {
        i = cli__iter_step(hash, flags->exp, i);
        if (flags->slots[i].arg_name.length == 0) {
            return NULL;
        }
        if (str_eq(flag, flags->slots[i].arg_name)) {
            return &flags->slots[i].arg_index;
        }
    }
}


typedef struct {
    Str default_val;
    Str short_opt;
    Cli *ctx;
    Arena *arena;
} CliStrOpt;

#define cli_str(name, help, ...)              \
    (global_cli_temp.arena == NULL            \
        ? global_cli_temp = arena_temp()      \
        : (void)0,                            \
    cli_str_opt((name), (help), (CliStrOpt) { \
        .ctx = &global_cli,                   \
        .arena = global_cli_temp.arena,       \
        __VA_ARGS__                           \
    }))

static Str *cli_str_opt(Str name, Str help, CliStrOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .short_opt = opt.short_opt,
        .type = CliArg_Str,
        .as_str = opt.default_val
    };
    int32_t index = cli__push_arg(opt.arena, opt.ctx, arg);
    cli__insert(opt.arena, opt.ctx, name, index);
    cli__insert(opt.arena, opt.ctx, opt.short_opt, index);

    return &opt.ctx->args[index].as_str;
}

typedef struct {
    int64_t default_val;
    Str short_opt;
    Cli *ctx;
    Arena *arena;
} CliIntOpt;

#define cli_int(name, help, ...)             \
    (global_cli_temp.arena == NULL           \
        ? global_cli_temp = arena_temp()     \
        : (void)0,                           \
    cli_int_opt((name), (help), (CliIntOpt){ \
        .ctx = &global_cli,                  \
        .arena = global_cli_temp.arena,      \
        __VA_ARGS__                          \
    }))

static int64_t *cli_int_opt(Str name, Str help, CliIntOpt opt) {
    // if (global_cli_temp.arena == NULL) global_cli_temp = arena_temp();

    CliArg arg = {
        .name = name,
        .help = help,
        .short_opt = opt.short_opt,
        .type = CliArg_Int,
        .as_int = opt.default_val
    };
    int32_t index = cli__push_arg(opt.arena, opt.ctx, arg);
    cli__insert(opt.arena, opt.ctx, name, index);
    cli__insert(opt.arena, opt.ctx, opt.short_opt, index);

    return &opt.ctx->args[index].as_int;
}

typedef struct {
    bool default_val;
    Str short_opt;
    Cli *ctx;
    Arena *arena;
} CliBoolOpt;

#define cli_bool(name, help, ...)              \
    (global_cli_temp.arena == NULL             \
        ? global_cli_temp = arena_temp()       \
        : (void)0,                             \
    cli_bool_opt((name), (help), (CliBoolOpt){ \
        .ctx = &global_cli,                    \
        .arena = global_cli_temp.arena,        \
        __VA_ARGS__                            \
    }))


static bool *cli_bool_opt(Str name, Str help, CliBoolOpt opt) {
    // if (global_cli_temp.arena == NULL) global_cli_temp = arena_temp();

    CliArg arg = {
        .name = name,
        .help = help,
        .short_opt = opt.short_opt,
        .type = CliArg_Bool,
        .as_bool = opt.default_val
    };
    int32_t index = cli__push_arg(opt.arena, opt.ctx, arg);
    cli__insert(opt.arena, opt.ctx, name, index);
    cli__insert(opt.arena, opt.ctx, opt.short_opt, index);

    return &opt.ctx->args[index].as_bool;
}

typedef struct {
    double default_val;
    Str short_opt;
    Cli *ctx;
    Arena *arena;
} CliDoubleOpt;

#define cli_double(name, help, ...)                \
    (global_cli_temp.arena == NULL                 \
        ? global_cli_temp = arena_temp()           \
        : (void)0,                                 \
    cli_double_opt((name), (help), (CliDoubleOpt){ \
        .ctx = &global_cli,                        \
        .arena = global_cli_temp.arena,            \
        __VA_ARGS__                                \
    }))

static double *cli_double_opt(Str name, Str help, CliDoubleOpt opt) {
    // if (global_cli_temp.arena == NULL) global_cli_temp = arena_temp();

    CliArg arg = {
        .name = name,
        .help = help,
        .short_opt = opt.short_opt,
        .type = CliArg_Double,
        .as_double = opt.default_val
    };
    int32_t index = cli__push_arg(opt.arena, opt.ctx, arg);
    cli__insert(opt.arena, opt.ctx, name, index);
    cli__insert(opt.arena, opt.ctx, opt.short_opt, index);

    return &opt.ctx->args[index].as_double;
}

static Str cli_help_text(Arena *arena, Cli *cli);

typedef struct {
    Cli *ctx;
    Arena *arena;
    Str help;       // help text for the program as a whole
} CliParseOpt;

#define cli_parse_args(argc, argv, ...)                \
    (global_cli_temp.arena == NULL                     \
        ? global_cli_temp = arena_temp()               \
        : (void)0,                                     \
    cli_parse_args_opt((argc), (argv), (CliParseOpt) { \
        .ctx = &global_cli,                            \
        .arena = global_cli_temp.arena,                \
        __VA_ARGS__                                    \
    }))

static bool cli_parse_args_opt(int argc, char **argv, CliParseOpt opt) {
    Temp tmp = arena_temp();

    if (opt.ctx->slots == NULL) {
        cli__init(opt.arena, opt.ctx);
    }

    bool handle_help_flag = false;
    // Insert help option and handle it if it wasnt provided
    if (!cli__lookup(opt.ctx, S("help"))) {
        CliArg help_arg = {
            .name = S("help"),
            .help = S("show this help message"),
            .short_opt = S("h"),
            .type = CliArg_Bool,
            .as_bool = false
        };
        int32_t index = cli__push_arg(opt.arena, opt.ctx, help_arg);
        cli__insert(opt.arena, opt.ctx, S("help"), index);
        cli__insert(opt.arena, opt.ctx, S("h"), index);
        handle_help_flag = true;
    }

    opt.ctx->executable = str_from_cstr(argv[0]);
    opt.ctx->help = opt.help;

    Str flag_key = {0};
    bool help_flag_found = false;

    for (int i = 1; i < argc; i++) {
        Str arg = str_from_cstr(argv[i]);
        if (arg.length == 0) continue;

        // parse as a positional argument or option to an already parsed flag key
        if (arg.data[0] != '-') {
            assertf(flag_key.length != 0, "positional arguments are not yet supported");

            int32_t *arg_index = cli__lookup(opt.ctx, flag_key);
            if (arg_index == NULL) {
                migi_log(Log_Error, "unknown flag: '%.*s'", SArg(flag_key));
                arena_temp_release(tmp);
                return false;
            }

            CliArg *cli_arg = &opt.ctx->args[*arg_index];
            // TODO: add support for flags taking a list of values
            // TODO: add support for flags taking no values
            switch (cli_arg->type) {
                case CliArg_Str: {
                    cli_arg->as_str = arg;
                } break;
                case CliArg_Int: {
                    char *endptr;
                    int64_t num = strtoll(argv[i], &endptr, 10);
                    if (endptr != arg.data + arg.length) {
                        migi_log(Log_Error, "invalid value: '%.*s' for argument of type int"
                                "(argument was for option: '%.*s')", SArg(arg), SArg(flag_key));
                        arena_temp_release(tmp);
                        return false;
                    }
                    cli_arg->as_int = num;
                } break;
                case CliArg_Bool: {
                    Str arg_lower = str_to_lower(tmp.arena, arg);
                    if (str_eq(arg_lower, S("1")) || str_eq(arg_lower, S("true"))) {
                        cli_arg->as_bool = true;
                    } else if (str_eq(arg_lower, S("1")) || str_eq(arg_lower, S("true"))) {
                        cli_arg->as_bool = false;
                    } else {
                        migi_log(Log_Error, "invalid value: '%.*s' for argument of type bool"
                                "(argument was for option: '%.*s')", SArg(arg), SArg(flag_key));
                        arena_temp_release(tmp);
                        return false;
                    }
                } break;
                case CliArg_Double: {
                    char *endptr;
                    double num = strtod(argv[i], &endptr);
                    if (endptr != arg.data + arg.length) {
                        migi_log(Log_Error, "invalid value: '%.*s' for argument of type double"
                                "(argument was for option: '%.*s')", SArg(arg), SArg(flag_key));
                        arena_temp_release(tmp);
                        return false;
                    }
                    cli_arg->as_double = num;
                } break;
                default:
                    migi_unreachable();
            }
            flag_key.length = 0;
            continue;
        }

        // `-` on its own is invalid, skip it
        if (arg.length == 1) continue;

        if (arg.data[1] != '-') {
            // -flag
            flag_key = str_skip(arg, 1);
        } else {
            // --flag
            flag_key = str_skip(arg, 2);

            // if arg is only a `--` parse everything after it as meta arguments
            if (flag_key.length == 0) {
                while (i < argc) {
                    i++;
                    strlist_push_cstr(opt.arena, &opt.ctx->meta_args, argv[i++]);
                }
                break;
            }
        }

        // Dont parse argument for "help" if handling it
        if (handle_help_flag && (str_eq(flag_key, S("help")) || str_eq(flag_key, S("h")))) {
            help_flag_found = true;
            flag_key.length = 0;
        }

        // StrCut cut = str_cut(flag_key, S("="));
        // if (!cut.found) {
        //     // insert as key with no value (eg: `-h`/`--help`)
        //     cli__insert(&global_cli, flag_key, S(""), (StrList){0});
        //     continue;
        // };

        // flag_key = cut.head;
        // Str flag_value = cut.tail;
        //
        // // --flag=foo,bar,baz
        // StrList values = {0};
        // StrCut values_cut = str_cut(flag_value, S(","));
        // Str prev_tail = {0};
        // while (values_cut.found) {
        //     strlist_push(arena, &values, values_cut.head);
        //     prev_tail = values_cut.tail;
        //     values_cut = str_cut(values_cut.tail, S(","));
        // }
        // if (prev_tail.length != 0) {
        //     strlist_push(arena, &values, prev_tail);
        // }

        // cli__insert(&global_cli, flag_key, flag_value, values);
    }

    if (help_flag_found) {
        fprintf(stderr, "%.*s", SArg(cli_help_text(tmp.arena, opt.ctx)));
    }

    arena_temp_release(tmp);
    return true;
}

// Iterate over each flag argument
#define flag_foreach(flags, arg)                    \
    for (CliArg *(arg) = (flags)->args;             \
        arg < (flags)->args + (flags)->args_length; \
        arg++)

static Str cli_help_text(Arena *arena, Cli *cli) {
    Temp tmp = arena_temp_excl(arena);
    Str help_text = {0};

    help_text = str_cat(arena, help_text, stringf(tmp.arena, "usage: %.*s [OPTIONS]\n", SArg(cli->executable)));

    if (cli->help.length > 0) {
        help_text = str_cat(arena, help_text, S("\n"));
        help_text = str_cat(arena, help_text, cli->help);
        help_text = str_cat(arena, help_text, S("\n\n"));
    }

    if (cli->args_length > 0) {
        help_text = str_cat(arena, help_text, S("Options:\n"));
        flag_foreach(cli, arg) {
            // TODO: improve the alignment of options and help
            help_text = str_cat(arena, help_text, S("  "));
            if (arg->short_opt.length != 0) {
                help_text = str_cat(arena, help_text, stringf(tmp.arena, "-%.*s, ", SArg(arg->short_opt)));
            }
            help_text = str_cat(arena, help_text, stringf(tmp.arena, "--%.*s      %.*s\n", SArg(arg->name), SArg(arg->help)));
        }
    }
    help_text = str_cat(arena, help_text, S("\n"));
    arena_temp_release(tmp);
    return help_text;
}

int main(int argc, char **argv) {
    Temp tmp = arena_temp();

    // NOTE: the arenas in the parse_args and cli_* functions may or may not be provided
    // They can even be separate arenas if required
    Cli cli = {0};
    Str *str        = cli_str   (S("str"),  S("help text for str"),  .ctx = &cli, /*.arena = tmp.arena*/);
    int64_t *num    = cli_int   (S("num"),  S("help text for num"),  .ctx = &cli, /*.arena = tmp.arena*/);
    bool *flag      = cli_bool  (S("flag"), S("help text for flag"), .ctx = &cli, /*.arena = tmp.arena*/);
    double *real    = cli_double(S("real"), S("help text for real"), .ctx = &cli, /*.arena = tmp.arena*/);
    if (!cli_parse_args(argc, argv, .help = S("Does some stuff"), .ctx = &cli, /* .arena = tmp.arena */)) return 1;

    printf("'%.*s'\n", SArg(*str));

    arena_temp_release(tmp);
    return 0;
}

int main1(int argc, char **argv) {
    Temp tmp = arena_temp();

    Str *str        = cli_str   (S("str"),  S("help text for str"),  .default_val = S("foo"), .short_opt = S("s"));
    int64_t *num    = cli_int   (S("num"),  S("help text for num"),  .default_val = 25,       .short_opt = S("n"));
    bool *flag      = cli_bool  (S("flag"), S("help text for flag"), .default_val = false,    .short_opt = S("f"));
    double *real    = cli_double(S("real"), S("help text for real"), .default_val = 3.1415,   .short_opt = S("r"));

    // bool *help    = cli_bool(S("help"), S("help text for help"));
    if (!cli_parse_args(argc, argv, .help = S("Does some stuff"))) return 1;

    printf("'%.*s'\n", SArg(*str));

    arena_temp_release(tmp);
    return 0;
}
