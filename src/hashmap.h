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
    uint64_t hash;  // TODO: see why storing the hash here is needed
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
#if 0
void hashmap_rehash(Arena *a, HashMap *hm) {
    size_t old_capacity = (hm)->capacity;
    (hm)->capacity = ((hm)->capacity == 0)? HASHMAP_INIT_CAP: (hm)->capacity*2;
    HashEntry *entries = arena_push(a, HashEntry, hm->capacity);
    bool *occupied = arena_push(a, bool, hm->capacity);
    migi_mem_clear(occupied, bool, hm->capacity);

    for (size_t j = 0; j < old_capacity; j++) {
        if (!(hm)->occupied[j]) continue;

        uint64_t i = hash_fnv((byte *)hm->entries->key.data, hm->entries->key.length) & (hm->capacity - 1);
        while (occupied[i]) {
            i = (i + 1) & (hm->capacity - 1);
        }
        entries[i] = hm->entries[j];
        occupied[i] = true;
    }
    (hm)->entries = entries;
    (hm)->occupied = occupied;
}
#endif

size_t hms_index_of_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key, uint64_t *hash_out) {
    uint64_t hash = hash_fnv((byte *)key.data, key.length) & (header->capacity - 1);
    size_t i = hash;
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

void hms_put_impl(Arena *a, HashmapHeader_ *header, void **data, size_t entry_size, String key, void *value) {
    if (header->capacity == 0) {
        header->capacity = HASHMAP_INIT_CAP;
        header->entries = arena_push(a, HashEntry_, header->capacity);
        // allocating an extra item for index 0 being treated as empty
        *data = arena_push_bytes(a, entry_size * (header->capacity + 1));

        migi_mem_clear(header->entries, HashEntry_, header->capacity);
        migi_mem_clear(*data, byte, entry_size * (header->capacity + 1));
    }

    if (header->size >= header->capacity * HASHMAP_LOAD_FACTOR) {
        header->capacity *= 2;
        crash_with_message("time to rehash!");
        // hm_rehash(a, header);
    }

    uint64_t hash = 0;
    size_t i = hms_index_of_impl(header, *data, entry_size, key, &hash);
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

void *hms_del_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    if (header->capacity == 0) return NULL;
    uint64_t hash_ = 0;
    size_t i = hms_index_of_impl(header, data, entry_size, key, &hash_);

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

    migi_mem_clear(&header->entries[i], HashEntry_, 1);
    header->size--;
    return last;
}


ptrdiff_t hms_index_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_index_of_impl(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return -1;
    }
    return header->entries[i].index;
}

void *hms_get_value_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_index_of_impl(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size) + sizeof(key);
}

void *hms_get_pair_impl(HashmapHeader_ *header, void *data, size_t entry_size, String key) {
    uint64_t hash_ = 0;
    size_t i = hms_index_of_impl(header, data, entry_size, key, &hash_);

    if (header->entries[i].index == 0) {
        return NULL;
    }

    byte *table = data;
    return table + (header->entries[i].index * entry_size);
}


#define hms_put(arena, hashmap, key, value)                   \
    (hms_put_impl((arena), (HashmapHeader_*)(hashmap),        \
        (void **)&(hashmap)->data, sizeof *((hashmap)->data), \
        (key), &(value)))


#define hms_get(hashmap, type, key)                             \
    ((type *)hms_get_value_impl((HashmapHeader_*)(hashmap),     \
        (hashmap)->data, sizeof (*(hashmap)->data), (key)))

#define hms_del(hashmap, type, key)                         \
    (type *)hms_del_impl((HashmapHeader_ *)(hashmap),       \
        (hashmap)->data, sizeof (*(hashmap)->data), (key))

#define hms_get_pair(hashmap, type, key)                    \
    ((type *)hms_get_pair_impl((HashmapHeader_*)(hashmap),  \
        (hashmap)->data, sizeof (*(hashmap)->data), (key))) \


#define hms_index(hashmap, key)                                  \
    (hms_index_impl((HashmapHeader_*)(hashmap), (hashmap)->data, \
        sizeof (*(hashmap)->data), (key)))



// Iterate over the hashmap by reference
// Use in a similar way as array_foreach
#define hm_foreach(hashmap, pair_type, pair)       \
    for (pair_type *(pair) = (hashmap)->data + 1;  \
        pair <= (hashmap)->data + (hashmap)->size; \
        pair++)


#endif // MIGI_HASHMAP_H
