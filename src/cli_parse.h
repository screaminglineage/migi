#ifndef MIGI_CLI_PARSE_NEW_H
#define MIGI_CLI_PARSE_NEW_H

#include <inttypes.h>
#include "migi_core.h"
#include "arena.h"
#include "migi_list.h"
#include "string_builder.h"

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

#define CLI_BOOL_FOUND -1
typedef struct {
    Str name;
    Str help;
    StrSpan aliases;
    int32_t nargs;
    int32_t found_args;  // NOTE: for bool's that do not take any arguments, this is set to `CLI_BOOL_FOUND` (-1) if the flag was set
    bool required;
    bool was_set;        // whether the option was passed on the command line, "commented" options are not counted

    CliArgType type;
    union {
        Str as_str;
        int64_t as_int;
        bool as_bool;
        double as_double;
        StrList as_list;
    };
    union {
        Str default_str;
        int64_t default_int;
        bool default_bool;
        double default_double;
    };
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

    Arena *arena;
} Cli;


typedef struct {
    bool required;         // argument must be provided if set
    Str value;             // default value of argument
    StrSpan aliases;       // aliases that map to the same option
    Cli *cli;              // cli context to store options         [default: uses global cli context]
} CliStrOpt;
// Str *cli_add_str(name, help, ...)
static Str *cli_add_str_opt(Str name, Str help, CliStrOpt opt);

typedef struct {
    bool required;
    int64_t value;
    StrSpan aliases;
    Cli *cli;
} CliIntOpt;
// int64_t *cli_add_int(name, help, ...)
static int64_t *cli_add_i64_opt(Str name, Str help, CliIntOpt opt);

typedef struct {
    bool takes_arg;   // whether to take an argument, supported arguments: `1`, `0`, `y[es]`, `n[o]` `true`, `false` (case insensitive)
    bool required;
    bool value;
    StrSpan aliases;
    Cli *cli;
} CliBoolOpt;
// bool *cli_add_bool(name, help, ...)
static bool *cli_add_bool_opt(Str name, Str help, CliBoolOpt opt);

typedef struct {
    bool required;
    double value;
    StrSpan aliases;
    Cli *cli;
} CliDoubleOpt;
// double *cli_add_double(name, help, ...)
static double *cli_add_double_opt(Str name, Str help, CliDoubleOpt opt);

// TODO: maybe add support for default list values
// However it might not really be needed for most use cases of list
//
// If added should probably be a StrSpan. During the validation stage,
// if there are default values they will be added if there are missing string
// values.
// For example, cli_add_list(/* ... */, .nargs=3, .value=str_span("a", "b", "c", "d", "e"))
// and -list=x,y,z will result in list being ["x", "y", "z", "d", "e"]
// Storing the default value as a StrSpan will allow easily copying the items,
// starting from a particular index like mentioned above
#define CLI_NARGS_INF -1
typedef struct {
    bool required;     // if set then checks that exactly `nargs` arguments are provided, otherwise
                       // less than `nargs` arguments are allowed, but not any more [default: false]

    int32_t nargs;     // no. of arguments to take [default: -1 or `CLI_NARGS_INF` for no limit]
    StrSpan aliases;
    Cli *cli;
} CliListStrOpt;
// StrList *cli_add_list(name, help, ...)
static StrList *cli_list_str_opt(Str name, Str help, CliListStrOpt opt);


