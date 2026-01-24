#ifndef CLI_PARSE_H
#include <stddef.h>
#include <string.h>
#include "migi.h"

typedef struct {
    String key;
    String value;
    StringList values;
} FlagSlot;

typedef struct {
    FlagSlot *slots;
    StringList args;         // positional arguments
    StringList meta_args;    // arguments following a `--`, usually passed to the program being called by this program
    String executable;

    size_t length;
    int exp;
} CmdLn;

static int32_t hash_table_lookup(uint64_t hash, int exp, int32_t index) {
    uint32_t mask = ((uint32_t)1 << exp) - 1;
    uint32_t step = (uint32_t)(hash >> (64 - exp)) | 1;
    return (index + step) & mask;
}

static void flag_insert(CmdLn *flags, String key, String value, StringList values) {
    assertf(flags->length + 1 < (size_t)(1 << flags->exp), "flag_insert: flag table capacity exceeded!");

    uint64_t hash = string_hash(key);
    for (uint32_t i = (uint32_t)hash;;) {
        i = hash_table_lookup(hash, flags->exp, i);
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

static FlagSlot *flag_lookup(CmdLn *flags, String flag) {
    uint64_t hash = string_hash(flag);
    for (uint32_t i = (uint32_t)hash;;) {
        i = hash_table_lookup(hash, flags->exp, i);
        if (flags->slots[i].key.length == 0) {
            return NULL;
        }
        if (string_eq(flag, flags->slots[i].key)) {
            return &flags->slots[i];
        }
    }
}

// TODO: add support for --flag=a,b,c and --flag a,b,c
static CmdLn cli_parse_args(Arena *arena, int argc, char *argv[]) {
    CmdLn cli = {0};

    // initialize MSI hashmap
    cli.exp = 3;
    for (; (1 << cli.exp) - (1 << (cli.exp - 3)) < argc; cli.exp++) {}
    cli.slots = arena_push(arena, FlagSlot, 1LL << cli.exp);

    cli.executable = string_from_cstr(argv[0]);

    String flag_key = {0};

    for (size_t i = 1; i < argc; i++) {
        String arg = string_from_cstr(argv[i]);
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
            flag_key = string_skip(arg, 1);
        } else {
            // --flag
            flag_key = string_skip(arg, 2);

            // if arg is only a `--` parse everything after it as meta arguments
            if (flag_key.length == 0) {
                while (i < argc) {
                    strlist_push_cstr(arena, &cli.meta_args, argv[i++]);
                }
                break;
            }
        }

        StringCut cut = string_cut(flag_key, SV("="));
        if (!cut.valid) {
            // insert as key with no value (eg: `-h`/`--help`)
            flag_insert(&cli, flag_key, SV(""), (StringList){0});
            continue;
        };

        flag_key = cut.head;
        String flag_value = cut.tail;

        // --flag=foo,bar,baz
        StringList values = {0};
        StringCut values_cut = string_cut(flag_value, SV(","));
        String prev_tail = {0};
        while (values_cut.valid) {
            strlist_push(arena, &values, values_cut.head);
            prev_tail = values_cut.tail;
            values_cut = string_cut(values_cut.tail, SV(","));
        }
        if (prev_tail.length != 0) {
            strlist_push(arena, &values, prev_tail);
        }

        flag_insert(&cli, flag_key, flag_value, values);
    }

    return cli;
}


// Use for flags like (-v, -h)
static bool flag_exists(CmdLn *flags, String name) {
    return flag_lookup(flags, name) != NULL;
}

// Use for flags like (--color=true)
static bool flag_as_bool(CmdLn *flags, String name) {
    FlagSlot *slot = flag_lookup(flags, name);
    return slot && (string_eq(slot->value, SV("true")) || string_eq(slot->value, SV("1")));
}

static String flag_as_string(CmdLn *flags, String name, String fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }
    return slot->value;
}

static int64_t flag_as_i64(CmdLn *flags, String name, int64_t fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }

    String value = slot->value;

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

static double flag_as_f64(CmdLn *flags, String name, double fallback) {
    FlagSlot *slot = flag_lookup(flags, name);
    if (!slot || slot->value.length == 0) {
        return fallback;
    }

    String value = flag_lookup(flags, name)->value;

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

static StringList flag_as_strlist(CmdLn *flags, String name) {
    return flag_lookup(flags, name)->values;
}


// Iterate over each flag as key-value pairs
#define flag_foreach(flags, slot)                   \
    for (FlagSlot *slot = (flags).slots;            \
         slot < (flags).slots + (1LL << (flags).exp); \
         slot++)                                    \
        if (slot->key.length != 0)

// Iterate over each positional argument
#define flag_args_foreach(flags, arg) list_foreach((flags).args.head, StringNode, (arg))

#endif // ifndef CLI_PARSE_H

