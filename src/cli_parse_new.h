#ifndef MIGI_CLI_PARSE_NEW_H
#define MIGI_CLI_PARSE_NEW_H

// TODO: add support for enums

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
    CliArg_List,
} CliArgType;

typedef struct {
    CliArgType type;
    union {
        Str as_str;
        int64_t as_int;
        bool as_bool;
        double as_double;
        StrList as_list;
    };

    Str help;
    Str name;
    StrSpan aliases;
    int32_t nargs;
    bool required;
    bool found;
} CliArg;

typedef struct {
    Str arg_name;
    uint32_t arg_index;
} CliSlot;

typedef struct {
    CliArg *args;
    uint32_t args_length;

    CliSlot *slots;
    uint32_t slots_length;
    int exp;

    StrList pos_args;     // positional arguments (anything that wasnt parsed as part of the regular parsing process)
    StrList meta_args;    // arguments following a `--`, usually passed to the program being called by this program
    Str executable;
    Str help;
} Cli;

// Optional parameters common to all `cli_add_` macros
// Cli *cli;
// Arena *arena;
// Str alias;
// bool required;
//
// TODO: should this be named something else for bools?
// int32_t nargs; Used by bool and list to specify the number of arguments [default: 0 for bools and unlimited for lists]
//
// bool cli_add_str(name, help, ...);
// bool cli_add_i64(name, help, ...);
// bool cli_add_f64(name, help, ...);
// bool cli_add_bool(name, help, ...);
// bool cli_add_list(name, help, ...);


// Optional parameters for cli_parse_args
// Cli *cli;             - cli context to store options         [default: uses global cli context]
// Arena *arena;         - arena to allocate into               [default: uses global arena]
// Str help;             - help text for the program as a whole
// bool ignore_first;    - whether to ignore the first argument (usually the name of the executable) [default: to false and consumes it]
// Str executable;       - this is only used when `ignore_executable_name` is true
// int nargs_atleast;    - minimum number of positional arguments to expect
//
// bool cli_parse_args(argc, argv, ...);

static void cli_free(Cli *cli);


typedef struct {
    Cli *cli;
} CliHelpTextOpt;
#define cli_help_text(arena, ...) cli_help_text_opt((arena), (CliHelpTextOpt){ .cli = &global_cli, __VA_ARGS__ })
static Str cli_help_text_opt(Arena *arena, CliHelpTextOpt opt);

static CliArg *cli_key_to_arg(Cli *cli, Str key);

#define cli_executable() global_cli.executable

// Iterate over each flag argument
#define clic_foreach(cli, arg)                  \
    for (CliArg *arg = (cli)->args;             \
        arg < (cli)->args + (cli)->args_length; \
        arg++)                                  \

// Iterate over each positional and meta arguments
#define clic_args_foreach(cli, arg)      strlist_foreach(&(cli)->pos_args, arg)
#define clic_meta_args_foreach(cli, arg) strlist_foreach(&(cli)->meta_args, arg)

// Iteration functions for global CLI
#define cli_foreach(arg)            clic_foreach(&global_cli, arg)
#define cli_args_foreach(arg)       clic_args_foreach(&global_cli, arg)
#define cli_meta_args_foreach(arg)  clic_meta_args_foreach(&global_cli, arg)


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

static uint32_t *cli__lookup(Cli *flags, Str flag) {
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
    bool required;
    Str value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliStrOpt;

#define cli_add_str(name, help, ...)              \
    (global_cli_temp.arena == NULL                \
        ? global_cli_temp = arena_temp()          \
        : (void)0,                                \
    cli_add_str_opt((name), (help), (CliStrOpt) { \
        .cli = &global_cli,                       \
        .arena = global_cli_temp.arena,           \
        __VA_ARGS__                               \
    }))

static Str *cli_add_str_opt(Str name, Str help, CliStrOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_Str,
        .as_str = opt.value,
        .nargs = 1,
        .required = opt.required
    };
    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_str;
}

typedef struct {
    bool required;
    int64_t value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliIntOpt;

#define cli_add_i64(name, help, ...)             \
    (global_cli_temp.arena == NULL               \
        ? global_cli_temp = arena_temp()         \
        : (void)0,                               \
    cli_add_i64_opt((name), (help), (CliIntOpt){ \
        .cli = &global_cli,                      \
        .arena = global_cli_temp.arena,          \
        __VA_ARGS__                              \
    }))

static int64_t *cli_add_i64_opt(Str name, Str help, CliIntOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_Int,
        .as_int = opt.value,
        .nargs = 1,
        .required = opt.required,
    };
    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_int;
}

