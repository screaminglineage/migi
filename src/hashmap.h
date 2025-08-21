#ifndef MIGI_HASHMAP_H
#define MIGI_HASHMAP_H

// TODO: try out an architecture where the keys and values are separate arrays
//
// Declared like:
// typedef struct {
//   HASHMAP_HEADER;
//   Key *keys;
//   Value *values;
// } MapKeyValue;
//
// Any macro returning Pair will have to pass in the type to fill in
// In this way, the pair struct will only need to be created if really needed
// typedef struct {
//    Key key;
//    Value value;
// } PairType;
// PairType p = get_pair(&map, PairType, key);
//
// This should work too
// hashmap_foreach(&map, key, value) {
//    printf(key);
//    printf(value);
// }
//
// Also fixes the issue of expecting that key always has to come first
// Also makes it possible to allocate the values separately if they are really large
//
// TODO: use a different hash function when working with non-strings

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

// Optional tracking of maximum probe length for statistics
#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    static size_t _hashmap_max_probe_length = 0;
    static size_t _hashmap_probes = 0;
#endif

// Uses robin hood linear probing to insert an entry
static void hm_internal_insert_entry(HashmapHeader_ *header, HashEntry_ entry) {
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
static void *hm_grow(Arena *a, HashmapHeader_ *header, void *data, size_t entry_size) {
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
            hm_internal_insert_entry(header, old_entries[j]);
        }
    }
    return new_data;
}

// Backshift Erasure
// Move elements back until theres an empty entry or the entry is already
// in its desired (best possible) position
// TODO: check if this can be done with a memmove instead
// the only issue would be when the shift wraps back to the start from the end
static void hm_internal_erase(HashmapHeader_ *header, size_t start) {
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
    Key_String,
    Key_Other
} HashmapKeyType;

typedef struct {
    uint64_t hash;
    size_t entry_index;
    bool is_present;
} HashmapItem;

static HashmapItem hm_internal_index(HashmapHeader_ *header, void *data, size_t entry_size,
                                      void *key, size_t key_size, HashmapKeyType key_type) {
    TIME_FUNCTION;
    HashmapItem result = {0};
    result.hash = hash_fnv((byte *)key, key_size);
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
        void *map_key = data + (header->entries[i].index * entry_size);
        if (key_type == Key_String) {
            String map_key_str = *(String *)map_key;
            String new_key = (String){.data = key, .length = key_size};
            if (string_eq(new_key, map_key_str)) {
                result.is_present = true;
                result.entry_index = i;
                break;
            }
        } else if (key_type == Key_Other) {
            if (migi_mem_eq(key, map_key, key_size)) {
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

static void *hm_put_impl(Arena *a, HashmapHeader_ *header, void *data, size_t entry_size, void *key, size_t key_size, HashmapKeyType key_type) {
    TIME_FUNCTION;
    void *new_data = data;
    if (header->size >= (size_t)(header->capacity * HASHMAP_LOAD_FACTOR)) {
        new_data = hm_grow(a, header, data, entry_size);
    }

    HashmapItem item = hm_internal_index(header, data, entry_size, key, key_size, key_type);
    if (!item.is_present) {
        // new items are always inserted at the end of the table
        header->size++;
        hm_internal_insert_entry(header, (HashEntry_){ .hash = item.hash, .index = header->size });
        header->temp_index = header->size;
    } else {
        header->temp_index = header->entries[item.entry_index].index;
    }
    return new_data;
}

static void hm_delete_impl(HashmapHeader_ *header, void *data, size_t entry_size, void *key, size_t key_size, HashmapKeyType key_type) {
    TIME_FUNCTION;
    if (header->capacity == 0) {
        header->temp_index = 0;
        return;
    };
    HashmapItem item = hm_internal_index(header, data, entry_size, key, key_size, key_type);
    if (!item.is_present) {
        header->temp_index = 0;
        return;
    }

    // Update the entry of the last key in the hashmap data array to its new index
    void *last_key = data + (header->size * entry_size);
    HashmapItem last_item = {0};
    if (key_type == Key_String) {
        String *last_key_str = (String *)last_key;
        last_item = hm_internal_index(header, data, entry_size, (void *)last_key_str->data, key_size, key_type);
    } else if (key_type == Key_Other) {
        last_item = hm_internal_index(header, data, entry_size, last_key, key_size, key_type);
    } else {
        migi_unreachable();
    }
    assertf(last_item.is_present, "hashmap should always have a last entry");

    header->entries[last_item.entry_index].index = header->entries[item.entry_index].index;
    header->temp_index = header->entries[item.entry_index].index;
    assertf(header->temp_index != 0, "nothing can map to the 0 value of the data array");

    header->size--;
    hm_internal_erase(header, item.entry_index);
}

// Returns the default value at index 0 if not found
static void hm_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, void *key, size_t key_size, HashmapKeyType key_type) {
    HashmapItem item = hm_internal_index(header, data, entry_size, key, key_size, key_type);
    if (item.is_present) {
        header->temp_index = header->entries[item.entry_index].index;
    } else {
        header->temp_index = 0;
    }
}

// array with a single element that decays to a pointer
#define addr_of(x) ((__typeof__(x)[1]){x})


// Insert a new key-value pair or update the value if it already exists
#define hms_put(arena, hashmap, k, v)                                   \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]),            \
        (void *)(k).data, (k).length, Key_String),                      \
        (hashmap)->data[(hashmap)->temp_index].key = (k),               \
        (hashmap)->data[(hashmap)->temp_index].value = (v))

