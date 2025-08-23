#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

// NOTE: HASHMAP_INIT_CAP *must* always be a power of two or bad things will happen
#ifndef HASHMAP_INIT_CAP
   #define HASHMAP_INIT_CAP 256
#endif

// Any number in the range (0, 1.0) (both exclusive) should work
// A load factor of 1.0 is not supported as atleast 1 empty spot must
// exist at the end, for hashmap_pop to use it as a temporary variable
// for swap-removal
#ifndef HASHMAP_LOAD_FACTOR
   #define HASHMAP_LOAD_FACTOR 0.75
#endif

// Index of default key value pair in the table
#define HASHMAP_DEFAULT_INDEX 0

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
    void *temp_values;       /* temporary storage for the value array index (used by macros for various stuff) */ \
}

// Typedef needed to cast a `hashmap *` to a `HashmapHeader_ *`
typedef HASHMAP_HEADER HashmapHeader_;

// Optional tracking of maximum probe length for statistics
#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    static size_t _hashmap_max_probe_length = 0;
    static size_t _hashmap_probes = 0;
#endif

// Uses robin hood linear probing to insert an entry
static void hm_internal_insert_entry(HashmapHeader_ *header, HashEntry_ entry) {
    TIME_FUNCTION;
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
// if `at_least` == 0, then the capacity is simply doubled
static void *hm_grow(Arena *a, HashmapHeader_ *header, void *keys, size_t key_size, size_t key_align,
                    void *values, size_t value_size, size_t value_align, size_t at_least) {
    TIME_FUNCTION;
    size_t old_capacity = header->capacity;

    // grow the required amount
    if (at_least > 0) {
        size_t required_capacity = at_least * (1 + HASHMAP_LOAD_FACTOR);
        if (header->capacity == 0 && HASHMAP_INIT_CAP >= required_capacity) {
            header->capacity = HASHMAP_INIT_CAP;
        } else {
            header->capacity = next_power_of_two(required_capacity);
        }
    } else {
        header->capacity = (header->capacity == 0)? HASHMAP_INIT_CAP: header->capacity * 2;
    }

    HashEntry_ *new_entries = arena_push(a, HashEntry_, header->capacity);
    mem_clear(new_entries, header->capacity);

    // allocating an extra item for index 0 being treated as the default index
    size_t keys_size       = key_size   * (old_capacity + 1);
    size_t values_size     = value_size * (old_capacity + 1);
    size_t new_keys_size   = key_size   * (header->capacity + 1);
    size_t new_values_size = value_size * (header->capacity + 1);

    void *new_keys = arena_realloc_bytes(a, keys, keys_size, new_keys_size, key_align);
    header->temp_values = arena_realloc_bytes(a, values, values_size, new_values_size, value_align);

    HashEntry_ *old_entries = header->entries;
    header->entries = new_entries;
    for (size_t j = 0; j < old_capacity; j++) {
        if (old_entries[j].index != 0) {
            hm_internal_insert_entry(header, old_entries[j]);
        }
    }
    return new_keys;
}

// Backshift Erasure
// Move elements back until theres an empty entry or the entry is already
// in its desired (best possible) position
static void hm_internal_erase(HashmapHeader_ *header, size_t start) {
    TIME_FUNCTION;
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

typedef enum {
    HashmapKey_String,
    HashmapKey_Other
} HashmapKeyType;

typedef struct {
    uint64_t hash;
    size_t entry_index;
    bool is_present;
} HashmapItem;

static HashmapItem hm_internal_index(HashmapHeader_ *header, void *keys, size_t key_size,
                                      void *search_key, HashmapKeyType key_type) {
    TIME_FUNCTION;
    HashmapItem result = {0};
    if (key_type == HashmapKey_String) {
        String *search_key_str = (String *)search_key;
        result.hash = hash_fnv((byte *)search_key_str->data, search_key_str->length);
    } else if (key_type == HashmapKey_Other) {
        result.hash = hash_fnv((byte *)search_key, key_size);
    } else {
        migi_unreachable();
    }
    size_t i = result.hash & (header->capacity - 1);
    size_t dist = 0;

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    _hashmap_probes = 0;
#endif

    while (true) {
        if (header->entries[i].index == 0) {
            result.is_present = false;
            break;
        }

        // return if key was found
        byte *map_key = (byte *)keys + (header->entries[i].index * key_size);
        if (key_type == HashmapKey_String) {
            String map_key_str = *(String *)map_key;
            String search_key_str = *(String*)search_key;
            if (string_eq(search_key_str, map_key_str)) {
                result.is_present = true;
                result.entry_index = i;
                break;
            }
        } else if (key_type == HashmapKey_Other) {
            if (mem_eq((byte *)search_key, map_key, key_size)) {
                result.is_present = true;
                result.entry_index = i;
                break;
            }
        } else {
            migi_unreachable();
        }
        size_t cur_desired = header->entries[i].hash & (header->capacity - 1);
        size_t cur_dist = (i + header->capacity - cur_desired) & (header->capacity - 1);
        if (cur_dist < dist) {
            result.is_present = false;
        }

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
        _hashmap_probes++;
        _hashmap_max_probe_length = max(_hashmap_max_probe_length, _hashmap_probes);
#endif
        dist++;
        i = (i + 1) & (header->capacity - 1);
    }
    return result;
}

static void *hm_put_impl(Arena *a, HashmapHeader_ *header,
                        void *keys, size_t key_size, size_t key_align,
                        void *values, size_t value_size, size_t value_align,
                        void *new_key, HashmapKeyType key_type) {
    TIME_FUNCTION;
    void *new_keys = keys;
    if (header->size >= (size_t)(header->capacity * HASHMAP_LOAD_FACTOR)) {
        new_keys = hm_grow(a, header, keys, key_size, key_align, values, value_size, value_align, 0);
    }

    HashmapItem item = hm_internal_index(header, keys, key_size, new_key, key_type);
    if (!item.is_present) {
        // new items are always inserted at the end of the table
        header->size++;
        hm_internal_insert_entry(header, (HashEntry_){ .hash = item.hash, .index = header->size });
        header->temp_index = header->size;
    } else {
        header->temp_index = header->entries[item.entry_index].index;
    }
    return new_keys;
}

static void hm_delete_impl(HashmapHeader_ *header, void *keys, size_t key_size, void *delete_key, HashmapKeyType key_type) {
    TIME_FUNCTION;
    if (header->capacity == 0) {
        header->temp_index = 0;
        return;
    };
    HashmapItem item = hm_internal_index(header, keys, key_size, delete_key, key_type);
    if (!item.is_present) {
        header->temp_index = 0;
        return;
    }

    // Update the entry of the last key in the hashmap data array to its new index
    byte *last_key = (byte *)keys + (header->size * key_size);
    HashmapItem last_item = hm_internal_index(header, keys, key_size, last_key, key_type);
    assertf(last_item.is_present, "hashmap should always have a last entry");

    header->entries[last_item.entry_index].index = header->entries[item.entry_index].index;
    header->temp_index = header->entries[item.entry_index].index;
    assertf(header->temp_index != 0, "nothing can map to the 0 value of the data array");

    header->size--;
    hm_internal_erase(header, item.entry_index);
}

// Returns the default value at index 0 if not found
static void hms_get_impl(HashmapHeader_ *header, void *keys, size_t key_size, void *search_key, HashmapKeyType key_type) {
    HashmapItem item = hm_internal_index(header, keys, key_size, search_key, key_type);
    if (item.is_present) {
        header->temp_index = header->entries[item.entry_index].index;
    } else {
        header->temp_index = 0;
    }
}

// array with a single element that decays to a pointer
#define addr_of(x) ((__typeof__(x)[1]){x})

#define _hashmap_key_type(k)        \
    _Generic((k),                   \
        String:  HashmapKey_String, \
        default: HashmapKey_Other)

// Reserve space for insertion of `count` elements into the hashmap without growing
#define hashmap_reserve(arena, hashmap, count)                                                               \
    ((hashmap)->keys = hm_grow((arena), (HashmapHeader_*)(hashmap),                                          \
        (void *)(hashmap)->keys, sizeof((hashmap)->keys[0]), _Alignof(__typeof__((hashmap)->keys[0])),       \
        (void *)(hashmap)->values, sizeof((hashmap)->values[0]), _Alignof(__typeof__((hashmap)->values[0])), \
        (count)),                                                                                            \
    (hashmap)->values = (hashmap)->temp_values)

// Insert a new key-value pair or update the value if it already exists
#define hashmap_put(arena, hashmap, k, v)                                                                    \
    ((hashmap)->keys = hm_put_impl((arena), (HashmapHeader_*)(hashmap),                                      \
        (void *)(hashmap)->keys, sizeof((hashmap)->keys[0]), _Alignof(__typeof__((hashmap)->keys[0])),       \
        (void *)(hashmap)->values, sizeof((hashmap)->values[0]), _Alignof(__typeof__((hashmap)->values[0])), \
        addr_of((k)), _hashmap_key_type((k))),                                                               \
    (hashmap)->values = (hashmap)->temp_values,                                                              \
    (hashmap)->keys[(hashmap)->temp_index] = (k),                                                            \
    (hashmap)->values[(hashmap)->temp_index] = (v))


// Insert a new key and return a pointer to the value
#define hashmap_entry(arena, hashmap, k)                                                                     \
    ((hashmap)->keys = hm_put_impl((arena), (HashmapHeader_*)(hashmap),                                      \
        (void *)(hashmap)->keys, sizeof((hashmap)->keys[0]), _Alignof(__typeof__((hashmap)->keys[0])),       \
        (void *)(hashmap)->values, sizeof((hashmap)->values[0]), _Alignof(__typeof__((hashmap)->values[0])), \
        addr_of((k)), _hashmap_key_type((k))),                                                              \
    (hashmap)->values = (hashmap)->temp_values,                                                              \
    (hashmap)->keys[(hashmap)->temp_index] = (k),                                                            \
    &((hashmap)->values[(hashmap)->temp_index]))

// Return a pointer to value if it exists, otherwise NULL
#define hashmap_get_ptr(hashmap, k)                          \
    (hms_get_impl((HashmapHeader_*)(hashmap),                \
        (void *)(hashmap)->keys, sizeof((hashmap)->keys[0]), \
        addr_of((k)), _hashmap_key_type((k))),               \
    (hashmap)->temp_index == 0                               \
        ? NULL                                               \
        : &((hashmap)->values[(hashmap)->temp_index]))

// Return the value if it exists, otherwise the default value at index 0
#define hashmap_get(hashmap, k)                       \
    (hms_get_impl((HashmapHeader_*)(hashmap),         \
        (hashmap)->keys, sizeof ((hashmap)->keys[0]), \
        addr_of((k)), _hashmap_key_type((k))),        \
    (hashmap)->values[(hashmap)->temp_index])

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hashmap_get_index(hashmap, k)                 \
    (hms_get_impl((HashmapHeader_*)(hashmap),         \
        (hashmap)->keys, sizeof ((hashmap)->keys[0]), \
        addr_of((k)), _hashmap_key_type((k))),              \
    (hashmap)->temp_index == 0                        \
        ? -1                                          \
        : (hashmap)->temp_index)

// Remove a key-value pair if it exists, and returns it, otherwise the default pair
#define hashmap_pop(hashmap, k)                                                             \
     (hm_delete_impl((HashmapHeader_ *)(hashmap),                                           \
        (hashmap)->keys, sizeof((hashmap)->keys[0]), addr_of((k)), _hashmap_key_type((k))), \
    (hashmap)->temp_index == 0                                                              \
      ? (hashmap)->values[0]                                                                \
      : ((hashmap)->values[(hashmap)->size + 2] = (hashmap)->values[(hashmap)->temp_index], \
        (hashmap)->keys[(hashmap)->temp_index] = (hashmap)->keys[(hashmap)->size + 1],      \
        (hashmap)->values[(hashmap)->temp_index] = (hashmap)->values[(hashmap)->size + 1],  \
        (hashmap)->values[(hashmap)->size + 2]))


#define hashmap_free(hashmap) (mem_clear_single((hashmap)))

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hashmap_foreach(hashmap, pair)                               \
    for (struct { __typeof__((hashmap)->keys) key;                   \
                  __typeof__((hashmap)->values) value; }             \
            (pair) = { (hashmap)->keys + 1, (hashmap)->values + 1 }; \
        (pair).key <= (hashmap)->keys + (hashmap)->size;             \
        (pair).key++, (pair).value++)

#endif // MIGI_HASHMAP_H