typedef struct {
    int32_t nargs;
    bool required;
    bool value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliBoolOpt;

#define cli_add_bool(name, help, ...)              \
    (global_cli_temp.arena == NULL                 \
        ? global_cli_temp = arena_temp()           \
        : (void)0,                                 \
    cli_add_bool_opt((name), (help), (CliBoolOpt){ \
        .cli = &global_cli,                        \
        .arena = global_cli_temp.arena,            \
        .nargs = 0,                                \
        __VA_ARGS__                                \
    }))

static bool *cli_add_bool_opt(Str name, Str help, CliBoolOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_Bool,
        .as_bool = opt.value,
        .nargs = opt.nargs,
        .required = opt.required,
    };
    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_bool;
}

typedef struct {
    bool required;
    double value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliDoubleOpt;

#define cli_add_f64(name, help, ...)                \
    (global_cli_temp.arena == NULL                  \
        ? global_cli_temp = arena_temp()            \
        : (void)0,                                  \
    cli_add_f64_opt((name), (help), (CliDoubleOpt){ \
        .cli = &global_cli,                         \
        .arena = global_cli_temp.arena,             \
        __VA_ARGS__                                 \
    }))

static double *cli_add_f64_opt(Str name, Str help, CliDoubleOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_Double,
        .as_double = opt.value,
        .nargs = 1,
        .required = opt.required,
    };
    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_double;
}

#define CLI_NARGS_INF  -1
typedef struct {
    bool required;
    int32_t nargs;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliListStrOpt;

#define cli_add_list(name, help, ...)                 \
    (global_cli_temp.arena == NULL                    \
        ? global_cli_temp = arena_temp()              \
        : (void)0,                                    \
    cli_list_str_opt((name), (help), (CliListStrOpt){ \
        .cli = &global_cli,                           \
        .arena = global_cli_temp.arena,               \
        .nargs = CLI_NARGS_INF,                       \
        __VA_ARGS__                                   \
    }))

static StrList *cli_list_str_opt(Str name, Str help, CliListStrOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_List,
        .nargs = opt.nargs,
        .required = opt.required,
    };

    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_list;
}

static bool cli__parse_value(CliArg *cli_arg, Str flag_key, Str arg) {
    bool result = false;

    Temp tmp = arena_temp();

    switch (cli_arg->type) {
        case CliArg_Str: {
            cli_arg->as_str = arg;
        } break;
        case CliArg_Int: {
            char *endptr;
            // TODO: make a custom strtoll function
            // Since arg comes from a C String, doing this should be fine as it has a NULL terminator at the end
            int64_t num = strtoll(arg.data, &endptr, 10);
            if (endptr != arg.data + arg.length) {
                migi_log(Log_Error, "expected value of type int for option: '-%.*s' but got: '%.*s'",
                        SArg(flag_key), SArg(arg));
                goto end;
            }
            cli_arg->as_int = num;
        } break;
        case CliArg_Bool: {
            Str arg_lower = str_to_lower(tmp.arena, arg);
            if (str_eq_any(arg_lower, S("1"), S("y"), S("yes"), S("true"))) {
                cli_arg->as_bool = true;

            } else if (str_eq_any(arg_lower, S("0"), S("n"), S("no"), S("false"))) {
                cli_arg->as_bool = false;
            } else {
                migi_log(Log_Error, "expected value of type bool for option: '-%.*s' "
                        "(supported values are: 1/0, y[es]/n[o], true/false) but got: '%.*s'",
                        SArg(flag_key), SArg(arg));
                goto end;
            }
        } break;
        case CliArg_Double: {
            char *endptr;
            // TODO: make a custom strtod function
            // Since arg comes from a C String, doing this should be fine as it has a NULL terminator at the end
            double num = strtod(arg.data, &endptr);
            if (endptr != arg.data + arg.length) {
                migi_log(Log_Error, "expected value of type double for option: '-%.*s' but got: '%.*s'",
                        SArg(flag_key), SArg(arg));
                goto end;
            }
            cli_arg->as_double = num;
        } break;
        default:
            migi_unreachable();
    }
    cli_arg->found = true;

    result = true;
end:
    arena_temp_release(tmp);
    return result;
}

