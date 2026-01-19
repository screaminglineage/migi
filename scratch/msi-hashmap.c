#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "migi.h"
#include "migi_string.h"
#include "migi_list.h"

typedef struct {
    String key;
    String value;
} FlagSlot;

typedef struct {
    FlagSlot *slots;
    StringList args; // positional arguments
    String executable;

    size_t length;
    int exp;
} FlagTable;

static int32_t hash_table_lookup(uint64_t hash, int exp, int32_t index) {
    uint32_t mask = ((uint32_t)1 << exp) - 1;
    uint32_t step = (hash >> (64 - exp)) | 1;
    return (index + step) & mask;
}

static void flag_insert(FlagTable *flags, String key, String value) {
    assertf(flags->length + 1 < (size_t)(1 << flags->exp), "flag_insert: flag table capacity exceeded!");

    uint64_t hash = string_hash(key);
    for (int32_t i = hash;;) {
        i = hash_table_lookup(hash, flags->exp, i);
        if (flags->slots[i].key.length == 0) {
            flags->slots[i] = (FlagSlot){
                .key = key,
                .value = value
            };
            flags->length += 1;
            break;
        }
    }
}

static String *flag_lookup(FlagTable *flags, String flag) {
    uint64_t hash = string_hash(flag);
    for (int32_t i = hash;;) {
        i = hash_table_lookup(hash, flags->exp, i);
        if (flags->slots[i].key.length == 0) {
            return NULL;
        }
        if (string_eq(flag, flags->slots[i].key)) {
            return &flags->slots[i].value;
        }
    }
}

// TODO: add support for --flag=a,b,c and --flag a,b,c
// TODO: cleanup this function a little bit, maybe convert to a state machine
static FlagTable cli_parse_args(Arena *arena, int argc, char *argv[]) {
    FlagTable flags = {0};
    flags.exp = 3;
    for (; (1 << flags.exp) - (1 << (flags.exp - 3)) < argc; flags.exp++) {}
    flags.slots = arena_push(arena, FlagSlot, 1 << flags.exp);

    char **ptr = argv;
    flags.executable = string_from_cstr(*ptr);
    ptr++;

    while (*ptr) {
        char *arg = *ptr++;
        if (arg[0] == '-') {
            String flag_key = {0};
            if (arg[1] && arg[1] == '-') {
                // --flag
                flag_key = string_skip(string_from_cstr(arg), 2);
            } else {
                // -flag
                flag_key = string_skip(string_from_cstr(arg), 1);
            }

            String flag_value = {0};
            StringCut cut = string_cut(flag_key, SV("="));
            if (cut.valid) {
                // --flag=value or -flag=value
                flag_key = cut.head;
                flag_value = cut.tail;
            } else {
                // --flag value or -flag value
                if (*ptr) {
                    arg = *ptr++;
                    if (arg[0] == '-') {
                        if (arg[1] && '0' <= arg[1] && arg[1] <= '9') {
                            // parse negative numbers, numeric flags are disallowed
                            flag_value = string_from_cstr(arg);
                        } else {
                            ptr--;
                        }
                    } else {
                        flag_value = string_from_cstr(arg);
                    }
                }
            }
            if (flag_key.length != 0) {
                flag_insert(&flags, flag_key, flag_value);
            }
        } else {
            StringNode *node = arena_new(arena, StringNode);
            node->string = string_from_cstr(arg);
            queue_push(flags.args.head, flags.args.tail, node);
        }
    }

    return flags;
}

// Use for flags like (-v, -h)
static bool flag_exists(FlagTable *flags, String name) {
    return flag_lookup(flags, name) != NULL;
}

// Use for flags like (--color=true)
static bool flag_as_bool(FlagTable *flags, String name) {
    String *value = flag_lookup(flags, name);
    return value && (string_eq(*value, SV("true")) || string_eq(*value, SV("1")));
}

static String flag_as_string(FlagTable *flags, String name, String fallback) {
    String *value = flag_lookup(flags, name);
    if (!value || value->length == 0) {
        return fallback;
    }
    return *value;
}

static int64_t flag_as_i64(FlagTable *flags, String name, int64_t fallback) {
    String *value = flag_lookup(flags, name);
    if (!value || value->length == 0) {
        return fallback;
    }

    int64_t num = 0;
    int sign = 1;

    size_t i = 0;
    if (value->data[i] == '-') {
        sign = -1;
        i++;
    } else if (value->data[i] == '+') {
        i++;
    }

    // TODO: add parsing numbers in other bases (2, 8, 16)
    for (; i < value->length; i++) {
        if (value->data[i] < '0' || value->data[i] > '9') {
            return fallback;
        }
        int digit = value->data[i] - '0';
        num *= 10;
        num += digit;
    }
    return sign * num;
}


static double flag_as_f64(FlagTable *flags, String name, double fallback) {
    String *value = flag_lookup(flags, name);
    if (!value || value->length == 0) {
        return fallback;
    }

    // TODO: implement strtod rather than depending on it
    // Allocating a temporary null terminated string for strtod
    char temp[64];
    char *end_ptr = NULL;
    memcpy(temp, value->data, value->length);
    temp[value->length] = 0;

    double num = strtod(temp, &end_ptr);
    if (end_ptr != temp + value->length) {
        return fallback;
    }
    return num;
}

// Iterate over each flag as key-value pairs
#define flag_foreach(flags, slot)                   \
    for (FlagSlot *slot = (flags).slots;            \
         slot < (flags).slots + (1 << (flags).exp); \
         slot++)                                    \
        if (slot->key.length != 0)

// Iterate over each positional argument
#define flag_args_foreach(flags, arg) list_foreach((flags).args.head, StringNode, (arg))


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

