#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifndef MIGI_SMOL_MAP_H

// Very simple hashmap following the ideas from:
// https://ruby0x1.github.io/machinery_blog_archive/post/minimalist-container-library-in-c-part-1/index.html
//
// Simply stores the hashes and the indices of the values, while the actual
// values are stored in a dynamic array. The hashing is done on the user side
// while calling any of these functions.
//
// UINT64_MAX is used as the sentinel value for an empty slot. Realistically
// arrays cannot ever grow to have indices that large, so it is not an issue.
// The main disadvantage of this approach is that it cannot recover from
// collisions. Either a really good hash function should be used, or a large
// enough space should be allocated for the hashmap.

#include "migi_core.h"
#include "arena.h"

#ifndef SMOL_MAP_DEFAULT_SIZE
    #define SMOL_MAP_DEFAULT_SIZE 256
#endif // SMOL_MAP_DEFAULT_SIZE


typedef struct {
    uint64_t *hashes;
    uint64_t *values;
    size_t length;
} SmolHashmap;

// Inserts a value into the hashmap
static void smol_put(Arena *arena, SmolHashmap *hashmap, uint64_t hash, uint64_t value, bool replace);

// Returns a value or the default value provided, if it doesn't exist
static uint64_t smol_get(SmolHashmap *hashmap, uint64_t key, uint64_t default_value);

// Looks up a value into the hashmap
// If an arena is provided (arena != 0) it will update the old value with the new one
static inline uint64_t smol_lookup(Arena *arena, SmolHashmap *hashmap, uint64_t key_hash, uint64_t value);

static void smol_put(Arena *arena, SmolHashmap *hashmap, uint64_t key_hash, uint64_t value, bool replace) {
    if (hashmap->length == 0) {
        hashmap->hashes = arena_push_bytes(arena, SMOL_MAP_DEFAULT_SIZE, _Alignof(uint64_t), false);
        hashmap->values = arena_push_bytes(arena, SMOL_MAP_DEFAULT_SIZE, _Alignof(uint64_t), false);
        hashmap->length = SMOL_MAP_DEFAULT_SIZE;

        memset(hashmap->hashes, 0xff, hashmap->length*sizeof(uint64_t));
        memset(hashmap->values, 0xff, hashmap->length*sizeof(uint64_t));
    }

    size_t index = key_hash & (hashmap->length - 1);
    if (!replace) {
        avow(hashmap->hashes[index] == (uint64_t)-1,
            "smol_add: collision occured but replacement is prohibited");
    }

    hashmap->hashes[index] = key_hash;
    hashmap->values[index] = value;
}

static uint64_t smol_get(SmolHashmap *hashmap, uint64_t key_hash, uint64_t default_value) {
    size_t result = default_value;
    size_t index = key_hash & (hashmap->length - 1);
    if (hashmap->length > 0 && hashmap->values[index] != (uint64_t)-1) {
        result = hashmap->values[index];
    }
    return result;
}

static inline uint64_t smol_lookup(Arena *arena, SmolHashmap *hashmap, uint64_t key_hash, uint64_t value) {
    uint64_t result = value;
    if (arena) {
        smol_put(arena, hashmap, key_hash, value, true);
    } else {
        result = smol_get(hashmap, key_hash, 0);
    }
    return result;
}

#define MIGI_SMOL_MAP_H
#endif // MIGI_SMOL_MAP_H