static CliArg *cli_key_to_arg(Cli *cli, Str key) {
    uint32_t *arg_index = cli__lookup(cli, key);
    if (arg_index == NULL) {
        migi_log(Log_Error, "unknown flag: '-%.*s'", SArg(key));
        return NULL;
    }
    return &cli->args[*arg_index];
}


typedef struct {
    Cli *cli;
    Arena *arena;
    Str help;             // help text for the program as a whole
    bool ignore_first;    // whether to ignore the first argument (usually the name of the executable) [defaults to false and consumes it]
    Str executable;       // this is only used when `ignore_executable_name` is true
    int nargs_atleast;    // minimum number of positional arguments to expect // TODO: maybe split this into nargs_min and nargs_max
} CliParseOpt;

#define cli_parse_args(argc, argv, ...)                \
    (global_cli_temp.arena == NULL                     \
        ? global_cli_temp = arena_temp()               \
        : (void)0,                                     \
    cli_parse_args_opt((argc), (argv), (CliParseOpt) { \
        .cli = &global_cli,                            \
        .arena = global_cli_temp.arena,                \
        __VA_ARGS__                                    \
    }))


static bool cli_parse_args_opt(int argc, char **argv, CliParseOpt opt) {
    bool result = true;

    Temp tmp = arena_temp();

    if (opt.cli->slots == NULL) {
        cli__init(opt.arena, opt.cli);
    }

    bool handle_help_flag = false;
    // Insert help option and handle it if it wasnt provided
    if (!cli__lookup(opt.cli, S("help"))) {
        CliArg help_arg = {
            .name = S("help"),
            .aliases = str_span_new(opt.arena, S("h")),
            .help = S("show this help message"),
            .type = CliArg_Bool,
            .as_bool = false,
            .nargs = 0,
        };
        int32_t index = cli__push_arg(opt.arena, opt.cli, help_arg);
        cli__insert(opt.arena, opt.cli, S("help"), index);
        array_foreach(&help_arg.aliases, alias) {
            cli__insert(opt.arena, opt.cli, *alias, index);
        }
        handle_help_flag = true;
    }

    opt.cli->help = opt.help;

    // Parse the first arg as executable if not asked to ignore
    int i = 0;
    opt.cli->executable = (!opt.ignore_first)? str_from_cstr(argv[i++]): opt.executable;
    for (; i < argc; i++) {
        Str arg = str_from_cstr(argv[i]);
        if (arg.length == 0) continue;

        // parse as a positional argument
        if (arg.data[0] != '-') {
            strlist_push(opt.arena, &opt.cli->pos_args, arg);
            continue;
        }

        // `-` on its own is invalid, skip it
        if (arg.length == 1) continue;
        Str flag_key = str_skip(arg, 1);

        // if arg is only a `--` parse everything after it as meta arguments
        if (str_eq(arg, S("--"))) {
            i++;
            while (i < argc) {
                strlist_push_cstr(opt.arena, &opt.cli->meta_args, argv[i++]);
            }
            break;
        }

        StrCut cut = str_cut(flag_key, S("="));

        // -flag foo
        if (!cut.found) {
            CliArg *cli_arg = cli_key_to_arg(opt.cli, flag_key);
            if (!cli_arg) {
                return_with(false);
            }
            if (cli_arg->nargs == 0) {
                assertf(cli_arg->type == CliArg_Bool, "only boolean flags can have no arguments");
                cli_arg->as_bool = true;
                cli_arg->found = true;
                continue;
            }

            if (i + 1 == argc) {
                migi_log(Log_Error, "expected argument after flag: '%.*s'", SArg(flag_key));
                return_with(false);
            }

            i++;
            Str flag_value = str_from_cstr(argv[i]);

            if (cli_arg->type == CliArg_List) {
                // --list item1 ... --list item2 ... --list item3
                strlist_push(opt.arena, &cli_arg->as_list, flag_value);
            } else {
                if (!cli__parse_value(cli_arg, flag_key, flag_value)) {
                    return_with(false);
                }
            }
            continue;
        };


        flag_key = cut.head;
        Str flag_value = cut.tail;

        CliArg *cli_arg = cli_key_to_arg(opt.cli, flag_key);
        if (!cli_arg) return_with(false);

        // -flag=foo,bar,baz
        StrList items = {0};

        // TODO: `-flag="foo,bar"` is currently parsed like [`"foo`, `bar"`]
        StrCut values_cut = str_cut(flag_value, S(","));
        Str prev_tail = {0};
        do {
            if (values_cut.head.length == 0) {
                migi_log(Log_Error, "empty argument passed to list option: '-%.*s'", SArg(cli_arg->name));
                goto end;
            }
            strlist_push(opt.arena, &items, values_cut.head);
            prev_tail = values_cut.tail;
            values_cut = str_cut(values_cut.tail, S(","));
        } while (values_cut.found);

        if (prev_tail.length != 0) {
            strlist_push(opt.arena, &items, prev_tail);
        }

        if (cli_arg->type == CliArg_List) {
            // Extend list if there were previous arguments
            if (cli_arg->as_list.length == 0) {
                cli_arg->as_list = items;
                cli_arg->found = true;
            } else {
                strlist_extend(&cli_arg->as_list, &items);
            }
        } else {
            if (!cli__parse_value(cli_arg, flag_key, items.head->string)) {
                return_with(false);
            }
        }
    }

    // Validation
    clic_foreach(opt.cli, arg) {
        if (arg->type == CliArg_List && arg->nargs != CLI_NARGS_INF && (size_t)arg->nargs != arg->as_list.length) {
            migi_log(Log_Error, "too %s arguments for option: '-%.*s', expected %d but got %zu",
                    arg->as_list.length < (size_t)arg->nargs? "few": "many",
                    SArg(arg->name), arg->nargs, arg->as_list.length);

            return_with(false);
        }

        if (arg->required && !arg->found) {
            migi_log(Log_Error, "option: '-%.*s' is required but was not provided", SArg(arg->name));
            return_with(false);
        }
    }

    if (opt.cli->pos_args.length < (size_t)opt.nargs_atleast) {
        migi_log(Log_Error, "too few positional arguments, expected at least %d but got %zu",
                opt.nargs_atleast, opt.cli->pos_args.length);
        return_with(false);
    }


end:
    if (handle_help_flag) {
        uint32_t *help_index = cli__lookup(opt.cli, S("help"));
        assertf(help_index, "will never return NULL as 'help' was explicitly added");

        // Print help if there was an error or `-h` was explicitly specified
        if (!result || opt.cli->args[*help_index].as_bool) {
            fprintf(stderr, "%.*s", SArg(cli_help_text_opt(tmp.arena, (CliHelpTextOpt){ .cli = opt.cli })));
        }
    }
    arena_temp_release(tmp);
    return result;
}

