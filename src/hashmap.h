#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

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
    uint64_t index; // index of 0 means an entry is unoccupied
} HashEntry_;

// Need an anonymous struct for the actual header so that it can be embedded into the main hashmap struct
#define HASHMAP_HEADER struct {                                                                                  \
    HashEntry_ *entries;     /* entries in the hashmap table */                                                  \
    size_t size;             /* number of entries */                                                             \
    size_t capacity;         /* total allocated entries */                                                       \
    ptrdiff_t temp_index;    /* temporary storage for the data array index (used by macros for various stuff) */ \
}

// Typedef needed to cast a `hashmap *` to a `HashmapHeader_ *`
typedef HASHMAP_HEADER HashmapHeader_;


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
static void *hms_grow(Arena *a, HashmapHeader_ *header, void *data, size_t entry_size) {
    TIME_FUNCTION;
    size_t old_capacity = header->capacity;
    header->capacity = (header->capacity == 0)? HASHMAP_INIT_CAP: header->capacity*2;
    HashEntry_ *new_entries = arena_push(a, HashEntry_, header->capacity);

    // allocating an extra item for index 0 being treated as the default index
    byte *new_data = arena_push_bytes(a, entry_size * (header->capacity + 1), _Alignof(byte));

    migi_mem_clear(new_entries, header->capacity);
    migi_mem_clear(new_data, entry_size * (header->capacity + 1));

    if (data) {
        memcpy(new_data, data, entry_size * (header->size + 1));
    }

    HashEntry_ *old_entries = header->entries;
    header->entries = new_entries;
    for (size_t j = 0; j < old_capacity; j++) {
        if (old_entries[j].index != 0) {
            hms_internal_insert_entry(header, old_entries[j]);
        }
    }
    return new_data;
}

// Indexes into the data array of the hashmap
static inline byte *hms_internal_data_index(byte *table_data, size_t entry_size, size_t index) {
    return table_data + (index * entry_size);
}

// Returns the index of key in the hashmap entries
// Returns false if key doesnt exist
// `entry_index` is not modified if false was returned
static bool hms_internal_index(HashmapHeader_ *header, void *data, size_t entry_size, String key, size_t *entry_index) {
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
            *entry_index = i;
            return true;
        };

        size_t cur_desired = header->entries[i].hash & (header->capacity - 1);
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);
        if (cur_dist < dist) return false;

        dist++;
        i = (i + 1) & (header->capacity - 1);
    }
}

static void *hms_put_impl(Arena *a, HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    TIME_FUNCTION;
    void *new_data = data;
    if (header->size >= (size_t)(header->capacity * HASHMAP_LOAD_FACTOR)) {
        new_data = hms_grow(a, header, data, entry_size);
    }

    size_t index = 0;
    if (!hms_internal_index(header, data, entry_size, key, &index)) {
        uint64_t hash = hash_fnv((byte *)key.data, key.length);
        // new items are always inserted at the end of the table
        header->size++;
        hms_internal_insert_entry(header, (HashEntry_){ .hash = hash, .index = header->size });
        header->temp_index = header->size;
    } else {
        header->temp_index = header->entries[index].index;
    }
    return new_data;
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
    if (header->capacity == 0) {
        header->temp_index = 0;
        return;
    };
    size_t entry = 0;
    if (!hms_internal_index(header, data, entry_size, key, &entry)) {
        header->temp_index = 0;
        return;
    }

    // Update the entry of the last key in the hashmap data array to its new index
    String last_key = *((String*)hms_internal_data_index(data, entry_size, header->size));
    size_t last_entry = 0;
    bool exists = hms_internal_index(header, data, entry_size, last_key, &last_entry);
    assertf(exists, "hashmap should always have a last entry");

    header->entries[last_entry].index = header->entries[entry].index;
    header->temp_index = header->entries[entry].index;
    assertf(header->temp_index != 0, "nothing can map to the 0 value of the data array");

    header->size--;
    hms_internal_erase(header, entry);
}

// Returns the default value at index 0 if not found
static void hms_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    size_t entry;
    if (hms_internal_index(header, data, entry_size, key, &entry)) {
        header->temp_index = header->entries[entry].index;
    } else {
        header->temp_index = 0;
    }
}


// Insert a new key-value pair or update the value if it already exists
#define hms_put(arena, hashmap, k, v)                                    \
    ((hashmap)->data = hms_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]), (k)),       \
        (hashmap)->data[(hashmap)->temp_index].key = (k),                \
        (hashmap)->data[(hashmap)->temp_index].value = (v))


// Insert a new key-value pair or update the value if it already exists
#define hms_put_pair(arena, hashmap, pair)                                \
    ((hashmap)->data = hms_put_impl((arena), (HashmapHeader_*)(hashmap),  \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]), (pair).key), \
        (hashmap)->data[(hashmap)->temp_index] = (pair))                  \

// Insert a new key and return a pointer to the value
#define hms_entry(arena, hashmap, k)                                     \
    ((hashmap)->data = hms_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]), (k)),       \
        (hashmap)->data[(hashmap)->temp_index].key = (k),                \
        &((hashmap)->data[(hashmap)->temp_index].value))


// Return a pointer to value if it exists, otherwise NULL
#define hms_get_ptr(hashmap, key)                           \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),          \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)), \
    (hashmap)->temp_index == 0                              \
        ? NULL                                              \
        : &((hashmap)->data[(hashmap)->temp_index]).value)

// Return the value if it exists, otherwise the default value at index 0
#define hms_get(hashmap, key)                               \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),          \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)), \
    (hashmap)->data[(hashmap)->temp_index].value)


// Return a pointer to the key-value pair if it exists, otherwise NULL
#define hms_get_pair_ptr(hashmap, key)                      \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),          \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)), \
    (hashmap)->temp_index == 0                              \
        ? NULL                                              \
        : &(hashmap)->data[(hashmap)->temp_index])

// Return a pointer to the key-value pair if it exists, otherwise
// the default key-value pair at index 0
#define hms_get_pair(hashmap, key)                          \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),          \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)), \
    (hashmap)->data[(hashmap)->temp_index])

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hms_get_index(hashmap, key)                         \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),          \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)), \
    (hashmap)->temp_index == 0                              \
        ? -1                                                \
        : (hashmap)->temp_index)

// Returns true if key is present, false otherwise
#define hms_contains(hashmap, key)                               \
    (hms_get_pair_impl((HashmapHeader_*)(hashmap),               \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)),      \
    (hashmap)->temp_index != 0)

// Remove a key-value pair if it exists, and returns it, otherwise the default pair
#define hms_pop(hashmap, key)                                                           \
     (hms_delete_impl((HashmapHeader_ *)(hashmap),                                      \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)),                             \
    (hashmap)->temp_index == 0                                                          \
      ? (hashmap)->data[0]                                                              \
      : ((hashmap)->data[(hashmap)->size + 2] = (hashmap)->data[(hashmap)->temp_index], \
        (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1],  \
        (hashmap)->data[(hashmap)->size + 2]))

// Remove a key-value pair if it exists, and return it
#define hms_delete(hashmap, key)                                                      \
     ((void)(hms_delete_impl((HashmapHeader_ *)(hashmap),                             \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)),                           \
    (hashmap)->temp_index != 0                                                        \
      ? (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1] \
      : (void)0))

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hm_foreach(hashmap, pair)                                  \
    for (__typeof__((hashmap)->data) (pair) = (hashmap)->data + 1; \
        pair <= (hashmap)->data + (hashmap)->size;                 \
        pair++)


#endif // MIGI_HASHMAP_H
