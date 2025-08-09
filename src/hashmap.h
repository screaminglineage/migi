#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROFILER_H_IMPLEMENTATION
#include "profiler.h"

#include "arena.h"
#include "migi.h"
#include "migi_string.h"


// Taken from https://nullprogram.com/blog/2025/01/19/
static inline uint64_t hash_fnv(byte *data, size_t length) {
    uint64_t h = 0x100;
    for (size_t i = 0; i < length; i++) {
        h ^= data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

#ifndef HASHMAP_INIT_CAP
#   define HASHMAP_INIT_CAP 256
#endif

#ifndef HASHMAP_LOAD_FACTOR
#   define HASHMAP_LOAD_FACTOR 0.75
#endif

// index of default key value pair in the table
#define HASHMAP_DEFAULT_PAIR 0

typedef struct {
    uint64_t hash;  // rehashing is not needed while growing if the hash is stored
    uint64_t desired;  // desired index for robin hood linear probing
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
    TIME_FUNCTION;
    size_t old_capacity = header->capacity;
    header->capacity = (header->capacity == 0)? HASHMAP_INIT_CAP: header->capacity*2;
    HashEntry_ *new_entries = arena_push(a, HashEntry_, header->capacity);

    // allocating an extra item for index 0 being treated as the default index
    byte *new_data = arena_push_bytes(a, entry_size * (header->capacity + 1), _Alignof(byte));

    migi_mem_clear(new_entries, header->capacity);
    migi_mem_clear(new_data, entry_size * (header->capacity + 1));

    if (*data) {
        memcpy(new_data, *data, entry_size * (header->size + 1));
    }
    *data = new_data;

    for (size_t j = 0; j < old_capacity; j++) {
        if (header->entries[j].index == 0) continue;

        uint64_t i = header->entries[j].hash & (header->capacity - 1);
        while (new_entries[i].index != 0) {
            i = (i + 1) & (header->capacity - 1);
        }
        new_entries[i] = header->entries[j];
    }
    header->entries = new_entries;
}

#if 0
static bool hms_internal_insert_if_new(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;
    uint64_t hash = hash_fnv((byte *)key.data, key.length);

    size_t i = hash & (header->capacity - 1);
    size_t desired = i;
    size_t dist = 0;
    size_t actual_index = header->size + 1;

    size_t first_insert_index = 0;
    byte *table = data;
    while (header->entries[i].index != 0) {
        byte *item = table + (header->entries[i].index * entry_size);
        String cur_key = *(String *)item;

        size_t cur_desired = header->entries[i].desired;
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);

        if (cur_dist < dist) {
            desired = cur_desired;
            dist = cur_dist;
            if (!first_insert_index) {
                first_insert_index = i;
                hash = header->entries[i].hash;
                actual_index = header->entries[i].index;
                desired = header->entries[i].desired;
            } else {
                migi_swap(hash, header->entries[i].hash);
                migi_swap(actual_index, header->entries[i].index);
                migi_swap(desired, header->entries[i].desired);
                memcpy(item, &key, sizeof(key));
                key = cur_key;
            }
       }

        // return if key already exists
        if (string_eq(key, cur_key)) {
            break;
        }

        dist++;
        i = (i + 1) & (header->capacity - 1);
    }

    // breaked cause of key already existing
    if (header->entries[i].index != 0) {
        header->entries[first_insert_index].index = 
    }

    // insert key if new
    byte *item = table + (actual_index * entry_size);

    header->size++;
    header->entries[i].hash = hash;
    header->entries[i].index = actual_index;
    memcpy(item, &key, sizeof(key));

    return true;
}
#endif


// Inserts key assuming that it doesnt already exist
// WARNING: Always call contains/get before calling this
static void hms_internal_insert(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;

    // inserting key in the table
    header->size++;
    size_t actual_index = header->size;
    byte *table = data;
    byte *item = table + (actual_index * entry_size);
    memcpy(item, &key, sizeof(key));

    // use robin hood probing to adjust existing entries
    uint64_t hash = hash_fnv((byte *)key.data, key.length);
    size_t i = hash & (header->capacity - 1);
    size_t desired = i;
    size_t dist = 0;
    while (header->entries[i].index != 0) {
        size_t cur_desired = header->entries[i].desired;
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);

        if (cur_dist < dist) {
            migi_swap(hash, header->entries[i].hash);
            migi_swap(actual_index, header->entries[i].index);
            migi_swap(desired, header->entries[i].desired);
            desired = cur_desired;
            dist = cur_dist;
       }

        dist++;
        i = (i + 1) & (header->capacity - 1);
    }

    header->entries[i].hash = hash;
    header->entries[i].index = actual_index;
    header->entries[i].desired = desired;
}



// Returns the index of key in the hashmap entries
// Returns false if key doesnt exist
static bool hms_internal_index(HashmapHeader_ *header, void *data, size_t entry_size, String key, size_t *index) {
    TIME_FUNCTION;
    uint64_t hash = hash_fnv((byte *)key.data, key.length);
    size_t i = hash & (header->capacity - 1);
    size_t dist = 0;

    while (true) {
        if (header->entries[i].index == 0) return false;

        byte *table = data;
        byte *item = table + (header->entries[i].index * entry_size);
        String map_key = *(String *)item;

        // return if key was found
        if (string_eq(key, map_key)) {
            // TODO: return header->entries[i].index instead
            // requires changing a bunch of code that calls this function
            *index = i;
            return true;
        };

        size_t cur_desired = header->entries[i].desired;
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);
        if (cur_dist < dist) return false;

        dist++;
        i = (i + 1) & (header->capacity - 1);
    }
}