static Str cli_arg_type_to_str(CliArgType type) {
    switch (type) {
        case CliArg_None:   return S("NONE");
        case CliArg_Str:    return S("string");
        case CliArg_Int:    return S("int");
        case CliArg_Bool:   return S("bool");
        case CliArg_Double: return S("float");
        case CliArg_List:   return S("list");
    }
}

// TODO: mention argument is required
// TODO: mention argument aliases in a list
// TODO: factor out the printing of options to another function, to enable easy custom help printing functions
static Str cli_help_text_opt(Arena *arena, CliHelpTextOpt opt) {
    Temp tmp = arena_temp_excl(arena);
    Cli *cli = opt.cli;
    Str help_text = {0};

    help_text = str_cat(arena, help_text, strf(tmp.arena, "usage: %.*s [OPTIONS]\n", SArg(cli->executable)));

    if (cli->help.length > 0) {
        help_text = str_cat(arena, help_text, S("\n"));
        help_text = str_cat(arena, help_text, cli->help);
        help_text = str_cat(arena, help_text, S("\n\n"));
    }

    if (cli->args_length > 0) {
        help_text = str_cat(arena, help_text, S("Options:\n"));
        clic_foreach(cli, arg) {
            // TODO: improve the alignment of options and help
            help_text = str_cat(arena, help_text, S("  "));
            array_foreach(&arg->aliases, alias) {
                help_text = str_cat(arena, help_text, strf(tmp.arena, "-%.*s, ", SArg(*alias)));
            }
            // TODO: dont print the parameter if nargs = 0
            // TODO: print the list format and expected count if nargs > 0
            help_text = str_cat(arena, help_text, strf(tmp.arena, "-%.*s %.*s [%.*s]      %.*s\n",
                        SArg(arg->name), SArg(str_to_upper(tmp.arena, arg->name)),
                        SArg(cli_arg_type_to_str(arg->type)), SArg(arg->help)));
        }
    }
    help_text = str_cat(arena, help_text, S("\n"));
    arena_temp_release(tmp);
    return help_text;
}

#endif // MIGI_CLI_PARSE_NEW_H