typedef struct {
    Cli *cli;             // cli context to store options         [default: uses global cli context]
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


// Convenience macros for operating on variables returned by `cli_add_*` functions
#define cli_var_name(var) cli_arg_from_var((var))->name
#define cli_var_was_set(var) cli_arg_from_var((var))->was_set


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

#define cli_parse_args(argc, argv, ...)                \
    cli_parse_args_opt((argc), (argv), (CliParseOpt) { \
        .cli = &global_cli,                            \
        __VA_ARGS__                                    \
    })


#define cli_add_str(name, help, ...)               \
    cli_add_str_opt((name), (help), (CliStrOpt) {  \
        .cli = &global_cli,                        \
        __VA_ARGS__                                \
    })

#define cli_add_i64(name, help, ...)               \
    cli_add_i64_opt((name), (help), (CliIntOpt){   \
        .cli = &global_cli,                        \
        __VA_ARGS__                                \
    })

#define cli_add_bool(name, help, ...)              \
    cli_add_bool_opt((name), (help), (CliBoolOpt){ \
        .cli = &global_cli,                        \
        __VA_ARGS__                                \
    })

#define cli_add_double(name, help, ...)                \
    cli_add_double_opt((name), (help), (CliDoubleOpt){ \
        .cli = &global_cli,                            \
        __VA_ARGS__                                    \
    })

#define cli_add_list(name, help, ...)                 \
    cli_list_str_opt((name), (help), (CliListStrOpt){ \
        .cli = &global_cli,                           \
        .nargs = CLI_NARGS_INF,                       \
        __VA_ARGS__                                   \
    })


static void cli__init(Arena *arena, Cli *cli) {
    cli->exp = 3;
    for (; (1 << cli->exp) - (1 << (cli->exp - 3)) < CLI_MAX_OPTIONS; cli->exp++) {}
    cli->slots = arena_push(arena, CliSlot, 1LL << cli->exp);
    cli->args = arena_push(arena, CliArg, CLI_MAX_OPTIONS);
}


static void cli_free() {
    arena_free(global_cli.arena);
    mem_clear(&global_cli);
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
        .name        = name,
        .help        = help,
        .aliases     = opt.aliases,
        .type        = CliArg_Str,
        .as_str      = opt.value,
        .default_str = opt.value,
        .nargs       = 1,
        .required    = opt.required
    };

    if (!opt.cli->arena) opt.cli->arena = arena_init();

    int32_t index = cli__push_arg(opt.cli->arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.cli->arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.cli->arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_str;
}

static int64_t *cli_add_i64_opt(Str name, Str help, CliIntOpt opt) {
    CliArg arg = {
        .name        = name,
        .help        = help,
        .aliases     = opt.aliases,
        .type        = CliArg_Int,
        .as_int      = opt.value,
        .default_int = opt.value,
        .nargs       = 1,
        .required    = opt.required,
    };

    if (!opt.cli->arena) opt.cli->arena = arena_init();

    int32_t index = cli__push_arg(opt.cli->arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.cli->arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.cli->arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_int;
}

static bool *cli_add_bool_opt(Str name, Str help, CliBoolOpt opt) {
    CliArg arg = {
        .name         = name,
        .help         = help,
        .aliases      = opt.aliases,
        .type         = CliArg_Bool,
        .as_bool      = opt.value,
        .default_bool = opt.value,
        .nargs        = opt.takes_arg,
        .required     = opt.required,
    };

    if (!opt.cli->arena) opt.cli->arena = arena_init();

    int32_t index = cli__push_arg(opt.cli->arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.cli->arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.cli->arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_bool;
}

static double *cli_add_double_opt(Str name, Str help, CliDoubleOpt opt) {
    CliArg arg = {
        .name           = name,
        .help           = help,
        .aliases        = opt.aliases,
        .type           = CliArg_Double,
        .as_double      = opt.value,
        .default_double = opt.value,
        .nargs          = 1,
        .required       = opt.required,
    };

    if (!opt.cli->arena) opt.cli->arena = arena_init();

    int32_t index = cli__push_arg(opt.cli->arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.cli->arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.cli->arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_double;
}

static StrList *cli_list_str_opt(Str name, Str help, CliListStrOpt opt) {
    CliArg arg = {
        .name     = name,
        .help     = help,
        .aliases  = opt.aliases,
        .type     = CliArg_List,
        .nargs    = opt.nargs,
        .required = opt.required,
    };

    if (!opt.cli->arena) opt.cli->arena = arena_init();

    int32_t index = cli__push_arg(opt.cli->arena, opt.cli, arg);
    assertf(cli__lookup(opt.cli, name) == NULL, "redefinition of option: '%.*s'", SArg(name));
    cli__insert(opt.cli->arena, opt.cli, name, index);
    array_foreach(&opt.aliases, alias) {
        assertf(cli__lookup(opt.cli, *alias) == NULL, "redefinition of option: '%.*s'", SArg(name));
        cli__insert(opt.cli->arena, opt.cli, *alias, index);
    }

    return &opt.cli->args[index].as_list;
}

static bool cli__parse_value(Arena *arena, CliArg *cli_arg, Str key, Str value, bool ignore) {
    bool result = false;

    Temp tmp = arena_temp_excl(arena);

    switch (cli_arg->type) {
        case CliArg_Str: {
            if (!ignore) {
                cli_arg->as_str = value;
                cli_arg->was_set = true;
            }
            cli_arg->found_args = 1;
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
            if (!ignore) {
                cli_arg->as_int = num;
                cli_arg->was_set = true;
            }
            cli_arg->found_args = 1;
        } break;
        case CliArg_Bool: {
            bool bool_value = false;
            int32_t found_args = 0;

            Str arg_lower = str_to_lower(tmp.arena, value);
            if (str_eq_any(arg_lower, S("1"), S("y"), S("yes"), S("true"))) {
                bool_value = true;
                found_args = 1;
            } else if (str_eq_any(arg_lower, S("0"), S("n"), S("no"), S("false"))) {
                bool_value = false;
                found_args = 1;
            } else if (cli_arg->nargs == 0) {
                // flag variables that take no arguments
                bool_value = true;
                found_args = CLI_BOOL_FOUND;
            } else {
                migi_log(Log_Error, "expected value of type bool for option: '-%.*s' "
                        "(supported values are: 1/0, y[es]/n[o], true/false) but got: '%.*s'",
                        SArg(key), SArg(value));
                goto end;
            }
            if (!ignore) {
                cli_arg->as_bool = bool_value;
                cli_arg->was_set = true;
            }
            cli_arg->found_args = found_args;
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
            if (!ignore) {
                cli_arg->as_double = num;
                cli_arg->was_set = true;
            }
            cli_arg->found_args = 1;
        } break;
        case CliArg_List: {
            StrCut values_cut = str_cut(value, S(","));
            StrList items = {0};
            Str prev_tail = {0};
            do {
                if (values_cut.head.length == 0) {
                    migi_log(Log_Error, "empty argument passed to list option: '-%.*s'", SArg(cli_arg->name));
                    goto end;
                }
                strlist_push(arena, &items, values_cut.head);
                prev_tail = values_cut.tail;
                values_cut = str_cut(values_cut.tail, S(","));
            } while (values_cut.found);

            if (prev_tail.length != 0) {
                strlist_push(arena, &items, prev_tail);
            }
            if (!ignore) {
                strlist_extend(&cli_arg->as_list, &items);
                cli_arg->was_set = true;
            }
            cli_arg->found_args += items.length;
        } break;
        default:
            migi_unreachable();
    }

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

// Validation for required arguments
static bool cli__validate_args(Cli *cli) {
    clic_foreach(cli, arg) {
        if (arg->type == CliArg_List) {
            if (arg->required && arg->nargs != CLI_NARGS_INF && arg->nargs != arg->found_args) {
                migi_log(Log_Error, "too %s arguments for option: '-%.*s', expected %d but got %d",
                        arg->found_args < arg->nargs? "few": "many",
                        SArg(arg->name), arg->nargs, arg->found_args);
                return false ;
            } else if ((size_t)arg->nargs < arg->as_list.length) {
                migi_log(Log_Error, "too many arguments for option: '-%.*s', expected %d but got %zu",
                        SArg(arg->name), arg->nargs, arg->as_list.length);
                return false ;
            }
        } else if (arg->required && arg->found_args == 0) {
            migi_log(Log_Error, "option: '-%.*s' is required but was not provided", SArg(arg->name));
            return false ;
        }
    }

    return true;
}


static bool cli_parse_args_opt(int argc, char **argv, CliParseOpt opt) {
    bool result = true;

    Temp tmp = arena_temp();

    Arena *cli_arena = opt.cli->arena;
    if (!cli_arena) cli_arena = arena_init();

    if (opt.cli->slots == NULL) {
        cli__init(cli_arena, opt.cli);
    }

    // Insert help option and handle it if it wasnt provided
    bool handle_help_flag = false;
    if (!cli__lookup(opt.cli, S("help"))) {
        cli_add_bool_opt(S("help"), S("show this help message"),
            (CliBoolOpt){
                .aliases = str_span_new(cli_arena, S("h")),
                .cli     = opt.cli,
        });
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
            strlist_push(cli_arena, &opt.cli->pos_args, arg);
            continue;
        }

        // `-` on its own is invalid, skip it
        if (arg.length == 1) continue;
        Str key = str_skip(arg, 1);

        bool ignore = false;
        // if the option is `--` parse everything after it as meta arguments
        if (key.data[0] == '-') {
            i++;
            while (i < argc) {
                strlist_push_cstr(cli_arena, &opt.cli->meta_args, argv[i++]);
            }
            break;

        // ignore options that begin with -/
        // Eg. `-/foo 1` is parsed and type checked but the value is ultimately ignored
        // Idea from: https://github.com/tsoding/flag.h
        } else if (key.data[0] == '/') {
            ignore = true;
            key = str_skip(key, 1);

            // `-/` on its own is invalid, skip it
            if (key.length == 0) {
                continue;
            }
        }

        StrCut cut = str_cut(key, S("="));
        Str value = str_zero();
        CliArg *cli_arg = NULL;
        // -opt foo
        if (!cut.found) {
            cli_arg = cli_arg_by_name(opt.cli, key);
            if (!cli_arg) goto_end_with(false);

            if (cli_arg->nargs > 0) {
                if (i + 1 == argc) {
                    migi_log(Log_Error, "expected argument after option: '%.*s'", SArg(key));
                    goto_end_with(false);
                }
                // Consume the next argument as the value
                value = str_from_cstr(argv[++i]);
            }

        // -opt=foo
        } else {
            key = cut.head;
            value = cut.tail;
            cli_arg = cli_arg_by_name(opt.cli, key);
            if (!cli_arg) goto_end_with(false);

            // NOTE: `-opt="foo,bar"` is currently parsed like [`foo`, `bar`]
            // However since quotes are handled by the shell, by the time the program
            // gets access to the `argv` array, they have already been removed.
            // This makes it impossible to check for this case.

            if (value.length == 0) {
                migi_log(Log_Error, "expected value after: '-%.*s='", SArg(key));
                goto_end_with(false);
            }
        }

        if (!cli__parse_value(cli_arena, cli_arg, key, value, ignore)) {
            goto_end_with(false);
        }
    }

    if (!cli__validate_args(opt.cli)) goto_end_with(false);

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
        case CliArg_Str:    return S("str");
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
    StrBuilder options_sb = {.arena=tmp.arena};
    clic_foreach(opt.cli, arg) {
        array_foreach(&arg->aliases, alias) {
            sb_pushf(&options_sb, "-%.*s, ", SArg(*alias));
        }

        if (arg->nargs == 0) {
            sb_pushf(&options_sb, "-%.*s", SArg(arg->name));
        } else if (arg->nargs == 1) {
            sb_pushf(&options_sb, "-%.*s <%.*s>",
                    SArg(arg->name), SArg(cli_arg_type_to_str(arg->type)));
        } else if (arg->nargs > 1) {
            sb_pushf(&options_sb, "-%.*s <str[%d]>", SArg(arg->name), arg->nargs);
        } else if (arg->nargs == CLI_NARGS_INF) {
            sb_pushf(&options_sb, "-%.*s <str[..]>", SArg(arg->name));
        }

        if (arg->required) {
            sb_push(&options_sb, S(" (required)"));
        }

        max_option_length = max_of(options_sb.length, (int64_t)max_option_length);
        Str option = sb_to_str(&options_sb);
        strlist_push(tmp.arena, &options, option);
    }

    size_t i = 0;
    StrBuilder options_list = {.arena=arena};
    strlist_foreach(&options, option) {
        CliArg arg = opt.cli->args[i++];
        sb_push(&options_list, S("  "));
        sb_push(&options_list, option->string);

        int min_space_count = 5;
        size_t space_count = min_space_count + max_option_length - option->string.length;
        sb_pushf(&options_list, "%*.s%.*s", (int)space_count, " ", SArg(arg.help));

        switch (arg.type) {
            case CliArg_Str: {
                if (arg.default_str.length) {
                    sb_pushf(&options_list, " [default: %.*s]", SArg(arg.default_str));
                }
            } break;
            case CliArg_Int:  {
                if (arg.default_int) {
                    sb_pushf(&options_list, " [default: %"PRId64"]", arg.default_int);
                }
            } break;
            case CliArg_Bool:  {
                if (arg.default_bool) {
                    sb_pushf(&options_list, " [default: %s]", bool_to_cstr(arg.default_bool));
                }
            } break;
            case CliArg_Double: {
                if (arg.default_double) {
                    sb_pushf(&options_list, " [default: %.3f]", arg.default_double);
                }
            } break;

            case CliArg_List:  /* no default value for lists */ break;
            case CliArg_None:   migi_unreachable();
        }
        sb_push_char(&options_list, '\n');
    }
    arena_temp_release(tmp);

    return sb_to_str(&options_list);
}

static Str cli_help_text_opt(Arena *arena, CliOpt opt) {
    Temp tmp = arena_temp_excl(arena);
    Cli *cli = opt.cli;
    StrBuilder help_text = {.arena=arena};

    sb_pushf(&help_text, "usage: %.*s [OPTIONS]\n", SArg(cli->executable));
    if (cli->help.length != 0) {
        sb_pushf(&help_text, "\n%.*s\n\n", SArg(cli->help));
    }

    if (cli->args_length > 0) {
        sb_push(&help_text, S("Options:\n"));
        // Since the same arena that is backing `help_text` is passed into `cli_options_list_opt`,
        // the `StrBuilder`'s can simply be increased by the length of the pushed string
        help_text.length += cli_options_list_opt(arena, opt).length;
    }

    sb_push_char(&help_text, '\n');
    arena_temp_release(tmp);
    return sb_to_str(&help_text);
}

#endif // MIGI_CLI_PARSE_NEW_H
