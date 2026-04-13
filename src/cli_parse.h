#ifndef MIGI_CLI_PARSE_H
#define MIGI_CLI_PARSE_H

#include <stddef.h>
#include <string.h>
#include "migi_core.h"
#include "migi_string.h"
#include "migi_list.h"

typedef struct {
    Str key;
    Str value;
    StrList values;
} FlagSlot;

typedef struct {
    FlagSlot *slots;
    StrList args;         // positional arguments
    StrList meta_args;    // arguments following a `--`, usually passed to the program being called by this program
    Str executable;

    size_t length;
    int exp;
} CmdLn;


static CmdLn cli_parse_args(Arena *arena, int argc, char *argv[]);
static FlagSlot *flag_lookup(CmdLn *flags, Str flag);


// Looking up flags by value and fallback
//
// NOTE: Use flag_exists for flags like `-v`, `-h`, 
// and flag_as_bool for flags like `--color=true`, `-f=1`
//
static bool flag_exists(CmdLn *flags, Str name);
static bool flag_as_bool(CmdLn *flags, Str name);
static Str flag_as_string(CmdLn *flags, Str name, Str fallback);
static int64_t flag_as_i64(CmdLn *flags, Str name, int64_t fallback);
static double flag_as_f64(CmdLn *flags, Str name, double fallback);
static StrList flag_as_strlist(CmdLn *flags, Str name);

// Iterate over each flag as key-value pairs
#define flag_foreach(flags, slot)                   \
    for (FlagSlot *slot = (flags).slots;            \
         slot < (flags).slots + (1LL << (flags).exp); \
         slot++)                                    \
        if (slot->key.length != 0)

// Iterate over each positional/meta argument
#define flag_args_foreach(flags, arg) strlist_foreach(&(flags).args, (arg))
#define flag_meta_args_foreach(flags, arg) strlist_foreach(&(flags).meta_args, (arg))


static int32_t flag__table_step(uint64_t hash, int exp, int32_t index) {
    uint32_t mask = ((uint32_t)1 << exp) - 1;
    uint32_t step = (uint32_t)(hash >> (64 - exp)) | 1;
    return (index + step) & mask;
}

static void flag__insert(CmdLn *flags, Str key, Str value, StrList values) {
    assertf(flags->length + 1 < (size_t)(1 << flags->exp), "flag_insert: flag table capacity exceeded!");

    uint64_t hash = str_hash(key);
    for (uint32_t i = (uint32_t)hash;;) {
        i = flag__table_step(hash, flags->exp, i);
        if (flags->slots[i].key.length == 0) {
            flags->slots[i] = (FlagSlot){
                .key    = key,
                .value  = value,
                .values = values,
            };
            flags->length += 1;
            break;
        }
    }
}

static FlagSlot *flag_lookup(CmdLn *flags, Str flag) {
    uint64_t hash = str_hash(flag);
    for (uint32_t i = (uint32_t)hash;;) {
        i = flag__table_step(hash, flags->exp, i);
        if (flags->slots[i].key.length == 0) {
            return NULL;
        }
        if (str_eq(flag, flags->slots[i].key)) {
            return &flags->slots[i];
        }
    }
}

static CmdLn cli_parse_args(Arena *arena, int argc, char *argv[]) {
    CmdLn cli = {0};

    // initialize MSI hashmap
    cli.exp = 3;
    for (; (1 << cli.exp) - (1 << (cli.exp - 3)) < argc; cli.exp++) {}
    cli.slots = arena_push(arena, FlagSlot, 1LL << cli.exp);

    cli.executable = str_from_cstr(argv[0]);

    Str flag_key = {0};

    for (int i = 1; i < argc; i++) {
        Str arg = str_from_cstr(argv[i]);
        if (arg.length == 0) continue;

        // parse as a positional argument
        if (arg.data[0] != '-') {
            strlist_push(arena, &cli.args, arg);
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
                    strlist_push_cstr(arena, &cli.meta_args, argv[i++]);
                }
                break;
            }
        }

        StrCut cut = str_cut(flag_key, S("="));
        if (!cut.found) {
            // insert as key with no value (eg: `-h`/`--help`)
            flag__insert(&cli, flag_key, S(""), (StrList){0});
            continue;
        };

        flag_key = cut.head;
        Str flag_value = cut.tail;

        // --flag=foo,bar,baz
        StrList values = {0};
        StrCut values_cut = str_cut(flag_value, S(","));
        Str prev_tail = {0};
        while (values_cut.found) {
            strlist_push(arena, &values, values_cut.head);
            prev_tail = values_cut.tail;
            values_cut = str_cut(values_cut.tail, S(","));
        }
        if (prev_tail.length != 0) {
            strlist_push(arena, &values, prev_tail);
        }

        flag__insert(&cli, flag_key, flag_value, values);
    }

    return cli;
}


static bool flag_exists(CmdLn *flags, Str name) {
    return flag_lookup(flags, name) != NULL;
}

// Use for flags like (--color=true)
static bool flag_as_bool(CmdLn *flags, Str name) {
    FlagSlot *slot = flag_lookup(flags, name);
    return slot && (str_eq(slot->value, S("true")) || str_eq(slot->value, S("1")));
}

static Str flag_as_string(CmdLn *flags, Str name, Str fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }
    return slot->value;
}

static int64_t flag_as_i64(CmdLn *flags, Str name, int64_t fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }

    Str value = slot->value;

    int64_t num = 0;
    int sign = 1;

    size_t i = 0;
    if (value.data[i] == '-') {
        sign = -1;
        i++;
    } else if (value.data[i] == '+') {
        i++;
    }

    // TODO: add parsing numbers in other bases (2, 8, 16)
    for (; i < value.length; i++) {
        if (value.data[i] < '0' || value.data[i] > '9') {
            return fallback;
        }
        int digit = value.data[i] - '0';
        num *= 10;
        num += digit;
    }
    return sign * num;
}

static double flag_as_f64(CmdLn *flags, Str name, double fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }

    Str value = flag_lookup(flags, name)->value;

    // TODO: implement strtod rather than depending on it
    // Allocating a temporary null terminated string for strtod
    char temp[64];
    char *end_ptr = NULL;
    memcpy(temp, value.data, value.length);
    temp[value.length] = 0;

    double num = strtod(temp, &end_ptr);
    if (end_ptr != temp + value.length) {
        return fallback;
    }
    return num;
}

static StrList flag_as_strlist(CmdLn *flags, Str name) {
    return flag_lookup(flags, name)->values;
}


#endif // ifndef MIGI_CLI_PARSE_H

