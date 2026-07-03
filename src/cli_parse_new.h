#ifndef MIGI_CLI_PARSE_NEW_H
#define MIGI_CLI_PARSE_NEW_H

#include "migi_core.h"
#include "migi_string.h"
#include "migi_list.h"

// Maximum number of options supported
// Can be changed by #defining the constant before including the header
#ifndef CLI_MAX_OPTIONS
    #define CLI_MAX_OPTIONS 256
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
    Str executable;       // executable name (argv[0] by default but can be customized)
    Str help;
} Cli;


typedef struct {
    bool required;         // argument must be provided if set
    Str value;             // default value of argument
    StrSpan aliases;       // aliases that map to the same option
    Cli *cli;              // cli context to store options         [default: uses global cli context]
    Arena *arena;          // arena to allocate into               [default: uses global arena]
} CliStrOpt;
// Str *cli_add_str(name, help, ...)
static Str *cli_add_str_opt(Str name, Str help, CliStrOpt opt);

typedef struct {
    bool required;
    int64_t value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliIntOpt;
// int64_t *cli_add_int(name, help, ...)
static int64_t *cli_add_i64_opt(Str name, Str help, CliIntOpt opt);

typedef struct {
    bool takes_arg;   // whether to take an argument, supported arguments: `1`, `0`, `y[es]`, `n[o]` `true`, `false` (case insensitive)
    bool required;
    bool value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliBoolOpt;
// bool *cli_add_bool(name, help, ...)
static bool *cli_add_bool_opt(Str name, Str help, CliBoolOpt opt);

typedef struct {
    bool required;
    double value;
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliDoubleOpt;
// double *cli_add_double(name, help, ...)
static double *cli_add_double_opt(Str name, Str help, CliDoubleOpt opt);

#define CLI_NARGS_INF -1
typedef struct {
    bool required;     // if set then checks that exactly `nargs` arguments are provided, otherwise
                       // less than `nargs` arguments are allowed, but not any more [default: false]

    int32_t nargs;     // no. of arguments to take [default: -1 or `CLI_NARGS_INF` for no limit]
    StrSpan aliases;
    Cli *cli;
    Arena *arena;
} CliListStrOpt;
// StrList *cli_add_list(name, help, ...)
static StrList *cli_list_str_opt(Str name, Str help, CliListStrOpt opt);


typedef struct {
    Cli *cli;             // cli context to store options         [default: uses global cli context]
    Arena *arena;         // arena to allocate into               [default: uses global arena]
    Str help;             // help text for the program as a whole
    bool ignore_first;    // whether to ignore the first argument (usually the name of the executable) [defaults to false and consumes it]
    Str executable;       // this is only used when `ignore_first` is true to set the executable name manually
} CliParseOpt;
// bool cli_parse_args(argc, argv, ...);
static bool cli_parse_args_opt(int argc, char **argv, CliParseOpt opt);

static void cli_free();             // Free the global CLI context
static void clic_free(Cli *cli);

typedef struct {
    Cli *cli;
} CliOpt;

// Get the help text for the provided options
#define cli_help_text(arena, ...) cli_help_text_opt((arena), (CliOpt){ .cli = &global_cli, __VA_ARGS__ })
static Str cli_help_text_opt(Arena *arena, CliOpt opt);

// Get a formatted string of the list of all cli options (useful if making a custom help function)
#define cli_options_list(arena, ...) cli_options_list_opt((arena), (CliOpt){ .cli = &global_cli, __VA_ARGS__ })
static Str cli_options_list_opt(Arena *arena, CliOpt opt);

// Get the type of a CliArg as a Str
static Str cli_arg_type_to_str(CliArgType type);

// Lower level function to directly access the options table by name
static CliArg *cli_arg_by_name(Cli *cli, Str name);

#define cli_arg_from_var(var)                            \
    _Generic((var),                                      \
        Str *:      parent_of(CliArg, as_str,    (var)), \
        int64_t *:  parent_of(CliArg, as_int,    (var)), \
        bool *:     parent_of(CliArg, as_bool,   (var)), \
        double *:   parent_of(CliArg, as_double, (var)), \
        StrList *:  parent_of(CliArg, as_list,   (var))  \
    )


// Get global cli fields
#define cli_executable() global_cli.executable
#define cli_meta_args()  global_cli.meta_args
#define cli_pos_args()  global_cli.pos_args

// Iterate over each option
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
threadvar Arena *global_cli_arena = {0};

#define cli_parse_args(argc, argv, ...)                \
    (global_cli_arena == NULL                          \
        ? global_cli_arena = arena_init(), (void)0     \
        : (void)0,                                     \
    cli_parse_args_opt((argc), (argv), (CliParseOpt) { \
        .cli = &global_cli,                            \
        .arena = global_cli_arena,                     \
        __VA_ARGS__                                    \
    }))


#define cli_add_str(name, help, ...)               \
    (global_cli_arena == NULL                      \
        ? global_cli_arena = arena_init(), (void)0 \
        : (void)0,                                 \
    cli_add_str_opt((name), (help), (CliStrOpt) {  \
        .cli = &global_cli,                        \
        .arena = global_cli_arena,                 \
        __VA_ARGS__                                \
    }))

#define cli_add_i64(name, help, ...)               \
    (global_cli_arena == NULL                      \
        ? global_cli_arena = arena_init(), (void)0 \
        : (void)0,                                 \
    cli_add_i64_opt((name), (help), (CliIntOpt){   \
        .cli = &global_cli,                        \
        .arena = global_cli_arena,                 \
        __VA_ARGS__                                \
    }))

#define cli_add_bool(name, help, ...)              \
    (global_cli_arena == NULL                      \
        ? global_cli_arena = arena_init(), (void)0 \
        : (void)0,                                 \
    cli_add_bool_opt((name), (help), (CliBoolOpt){ \
        .cli = &global_cli,                        \
        .arena = global_cli_arena,                 \
        __VA_ARGS__                                \
    }))

#define cli_add_double(name, help, ...)                \
    (global_cli_arena == NULL                          \
        ? global_cli_arena = arena_init(), (void)0     \
        : (void)0,                                     \
    cli_add_double_opt((name), (help), (CliDoubleOpt){ \
        .cli = &global_cli,                            \
        .arena = global_cli_arena,                     \
        __VA_ARGS__                                    \
    }))

#define cli_add_list(name, help, ...)                 \
    (global_cli_arena == NULL                         \
        ? global_cli_arena = arena_init(), (void)0    \
        : (void)0,                                    \
    cli_list_str_opt((name), (help), (CliListStrOpt){ \
        .cli = &global_cli,                           \
        .arena = global_cli_arena,                    \
        .nargs = CLI_NARGS_INF,                       \
        __VA_ARGS__                                   \
    }))


static void cli__init(Arena *arena, Cli *cli) {
    cli->exp = 3;
    for (; (1 << cli->exp) - (1 << (cli->exp - 3)) < CLI_MAX_OPTIONS; cli->exp++) {}
    cli->slots = arena_push(arena, CliSlot, 1LL << cli->exp);
    cli->args = arena_push(arena, CliArg, CLI_MAX_OPTIONS);
}


static void cli_free() {
    mem_clear(&global_cli);
    arena_free(global_cli_arena);
}

// Doesnt actually free the memory as that is stored on an arena
static void clic_free(Cli *cli) {
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
    assertf(cli->slots_length + 1 < (uint32_t)(1 << cli->exp), "cli__insert: option table capacity exceeded!");

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

    assertf(cli->slots_length < CLI_MAX_OPTIONS, "cli__push_arg: args array capacity exceeded!");
    cli->args[cli->args_length++] = arg;
    return cli->args_length - 1;
}

static uint32_t *cli__lookup(Cli *cli, Str key) {
    uint64_t hash = str_hash(key);
    for (uint32_t i = (uint32_t)hash;;) {
        i = cli__iter_step(hash, cli->exp, i);
        if (cli->slots[i].arg_name.length == 0) {
            return NULL;
        }
        if (str_eq(key, cli->slots[i].arg_name)) {
            return &cli->slots[i].arg_index;
        }
    }
}


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
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_str;
}

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
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_int;
}

static bool *cli_add_bool_opt(Str name, Str help, CliBoolOpt opt) {
    CliArg arg = {
        .name = name,
        .help = help,
        .aliases = opt.aliases,
        .type = CliArg_Bool,
        .as_bool = opt.value,
        .nargs = opt.takes_arg,
        .required = opt.required,
    };
    int32_t index = cli__push_arg(opt.arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_bool;
}

static double *cli_add_double_opt(Str name, Str help, CliDoubleOpt opt) {
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
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_double;
}

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
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_list;
}

static bool cli__parse_value(CliArg *cli_arg, Str key, Str value) {
    bool result = false;

    Temp tmp = arena_temp();

    switch (cli_arg->type) {
        case CliArg_Str: {
            cli_arg->as_str = value;
        } break;
        case CliArg_Int: {
            char *endptr;
            // TODO: make a custom strtoll function
            // Since arg comes from a C String, doing this should be fine as it has a NULL terminator at the end
            int64_t num = strtoll(value.data, &endptr, 10);
            if (endptr != value.data + value.length) {
                migi_log(Log_Error, "expected value of type int for option: '-%.*s' but got: '%.*s'",
                        SArg(key), SArg(value));
                goto end;
            }
            cli_arg->as_int = num;
        } break;
        case CliArg_Bool: {
            Str arg_lower = str_to_lower(tmp.arena, value);
            if (str_eq_any(arg_lower, S("1"), S("y"), S("yes"), S("true"))) {
                cli_arg->as_bool = true;

            } else if (str_eq_any(arg_lower, S("0"), S("n"), S("no"), S("false"))) {
                cli_arg->as_bool = false;
            } else {
                migi_log(Log_Error, "expected value of type bool for option: '-%.*s' "
                        "(supported values are: 1/0, y[es]/n[o], true/false) but got: '%.*s'",
                        SArg(key), SArg(value));
                goto end;
            }
        } break;
        case CliArg_Double: {
            char *endptr;
            // TODO: make a custom strtod function
            // Since arg comes from a C String, doing this should be fine as it has a NULL terminator at the end
            double num = strtod(value.data, &endptr);
            if (endptr != value.data + value.length) {
                migi_log(Log_Error, "expected value of type double for option: '-%.*s' but got: '%.*s'",
                        SArg(key), SArg(value));
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

static CliArg *cli_arg_by_name(Cli *cli, Str name) {
    uint32_t *arg_index = cli__lookup(cli, name);
    if (arg_index == NULL) {
        migi_log(Log_Error, "unknown option: '-%.*s'", SArg(name));
        return NULL;
    }
    return &cli->args[*arg_index];
}

static bool cli__validate_args(Cli *cli) {
    // Validation for required arguments
    clic_foreach(cli, arg) {
        if (arg->type == CliArg_List) {
            if (arg->required && arg->nargs != CLI_NARGS_INF && (size_t)arg->nargs != arg->as_list.length) {
                migi_log(Log_Error, "too %s arguments for option: '-%.*s', expected %d but got %zu",
                        arg->as_list.length < (size_t)arg->nargs? "few": "many",
                        SArg(arg->name), arg->nargs, arg->as_list.length);
                return false ;
            } else if ((size_t)arg->nargs < arg->as_list.length) {
                migi_log(Log_Error, "too many arguments for option: '-%.*s', expected %d but got %zu",
                        SArg(arg->name), arg->nargs, arg->as_list.length);
                return false ;
            }
        } else if (arg->required && !arg->found) {
            migi_log(Log_Error, "option: '-%.*s' is required but was not provided", SArg(arg->name));
            return false ;
        }
    }

    return true;
}

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
        Str key = str_skip(arg, 1);

        // if arg is only a `--` parse everything after it as meta arguments
        if (str_eq(arg, S("--"))) {
            i++;
            while (i < argc) {
                strlist_push_cstr(opt.arena, &opt.cli->meta_args, argv[i++]);
            }
            break;
        }

        StrCut cut = str_cut(key, S("="));

        // -opt foo
        if (!cut.found) {
            CliArg *cli_arg = cli_arg_by_name(opt.cli, key);
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
                migi_log(Log_Error, "expected argument after option: '%.*s'", SArg(key));
                return_with(false);
            }

            i++;
            Str value = str_from_cstr(argv[i]);

            if (cli_arg->type == CliArg_List) {
                // --list item1 ... --list item2 ... --list item3
                strlist_push(opt.arena, &cli_arg->as_list, value);
            } else {
                if (!cli__parse_value(cli_arg, key, value)) {
                    return_with(false);
                }
            }
            continue;
        };


        key = cut.head;
        Str value = cut.tail;

        CliArg *cli_arg = cli_arg_by_name(opt.cli, key);
        if (!cli_arg) return_with(false);

        // -opt=foo,bar,baz
        StrList items = {0};

        // NOTE: `-opt="foo,bar"` is currently parsed like [`foo`, `bar`]
        // However since quotes are handled by the shell, by the time the program
        // gets access to the `argv` array, they have already been removed.
        // This makes it impossible to check for this case.
        StrCut values_cut = str_cut(value, S(","));
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
            if (!cli__parse_value(cli_arg, key, items.head->string)) {
                return_with(false);
            }
        }
    }

    if (!cli__validate_args(opt.cli)) return_with(false);

end:
    if (handle_help_flag) {
        uint32_t *help_index = cli__lookup(opt.cli, S("help"));
        assertf(help_index, "will never return NULL as 'help' was explicitly added");

        // Print help if there was an error or `-h` was explicitly specified
        if (!result || opt.cli->args[*help_index].as_bool) {
            fprintf(stderr, "%.*s", SArg(cli_help_text_opt(tmp.arena, (CliOpt){ .cli = opt.cli })));
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
    migi_unreachable();
}

static Str cli_options_list_opt(Arena *arena, CliOpt opt) {
    Temp tmp = arena_temp_excl(arena);
    StrList options = {0};
    size_t max_option_length = 0;
    clic_foreach(opt.cli, arg) {
        Str options_str = {0};
        array_foreach(&arg->aliases, alias) {
            options_str = str_catf(tmp.arena, options_str, "-%.*s, ", SArg(*alias));
        }

        if (arg->nargs == 0) {
            options_str = str_catf(tmp.arena, options_str, "-%.*s", SArg(arg->name));
        } else if (arg->nargs == 1) {
            options_str = str_catf(tmp.arena, options_str, "-%.*s <%.*s>",
                    SArg(arg->name), SArg(cli_arg_type_to_str(arg->type)));
        } else if (arg->nargs > 1) {
            options_str = str_catf(tmp.arena, options_str, "-%.*s <%.*s[%d]>",
                    SArg(arg->name), SArg(cli_arg_type_to_str(arg->type)), arg->nargs);
        }

        if (arg->required) {
            options_str = str_cat(tmp.arena, options_str, S(" (required)"));
        }

        max_option_length = max_of(options_str.length, max_option_length);
        strlist_push(tmp.arena, &options, options_str);
    }

    size_t i = 0;
    Str options_list = {0};
    strlist_foreach(&options, option) {
        CliArg arg = opt.cli->args[i++];
        options_list = str_cat(arena, options_list, S("  "));
        options_list = str_cat(arena, options_list, option->string);

        int min_space_count = 5;
        size_t space_count = min_space_count + max_option_length - option->string.length;
        options_list = str_catf(arena, options_list, "%*.s%.*s\n", (int)space_count, " ", SArg(arg.help));
    }
    arena_temp_release(tmp);

    return options_list;
}

static Str cli_help_text_opt(Arena *arena, CliOpt opt) {
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
        help_text.length += cli_options_list_opt(arena, opt).length;
    }

    help_text = str_cat(arena, help_text, S("\n"));
    arena_temp_release(tmp);
    return help_text;
}

#endif // MIGI_CLI_PARSE_NEW_H