// Insert a new key-value pair or update the value if it already exists
#define hm_put(arena, hashmap, k, v)                                    \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]),            \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other),         \
    (hashmap)->data[(hashmap)->temp_index].key = (k),                   \
    (hashmap)->data[(hashmap)->temp_index].value = (v))


// Insert a new key-value pair or update the value if it already exists
#define hms_put_pair(arena, hashmap, pair)                              \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]),            \
        (void *)(pair).key.data, (pair).key.length, Key_String),        \
        (hashmap)->data[(hashmap)->temp_index] = (pair))                \

// Insert a new key-value pair or update the value if it already exists
#define hm_put_pair(arena, hashmap, pair)                                                                 \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap),                                   \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]), &(pair).key, sizeof((pair).key), Key_Other), \
        (hashmap)->data[(hashmap)->temp_index] = (pair))                                                  \

// Insert a new key and return a pointer to the value
#define hms_entry(arena, hashmap, k)                                    \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]),            \
        (void *)(k).data, (k).length, Key_String),                      \
        (hashmap)->data[(hashmap)->temp_index].key = (k),               \
        &((hashmap)->data[(hashmap)->temp_index].value))

// Insert a new key and return a pointer to the value
#define hm_entry(arena, hashmap, k)                                     \
    ((hashmap)->data = hm_put_impl((arena), (HashmapHeader_*)(hashmap), \
        (void *)(hashmap)->data, sizeof((hashmap)->data[0]),            \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other),                \
        (hashmap)->data[(hashmap)->temp_index].key = (k),               \
        &((hashmap)->data[(hashmap)->temp_index].value))


// Return a pointer to value if it exists, otherwise NULL
#define hms_get_ptr(hashmap, key)                        \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),        \
        (hashmap)->data, sizeof (*(hashmap)->data),      \
        (void *)(key).data, (key).length, Key_String  ), \
    (hashmap)->temp_index == 0                           \
        ? NULL                                           \
        : &((hashmap)->data[(hashmap)->temp_index]).value)

// Return a pointer to value if it exists, otherwise NULL
#define hm_get_ptr(hashmap, k)                                  \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),               \
        (hashmap)->data, sizeof (*(hashmap)->data),             \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->temp_index == 0                                  \
        ? NULL                                                  \
        : &((hashmap)->data[(hashmap)->temp_index]).value)

// Return the value if it exists, otherwise the default value at index 0
#define hms_get(hashmap, key)                          \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),      \
        (hashmap)->data, sizeof (*(hashmap)->data),    \
        (void *)(key).data, (key).length, Key_String), \
    (hashmap)->data[(hashmap)->temp_index].value)

// Return the value if it exists, otherwise the default value at index 0
#define hm_get(hashmap, k)                                      \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),               \
        (hashmap)->data, sizeof (*(hashmap)->data),             \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->data[(hashmap)->temp_index].value)


// Return a pointer to the key-value pair if it exists, otherwise NULL
#define hms_get_pair_ptr(hashmap, key)                 \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),      \
        (hashmap)->data, sizeof (*(hashmap)->data),    \
        (void *)(key).data, (key).length, Key_String), \
    (hashmap)->temp_index == 0                         \
        ? NULL                                         \
        : &(hashmap)->data[(hashmap)->temp_index])

// Return a pointer to the key-value pair if it exists, otherwise NULL
#define hm_get_pair_ptr(hashmap, k)                           \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),             \
        (hashmap)->data, sizeof (*(hashmap)->data),           \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->temp_index == 0                                \
        ? NULL                                                \
        : &(hashmap)->data[(hashmap)->temp_index])