static void hms_put_impl(Arena *a, HashmapHeader_ *header, void **data, size_t entry_size, String key, void *value) {
    TIME_FUNCTION;
    if (header->size >= header->capacity * HASHMAP_LOAD_FACTOR) {
        hms_grow(a, header, data, entry_size);
    }

    size_t index_ = 0;
    if (!hms_internal_index(header, *data, entry_size, key, &index_)) {
        hms_internal_insert(header, *data, entry_size, key);
    }

    // new items are always inserted at the end of the table
    size_t inserted_index = header->size;
    byte *table = *data;
    byte *item = table + (inserted_index * entry_size);
    memcpy(item + sizeof(key), value, entry_size - sizeof(key));
}

static void *hms_entry_impl(Arena *a, HashmapHeader_ *header, void **data, size_t entry_size, String key) {
    TIME_FUNCTION;
    if (header->size >= header->capacity * HASHMAP_LOAD_FACTOR) {
        hms_grow(a, header, data, entry_size);
    }

    size_t index = 0;
    size_t actual_index = 0;
    if (!hms_internal_index(header, *data, entry_size, key, &index)) {
        hms_internal_insert(header, *data, entry_size, key);
        actual_index = header->size;
    } else {
        actual_index = header->entries[index].index;
    }

    // return a pointer to the value
    byte *table = *data;
    byte *item = table + (actual_index * entry_size);
    return item + sizeof(key);
}

// TODO: implement backshift erasure, currently this function is broken
// see test_small_hashmap_collision() for the bug
static void *hms_del_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;
    if (header->capacity == 0) return NULL;
    size_t i = 0;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return NULL;
    }

    // Swap-Remove the key-value pair in the table
    byte *table = data;
    byte *item = table + (header->entries[i].index * entry_size);
    byte *last = table + (header->size * entry_size);

    // Set the new index of the swapped value
    String last_str = *(String *)last;
    size_t last_i = 0;
    assert(hms_internal_index(header, data, entry_size, last_str, &last_i));
    header->entries[last_i].index = header->entries[i].index;

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
    size_t i = 0;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return -1;
    }
    return header->entries[i].index;
}

// Returns NULL if not found
static void *hms_get_value_null_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t i;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size) + sizeof(key);
}

// Returns NULL if not found
static void *hms_get_pair_null_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t i;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size);
}

// Returns the default value at index 0 if not found
static void *hms_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t i;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return data;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size);
}


#define MapStr(pair_type) struct {                  \
    union {                                         \
        struct {                                    \
            HASHMAP_HEADER;                         \
            pair_type *data;                        \
        };                                          \
        __typeof__(((pair_type){0}).value) *value_; \
        pair_type  *pair_;                          \
    };                                              \
}

// Insert a new key-value pair or update the value if it already exists
#define hms_put(arena, hashmap, key, value)                   \
    ((void)hms_put_impl((arena), (HashmapHeader_*)(hashmap),  \
        (void **)&(hashmap)->data, sizeof *((hashmap)->data), \
        (key), 1 ? &(value): (hashmap)->value_))

// Insert a new key and return a pointer to the value
#define hms_entry(arena, hashmap, key)                                                  \
    ((__typeof__((hashmap)->value_))hms_entry_impl((arena), (HashmapHeader_*)(hashmap), \
        (void **)&(hashmap)->data, sizeof *((hashmap)->data), (key)))

// Return a pointer to value if it exists, otherwise NULL
#define hms_get_ptr(hashmap, key)                                                       \
    ((__typeof__((hashmap)->value_))hms_get_value_null_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Return the value if it exists, otherwise the 0 value at index 0
#define hms_get(hashmap, key)                                                               \
    (*(__typeof__((hashmap)->value_))((byte *)hms_get_pair_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key))                                  \
            + sizeof(key)))

// Return a pointer to the key-value pair if it exists, otherwise NULL
#define hms_get_pair_ptr(hashmap, key)                                                \
    ((__typeof__((hashmap)->pair_))hms_get_pair_null_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Return a pointer to the key-value pair if it exists, otherwise the 0 value at index 0
#define hms_get_pair(hashmap, key)                                                \
    (*(__typeof__((hashmap)->pair_))hms_get_pair_impl((HashmapHeader_*)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hms_get_index(hashmap, key)                              \
    (hms_index_impl((HashmapHeader_*)(hashmap), (hashmap)->data, \
        sizeof (*(hashmap)->data), (key)))

// Returns true if key is present, false otherwise
#define hms_contains(hashmap, key)                               \
    (hms_index_impl((HashmapHeader_*)(hashmap), (hashmap)->data, \
        sizeof (*(hashmap)->data), (key)) != -1)

// Remove a key-value pair if it exists, and return it
#define hms_pop(hashmap, key)                                                 \
    (*(__typeof__((hashmap)->pair_))hms_del_impl((HashmapHeader_ *)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Remove a key-value pair if it exists, and return it
#define hms_delete(hashmap, key)                            \
    ((void)hms_del_impl((HashmapHeader_ *)(hashmap),        \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hm_foreach(hashmap, pair)                                   \
    for (__typeof__((hashmap)->pair_) (pair) = (hashmap)->data + 1; \
        pair <= (hashmap)->data + (hashmap)->size;                  \
        pair++)


#endif // MIGI_HASHMAP_H
