#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
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


// Uses robin hood linear probing to insert an entry
static void hms_internal_insert_entry(HashmapHeader_ *header, HashEntry_ entry) {
    size_t i = entry.hash & (header->capacity - 1);
    size_t dist = 0;
    while (header->entries[i].index != 0) {
        size_t cur_desired = header->entries[i].hash & (header->capacity - 1);
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);

        if (cur_dist < dist) {
            migi_swap(entry, header->entries[i]);
            dist = cur_dist;
        }

        dist++;
        i = (i + 1) & (header->capacity - 1);
    }
    header->entries[i] = entry;
}


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

    HashEntry_ *old_entries = header->entries;
    header->entries = new_entries;
    for (size_t j = 0; j < old_capacity; j++) {
        if (old_entries[j].index != 0) {
            hms_internal_insert_entry(header, old_entries[j]);
        }
    }
}

static inline byte *hms_internal_data_index(byte *table, size_t entry_size, size_t index) {
    return table + (index * entry_size);
}


// Inserts key assuming that it doesnt already exist
// WARNING: Always call contains/get before calling this
static void hms_internal_insert(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;

    // inserting key data at the end
    header->size++;
    size_t data_index = header->size;
    byte *item = hms_internal_data_index(data, entry_size, data_index);
    memcpy(item, &key, sizeof(key));

    uint64_t hash = hash_fnv((byte *)key.data, key.length);
    hms_internal_insert_entry(header, (HashEntry_){ .hash = hash, .index = data_index });
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

        String map_key = *(String *)hms_internal_data_index(data, entry_size, header->entries[i].index);

        // return if key was found
        if (string_eq(key, map_key)) {
            // TODO: maybe return header->entries[i].index instead?
            // most functions only need that except for possibly hms_del_impl
            *index = i;
            return true;
        };

        size_t cur_desired = header->entries[i].hash & (header->capacity - 1);
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

    size_t index = 0;
    size_t data_index = 0;
    if (!hms_internal_index(header, *data, entry_size, key, &index)) {
        hms_internal_insert(header, *data, entry_size, key);
        // new items are always inserted at the end of the table
        data_index = header->size;
    } else {
        data_index = header->entries[index].index;
    }
    byte *item = hms_internal_data_index(*data, entry_size, data_index);
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

    // return a pointer to the value by skipping the key
    return hms_internal_data_index(*data, entry_size, actual_index) + sizeof(key);
}

// Backshift Erasure
// Move elements back until theres an empty entry or the entry is already
// in its desired (best possible) position
// TODO: check if this can be done with a memmove instead
// the only issue would be when the shift wraps back to the start from the end
static void hms_internal_erase(HashmapHeader_ *header, size_t start) {
    size_t current = start;
    while (true) {
        header->entries[current].index = 0;
        size_t next = (current + 1) & (header->capacity - 1);

        HashEntry_ next_entry = header->entries[next];
        size_t next_desired = next_entry.hash & (header->capacity - 1);
        if (next_entry.index == 0) break;
        if (next_desired == next) break;

        header->entries[current] = header->entries[next];
        current = next;
    }
}

static void hms_delete_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;
    if (header->capacity == 0) return;
    size_t i = 0;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return;
    }

    // Swap-Remove the key-value pair in the table
    byte *last = hms_internal_data_index(data, entry_size, header->size);
    String last_key = *(String *)last;

    // If key is the same as last_key no swapping is needed
    // NOTE: memcpy also cannot be used on overlapping regions
    if (!string_eq(last_key, key)) {
        size_t last_i = 0;
        assert(hms_internal_index(header, data, entry_size, last_key, &last_i));
        // Set the new index of the swapped value
        header->entries[last_i].index = header->entries[i].index;

        byte *item = hms_internal_data_index(data, entry_size, header->entries[i].index);
        memcpy(item, last, entry_size);
    }

    header->size--;
    hms_internal_erase(header, i);
}


static void *hms_pop_impl(Arena *a, HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;
    if (header->capacity == 0) return NULL;
    size_t i = 0;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return NULL;
    }

    // Swap-Remove the key-value pair in the table
    byte *last = hms_internal_data_index(data, entry_size, header->size);
    String last_key = *(String *)last;

    byte *popped = NULL;
    // If key is the same as last_key no swapping is needed
    if (!string_eq(last_key, key)) {
        size_t last_i = 0;
        assert(hms_internal_index(header, data, entry_size, last_key, &last_i));
        // Set the new index of the swapped value
        header->entries[last_i].index = header->entries[i].index;

        byte *item = hms_internal_data_index(data, entry_size, header->entries[i].index);
        popped = arena_memdup_bytes(a, item, entry_size, _Alignof(byte));
        memcpy(item, last, entry_size);
    } else {
        popped = arena_memdup_bytes(a, last, entry_size, _Alignof(byte));
    }

    header->size--;
    hms_internal_erase(header, i);
    return popped;
}

// Returns -1 if not found
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

    // return a pointer to the value by skipping the key
    return hms_internal_data_index(data, entry_size, header->entries[i].index) + sizeof(key);
}

// Returns NULL if not found
static void *hms_get_pair_null_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t i;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return NULL;
    }

    // return a pointer to the pair
    return hms_internal_data_index(data, entry_size, header->entries[i].index);
}

// Returns the default value at index 0 if not found
static void *hms_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t i;
    if (!hms_internal_index(header, data, entry_size, key, &i)) {
        return data;
    }

    return hms_internal_data_index(data, entry_size, header->entries[i].index);
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
#define hms_pop(arena, hashmap, key)                                                   \
    (*(__typeof__((hashmap)->pair_))hms_pop_impl((arena), (HashmapHeader_ *)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Remove a key-value pair if it exists, and return it
#define hms_delete(hashmap, key)                  \
    (hms_delete_impl((HashmapHeader_ *)(hashmap), \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hm_foreach(hashmap, pair)                                   \
    for (__typeof__((hashmap)->pair_) (pair) = (hashmap)->data + 1; \
        pair <= (hashmap)->data + (hashmap)->size;                  \
        pair++)


#endif // MIGI_HASHMAP_H