// Return a pointer to the key-value pair if it exists, otherwise
// the default key-value pair at index 0
#define hms_get_pair(hashmap, key)                     \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),      \
        (hashmap)->data, sizeof (*(hashmap)->data),    \
        (void *)(key).data, (key).length, Key_String), \
    (hashmap)->data[(hashmap)->temp_index])

// Return a pointer to the key-value pair if it exists, otherwise
// the default key-value pair at index 0
#define hm_get_pair(hashmap, k)                            \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),            \
        (hashmap)->data, sizeof (*(hashmap)->data),          \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->data[(hashmap)->temp_index])

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hms_get_index(hashmap, key)                    \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),      \
        (hashmap)->data, sizeof (*(hashmap)->data),    \
        (void *)(key).data, (key).length, Key_String), \
    (hashmap)->temp_index == 0                         \
        ? -1                                           \
        : (hashmap)->temp_index)

// Return the index of the key-value pair in the table if it exists, otherwise -1
#define hm_get_index(hashmap, k)                                \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),               \
        (hashmap)->data, sizeof (*(hashmap)->data),             \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->temp_index == 0                                  \
        ? -1                                                    \
        : (hashmap)->temp_index)

// Returns true if key is present, false otherwise
#define hms_contains(hashmap, key)                     \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),      \
        (hashmap)->data, sizeof (*(hashmap)->data),    \
        (void *)(key).data, (key).length, Key_String), \
    (hashmap)->temp_index != 0)

// Returns true if key is present, false otherwise
#define hm_contains(hashmap, k)                                 \
    (hm_get_pair_impl((HashmapHeader_*)(hashmap),               \
        (hashmap)->data, sizeof (*(hashmap)->data),             \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->temp_index != 0)

// Remove a key-value pair if it exists, and returns it, otherwise the default pair
#define hms_pop(hashmap, key)                                                                      \
     (hm_delete_impl((HashmapHeader_ *)(hashmap),                                                  \
        (hashmap)->data, sizeof (*(hashmap)->data), (void *)(key).data, (key).length, Key_String), \
    (hashmap)->temp_index == 0                                                                     \
      ? (hashmap)->data[0]                                                                         \
      : ((hashmap)->data[(hashmap)->size + 2] = (hashmap)->data[(hashmap)->temp_index],            \
        (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1],             \
        (hashmap)->data[(hashmap)->size + 2]))

// Remove a key-value pair if it exists, and returns it, otherwise the default pair
#define hm_pop(hashmap, k)                                                                                  \
     (hm_delete_impl((HashmapHeader_ *)(hashmap),                                                           \
        (hashmap)->data, sizeof (*(hashmap)->data), addr_of((k)), sizeof((hashmap)->data->key), Key_Other), \
    (hashmap)->temp_index == 0                                                                              \
      ? (hashmap)->data[0]                                                                                  \
      : ((hashmap)->data[(hashmap)->size + 2] = (hashmap)->data[(hashmap)->temp_index],                     \
        (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1],                      \
        (hashmap)->data[(hashmap)->size + 2]))

// Remove a key-value pair if it exists, and return it
#define hms_delete(hashmap, key)                                                                   \
     ((void)(hm_delete_impl((HashmapHeader_ *)(hashmap),                                           \
        (hashmap)->data, sizeof (*(hashmap)->data), (void *)(key).data, (key).length, Key_String), \
    (hashmap)->temp_index != 0                                                                     \
      ? (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1]              \
      : (void)0))

// Remove a key-value pair if it exists, and return it
#define hm_delete(hashmap, k)                                                         \
     ((void)(hm_delete_impl((HashmapHeader_ *)(hashmap),                              \
        (hashmap)->data, sizeof (*(hashmap)->data),                                   \
        addr_of((k)), sizeof((hashmap)->data->key), Key_Other),                       \
    (hashmap)->temp_index != 0                                                        \
      ? (hashmap)->data[(hashmap)->temp_index] = (hashmap)->data[(hashmap)->size + 1] \
      : (void)0))


#define hm_free(hashmap)                               \
    ((hashmap)->size = 0, (hashmap)->capacity = 0,     \
    (hashmap)->temp_index = 0, (hashmap)->entries = 0, \
    (hashmap)->data = 0)

// Iterate over the hashmap by reference
// Use in a similar manner to array_foreach
#define hm_foreach(hashmap, pair)                                  \
    for (__typeof__((hashmap)->data) (pair) = (hashmap)->data + 1; \
        pair <= (hashmap)->data + (hashmap)->size;                 \
        pair++)


#endif // MIGI_HASHMAP_H
