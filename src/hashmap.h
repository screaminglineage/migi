#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "migi.h"
#include "migi_string.h"

// Taken from https://nullprogram.com/blog/2025/01/19/
uint64_t hash_fnv(byte *data, size_t length) {
    uint64_t h = 0x100;
    for (size_t i = 0; i < length; i++) {
        h ^= data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

#define HASHMAP_INIT_CAP 256
#define HASHMAP_LOAD_FACTOR 0.75

typedef struct {
    uint64_t hash;  // rehashing is not needed while growing if the hash is stored
    uint64_t index; // index of 0 means its empty
} HashEntry_;

typedef struct {
    HashEntry_ *entries;
    size_t size;
    size_t capacity;
} HashmapHeader_;


#define HASHMAP_HEADER struct { \
    HashEntry_ *entries;        \
    size_t size;                \
    size_t capacity;            \
}

static_assert(sizeof(HashmapHeader_) == sizeof(HASHMAP_HEADER), "One of the hashmap headers have changed!");


// Grow the hashmap and rehash all the keys into the new allocation
static void hms_grow(Arena *a, HashmapHeader_ *header, void **data, size_t entry_size) {
    size_t old_capacity = header->capacity;
    header->capacity = (header->capacity == 0)? HASHMAP_INIT_CAP: header->capacity*2;
    HashEntry_ *new_entries = arena_push(a, HashEntry_, header->capacity);

    // allocating an extra item for index 0 being treated as the default index
    byte *new_data = arena_push_bytes(a, entry_size * (header->capacity + 1));

    migi_mem_clear(new_entries, HashEntry_, header->capacity);
    migi_mem_clear(new_data, byte, entry_size * (header->capacity + 1));

    if (*data) {
        memcpy(new_data, *data, entry_size * (header->capacity + 1));
    }
    *data = new_data;

    for (size_t j = 0; j < old_capacity; j++) {
        if (header->entries[j].index == 0) continue;

        uint64_t i = header->entries[j].hash & (header->capacity - 1);
        while (new_entries[i].index != 0) {
            i = (i + 1) & (header->capacity - 1);
        }
        new_entries[i] = header->entries[j];
        // memcpy(&new_entries[i], &header->entries[j], sizeof(HashEntry_));
    }
    header->entries = new_entries;
}

static size_t hms_internal_index_of(HashmapHeader_ *header, void *data, size_t entry_size, String key, uint64_t *hash_out) {
    uint64_t hash = hash_fnv((byte *)key.data, key.length);
    size_t i = hash & (header->capacity - 1);
    while (true) {
        // return if empty
        if (header->entries[i].index == 0) break;

        byte *table = data;
        byte *item = table + (header->entries[i].index * entry_size);
        String map_key = *(String *)item;

        // return if key was found
        if (string_eq(key, map_key)) break;

        i = (i + 1) & (header->capacity - 1);
    }
    *hash_out = hash;
    return i;
}

static void hms_put_impl(Arena *a, HashmapHeader_ *header, void **data, size_t entry_size, String key, void *value) {
    if (header->size >= header->capacity * HASHMAP_LOAD_FACTOR) {
        hms_grow(a, header, data, entry_size);
    }

    uint64_t hash = 0;
    size_t i = hms_internal_index_of(header, *data, entry_size, key, &hash);
    size_t actual_index = header->size + 1;
    byte *table = *data;
    byte *item = table + (actual_index * entry_size);

    if (header->entries[i].index == 0) {
        header->size++;
        header->entries[i].hash = hash;
        header->entries[i].index = actual_index;
        memcpy(item, &key, sizeof(key));
    }
    memcpy(item + sizeof(key), value, entry_size - sizeof(key));
}

static void *hms_del_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    if (header->capacity == 0) return NULL;
    uint64_t hash_ = 0;
    size_t i = hms_internal_index_of(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return NULL;
    }

    // Swap-Remove the key-value pair in the table
    byte *table = data;
    byte *item = table + (header->entries[i].index * entry_size);
    byte *last = table + (header->size * entry_size);

    // TODO: maybe use a temporary arena instead of a VLA
    byte temp[entry_size];
    memcpy(temp, item, entry_size);
    memcpy(item, last, entry_size);
    memcpy(last, temp, entry_size);

    header->entries[i].index = 0;
    header->size--;
    return last;
}

static ptrdiff_t hms_index_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_internal_index_of(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return -1;
    }
    return header->entries[i].index;
}

// Returns NULL if not found
static void *hms_get_value_null_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_internal_index_of(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size) + sizeof(key);
}

// Returns NULL if not found
static void *hms_get_pair_null_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_internal_index_of(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size);
}

// Returns the default value at index 0 if not found
static void *hms_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_internal_index_of(header, data, entry_size, key, &hash_);

    byte *table = data;
    return table + (header->entries[i].index * entry_size);
}

static void hms_set_default_value(HashmapHeader_ *header, void *value, size_t value_size) {
    memcpy(header->entries, value, value_size);
}


// Insert a new key-value pair or update the value if it already exists
#define hms_put(arena, hashmap, key, value)                   \
    (hms_put_impl((arena), (HashmapHeader_*)(hashmap),        \
        (void **)&(hashmap)->data, sizeof *((hashmap)->data), \
        (key), &(value)))

// Return a pointer to value if it exists, otherwise NULL
#define hms_entry(hashmap, type, key)                            \
    ((type *)hms_get_value_null_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Return the value if it exists, otherwise the 0 value at index 0
#define hms_get(hashmap, type, key)                          \
    (*(type *)(hms_get_pair_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key))   \
            + sizeof(key)))

// Return a pointer to the key-value pair if it exists, otherwise NULL
#define hms_entry_pair(hashmap, type, key)                      \
    ((type *)hms_get_pair_null_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))     \

// Return a pointer to the key-value pair if it exists, otherwise the 0 value at index 0
#define hms_get_pair(hashmap, type, key)                    \
    (*(type *)hms_get_pair_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key))) \

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hms_index(hashmap, key)                                  \
    (hms_index_impl((HashmapHeader_*)(hashmap), (hashmap)->data, \
        sizeof (*(hashmap)->data), (key)))

// Remove a key-value pair if it exists, and return it
#define hms_pop(hashmap, type, key)                         \
    (*(type *)hms_del_impl((HashmapHeader_ *)(hashmap),     \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Remove a key-value pair if it exists
#define hms_del(hashmap, key)                                \
    ((void)hms_del_impl((HashmapHeader_ *)(hashmap),         \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hm_foreach(hashmap, pair_type, pair)       \
    for (pair_type *(pair) = (hashmap)->data + 1;  \
        pair <= (hashmap)->data + (hashmap)->size; \
        pair++)


#endif // MIGI_HASHMAP_H
