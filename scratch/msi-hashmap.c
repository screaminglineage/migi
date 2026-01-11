#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "migi.h"
#include "migi_string.h"

typedef enum {
    Flag_None,
    Flag_Bool,
    Flag_String,
    Flag_Int,
} FlagType;

typedef struct {
    FlagType type;
    union {
        bool as_bool;
        String as_string;
        int64_t as_int;
    };
} Flag;

#define TABLE_EXP 8
typedef struct {
    Flag data[1 << TABLE_EXP];
    size_t length;
} FlagTable;

static FlagTable hash_table;

static int32_t hash_table_lookup(uint64_t hash, int exp, int32_t index) {
    uint32_t mask = ((uint32_t)1 << exp) - 1;
    uint32_t step = (hash >> (64 - exp)) | 1;
    return (index + step) & mask;
}

void flag_insert(FlagTable *hash_table, String flag) {
    if (hash_table->length + 1 == (1 << TABLE_EXP)) {
        fprintf(stderr, "failed to add flag: flag table capacity exceeded!\n");
        return;
    }

    uint64_t hash = string_hash(flag);
    for (int32_t i = hash;;) {
        i = hash_table_lookup(hash, TABLE_EXP, i);
        if (hash_table->data[i].type == Flag_None) {
            hash_table->data[i] = (Flag){
                .type = Flag_String,
                .as_string = flag,
            };
            hash_table->length += 1;
            break;
        }
    }
}

Flag *flag_lookup(FlagTable *hash_table, String flag) {
    uint64_t hash = string_hash(flag);
    for (int32_t i = hash;;) {
        i = hash_table_lookup(hash, TABLE_EXP, i);
        if (hash_table->data[i].type == Flag_None) {
            return NULL;
        }
        return &hash_table->data[i];
    }
}

int main() {
    String flags[] = {SV("foo"), SV("bar"), SV("baz"), SV("abcd")};
    for (size_t i = 0; i < array_len(flags); i++) {
        flag_insert(&hash_table, flags[i]);
    }

    Flag *flag = flag_lookup(&hash_table, SV("baz"));


    return 0;
}
