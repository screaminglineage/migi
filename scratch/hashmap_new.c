#include "migi_string.h"
#include "arena.h"

// NOTE: HASHMAP_INIT_CAP *must* always be a power of two or bad things will happen
// TODO: describe exactly what will happen if the capacity is not a power of 2
#ifndef HASHMAP_INIT_CAP
   #define HASHMAP_INIT_CAP 256
#endif

// Any number in the range (0, 1.0) (both exclusive) should work
// A load factor of 1.0 is not supported as atleast 1 empty spot must
// exist at the end, for hashmap_pop to use it as a temporary variable
// for swap-removal and then return to caller
#ifndef HASHMAP_LOAD_FACTOR
   #define HASHMAP_LOAD_FACTOR 0.75
#endif
static_assert(HASHMAP_LOAD_FACTOR > 0.0 && HASHMAP_LOAD_FACTOR < 1.0, "load factor must be in the range (0.0, 1.0) (both exclusive)");


typedef uint64_t (*HashMapHashFn)(void *data, size_t size);
typedef bool (*HashMapEqFn)(void *a, void *b, size_t size);

typedef struct {
    uint64_t hash;
    uint64_t index;
} HashMapEntry;

#define HASHMAP__HEADER    \
    HashMapEntry *entries; \
    size_t size;           \
    size_t capacity;       \
    size_t _temp_index;    \
    HashMapHashFn hash_fn; \
    HashMapEqFn eq_fn;     \

typedef struct {
    HASHMAP__HEADER
} HashMapHeader;

#define HashMap(k, v)       \
    union {                 \
        HashMapHeader h;    \
        struct {            \
            HASHMAP__HEADER \
            struct {        \
                k key;      \
                v value;    \
            } *pairs;       \
        };                  \
    }


typedef enum {
    HashMapKey_Str,
    HashMapKey_CStr,
    HashMapKey_Other
} HashMapKeyType;

typedef struct {
    HashMapKeyType key_type;
    size_t key_size;
    size_t key_offset;
    size_t value_size;
    size_t value_offset;
    size_t elem_size;
    size_t elem_align;
} HashMapGeneric;

#define hashmap__generic(hashmap)                                   \
    (HashMapGeneric){                                               \
        .key_type     = hashmap__key_type((hashmap)->pairs->key),   \
        .key_size     = sizeof((hashmap)->pairs->key),              \
        .key_offset   = offsetof(typeof(*(hashmap)->pairs), key),   \
        .value_size   = sizeof((hashmap)->pairs->value),            \
        .value_offset = offsetof(typeof(*(hashmap)->pairs), value), \
        .elem_size    = sizeof(typeof(*(hashmap)->pairs)),          \
        .elem_align   = align_of(typeof(*(hashmap)->pairs)),         \
    }


// Array with a single element that decays to a pointer
// Needed for calls like `hashmap_put(&h, 1, foo)`, since `&1` is invalid
#define hashmap__addr_of(T, x) ((typeof(T)[1]){x})

// Strings store a pointer and a length which needs to be followed to get the
// actual data to be hashed, rather than hashing the raw bytes themselves
#define hashmap__key_type(k)      \
    _Generic((k),                 \
        Str:     HashMapKey_Str,  \
        char *:  HashMapKey_CStr, \
        default: HashMapKey_Other)

typedef struct {
    uint64_t hash;
    size_t entry_index;
    bool is_present;
} HashMapItem;

// Taken from https://nullprogram.com/blog/2025/01/19/
static uint64_t hash_fnv_bytes(void *d, size_t length) {
    byte *data = d;
    uint64_t h = 0x100;
    for (size_t i = 0; i < length; i++) {
        h ^= data[i] & 255;
        h *= 1111111111111111111;
    }
    return h;
}

static uint64_t hash_fnv_cstr(void *data, size_t size) {
    unused(size);
    char *cstr = *(char **)data;
    size_t len = strlen(cstr);
    return hash_fnv_bytes(cstr, len);
}

static uint64_t hash_fnv_str(void *s, size_t size) {
    unused(size);
    Str *str = s;
    return hash_fnv_bytes((void *)str->data, str->length);
}

static bool eq_bytes(void *a, void *b, size_t size) {
    return memcmp(a, b, size) == 0;
}

static bool eq_cstr(void *a, void *b, size_t size) {
    unused(size);
    char *str1 = *(char **)a;
    char *str2 = *(char **)b;
    return strcmp(str1, str2) == 0;
}

static bool eq_str(void *a, void *b, size_t size) {
    unused(size);
    Str str1 = *(Str *)a;
    Str str2 = *(Str *)b;
    return str_eq(str1, str2);
}


// Sets default equality and hash functions for some common types if not provided
static void hashmap__init(HashMapHeader *h, HashMapKeyType key_type) {
    if (h->hash_fn == NULL) {
        switch (key_type) {
            case HashMapKey_Str:    h->hash_fn = hash_fnv_str;   break;
            case HashMapKey_CStr:   h->hash_fn = hash_fnv_cstr;  break;
            case HashMapKey_Other:  h->hash_fn = hash_fnv_bytes; break;
            default:                migi_unreachable();               break;
        }
    }
    if (h->eq_fn == NULL) {
        switch (key_type) {
            case HashMapKey_Str:    h->eq_fn = eq_str;   break;
            case HashMapKey_CStr:   h->eq_fn = eq_cstr;  break;
            case HashMapKey_Other:  h->eq_fn = eq_bytes; break;
            default:                migi_unreachable();       break;
        }
    }
    return;
}

// Uses robin hood linear probing to insert an entry
// https://thenumb.at/Hashtables/#robin-hood-linear-probing
static void hashmap__insert_entry(HashMapHeader *h, HashMapEntry entry) {
    TIME_FUNCTION;
    size_t i = entry.hash & (h->capacity - 1);
    size_t dist = 0;
    while (h->entries[i].index != 0) {
        size_t cur_desired = h->entries[i].hash & (h->capacity - 1);
        size_t cur_dist = (i + h->capacity - cur_desired) & (h->capacity - 1);

        if (cur_dist < dist) {
            mem_swap(entry, h->entries[i]);
            dist = cur_dist;
        }

        dist++;
        i = (i + 1) & (h->capacity - 1);
    }
    h->entries[i] = entry;
}


// Grow the hashmap and rehash all the keys into the new allocation
// if `at_least` == 0, then the capacity is simply doubled
static void *hashmap__grow(Arena *a, HashMapHeader *h, HashMapGeneric g, void *pairs, size_t at_least) {
    TIME_FUNCTION;
    hashmap__init(h, g.key_type);

    size_t old_capacity = h->capacity;

    if (h->capacity == 0) {
        h->capacity = HASHMAP_INIT_CAP;
    } else {
        h->capacity = h->capacity * 2;
    }
    // grow the required amount
    if (at_least > 0) {
        size_t required_capacity = at_least * (1 + HASHMAP_LOAD_FACTOR);
        if (required_capacity > old_capacity) {
            h->capacity = next_power_of_two(required_capacity);
        }
    }

    // TODO: Shouldnt this also be an arena realloc?
    HashMapEntry *new_entries = arena_push(a, HashMapEntry, h->capacity);

    // allocating an extra item for index 0 being treated as the default index
    size_t pairs_size     = g.elem_size * (old_capacity + 1);
    size_t new_pairs_size = g.elem_size * (h->capacity + 1);

    void *new_pairs = arena_realloc_bytes(a, pairs, pairs_size, new_pairs_size, g.elem_align);

    HashMapEntry *old_entries = h->entries;
    h->entries = new_entries;
    for (size_t j = 0; j < old_capacity; j++) {
        if (old_entries[j].index != 0) {
            hashmap__insert_entry(h, old_entries[j]);
        }
    }
    return new_pairs;
}

static HashMapItem hashmap__index_of(HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    TIME_FUNCTION;
    HashMapItem result = {0};
    result.hash = h->hash_fn(key, g.key_size);
    size_t i = result.hash & (h->capacity - 1);
    size_t dist = 0;

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    hashmap__probes = 0;
#endif

    while (true) {
        if (h->entries[i].index == 0) {
            result.is_present = false;
            break;
        }

        // return if key was found
        byte *map_key = (byte *)pairs + (h->entries[i].index * g.elem_size);
        if (h->eq_fn(key, map_key, g.key_size)) {
            result.is_present = true;
            result.entry_index = i;
            break;
        }

        size_t cur_desired = h->entries[i].hash & (h->capacity - 1);
        size_t cur_dist = (i + h->capacity - cur_desired) & (h->capacity - 1);
        if (cur_dist < dist) {
            result.is_present = false;
        }

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
        hashmap__probes++;
        hashmap__max_probe_length = max_of(hashmap__max_probe_length, hashmap__probes);
#endif
        dist++;
        i = (i + 1) & (h->capacity - 1);
    }
    return result;
}



static void *hashmap__put(Arena *a, HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    TIME_FUNCTION;
    void *new_pairs = pairs;
    if (h->size >= (size_t)(h->capacity * HASHMAP_LOAD_FACTOR)) {
        new_pairs = hashmap__grow(a, h, g, pairs, 0);
    }

    HashMapItem item = hashmap__index_of(h, g, pairs, key);
    if (!item.is_present) {
        // new items are always inserted at the end of the table
        h->size++;
        hashmap__insert_entry(h, (HashMapEntry){ .hash = item.hash, .index = h->size });
        h->_temp_index = h->size;
    } else {
        h->_temp_index = h->entries[item.entry_index].index;
    }
    return new_pairs;
}


static void hashmap__get(HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    HashMapItem item = hashmap__index_of(h, g, pairs, key);
    if (item.is_present) {
        h->_temp_index = h->entries[item.entry_index].index;
    } else {
        h->_temp_index = 0;
    }
}

// Backshift Erasure (https://thenumb.at/Hashtables/#erase-backward-shift)
// Move elements back until theres an empty entry or the entry is already
// in its desired (best possible) position
static void hashmap__delete_entry(HashMapHeader *h, size_t start) {
    TIME_FUNCTION;
    size_t current = start;
    while (true) {
        h->entries[current].index = 0;
        size_t next = (current + 1) & (h->capacity - 1);

        HashMapEntry next_entry = h->entries[next];
        size_t next_desired = next_entry.hash & (h->capacity - 1);
        if (next_entry.index == 0) break;
        if (next_desired == next) break;

        h->entries[current] = h->entries[next];
        current = next;
    }
}

static void hashmap__delete(HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    TIME_FUNCTION;
    if (h->capacity == 0) {
        h->_temp_index = 0;
        return;
    }
    HashMapItem item = hashmap__index_of(h, g, pairs, key);
    if (!item.is_present) {
        h->_temp_index = 0;
        return;
    }

    // Update the entry of the last key in the hashmap data array to its new index
    byte *last_key = (byte *)pairs + (h->size * g.elem_size);
    HashMapItem last_item = hashmap__index_of(h, g, pairs, last_key);
    assertf(last_item.is_present, "hashmap should always have a last entry");

    h->entries[last_item.entry_index].index = h->entries[item.entry_index].index;
    h->_temp_index = h->entries[item.entry_index].index;
    assertf(h->_temp_index != 0, "nothing can map to the 0 value of the data array");

    h->size--;
    hashmap__delete_entry(h, item.entry_index);
}


// Reserve space for insertion of `count` elements into the hashmap without growing
#define hashmap_reserve(arena, hashmap, amount)                                                                           \
    (void)(                                                                                                               \
        hashmap__init(&(hashmap)->h, hashmap__key_type(hashmap)),                                                         \
        (hashmap)->pairs = hashmap__grow((arena), &(hashmap)->h, hashmap__generic(hashmap),  &(hashmap)->pairs, (amount)) \
    )

#define hashmap_put(arena, hashmap, k, v)                                                                                                                   \
    (void)(                                                                                                                                                 \
        (hashmap)->pairs = hashmap__put((arena), &(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
        (hashmap)->pairs[(hashmap)->_temp_index].key = (k),                                                                                                 \
        (hashmap)->pairs[(hashmap)->_temp_index].value = (v)                                                                                                \
    )

// Sets the default values of the hashmap
#define hashmap_set_default(arena, hashmap, k, v) \
    (((hashmap)->capacity == 0                    \
        ? hashmap__init(arena, hashmap), (void)0  \
        : (void)0),                               \
    ((hashmap)->keys[0] = (k),                    \
    (hashmap)->values[0] = (v)), (void)0)

#define hashmap_entry(arena, hashmap, k)                                                                                                                 \
    ((hashmap)->pairs = hashmap__put((arena), &(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
    (hashmap)->pairs[(hashmap)->_temp_index].key = (k),                                                                                                  \
    &(hashmap)->pairs[(hashmap)->_temp_index].value)

// TODO: make this type safe on the type of the key
// check_type macro needs to be modified a bit to get it to work
#define hashmap_get(hashmap, k)                                                                                              \
    ((hashmap__get(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
    (hashmap)->pairs[(hashmap)->_temp_index].value))                                                                         \

// TODO: make this type safe on the type of the key
// check_type macro needs to be modified a bit to get it to work
#define hashmap_at(hashmap, k)                                                                                                \
    ((hashmap__get(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
    (hashmap)->_temp_index == 0                                                                                               \
        ? NULL                                                                                                                \
        : (&(hashmap)->pairs[(hashmap)->_temp_index].value)))

#define hashmap_foreach(hashmap, pair)                         \
    for (typeof((hashmap)->pairs) pair = (hashmap)->pairs + 1; \
        pair <= (hashmap)->pairs + (hashmap)->size;            \
        pair++)

// TODO: make this type safe on the type of the key
// check_type macro needs to be modified a bit to get it to work
#define hashmap_delete(hashmap, k)                                                                                             \
    (hashmap__delete(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
    (hashmap)->_temp_index == 0                                                                                                \
      ? (hashmap)->pairs[0].value                                                                                              \
      : ((hashmap)->pairs[(hashmap)->size + 2] = (hashmap)->pairs[(hashmap)->_temp_index],                                     \
        (hashmap)->pairs[(hashmap)->_temp_index] = (hashmap)->pairs[(hashmap)->size + 1],                                      \
        (hashmap)->pairs[(hashmap)->size + 2].value))


#if 1
int main(void) {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    HashMap(char *, int) h = {0};

    #define RESERVE 0
    if (RESERVE) {
        hashmap_reserve(a, &h, 1024);
    }

    hashmap_put(a, &h, "hello", 1);
    *hashmap_entry(a, &h, "world") = 2;

    int v = hashmap_get(&h, "hello");
    printf("v = %d\n", v);
    int *p = hashmap_at(&h, "world");
    printf("*p = %d\n", *p);
    *p = 3;
    printf("*p = %d\n", *p);

    hashmap_foreach(&h, pair) {
        printf("%s => %d\n", pair->key, pair->value);
    }

    int n = hashmap_delete(&h, "hello");
    printf("%d", n);

    arena_temp_release(tmp);
    return 0;
}
#else
int main(void) {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    HashMap(Str, int) h = {0};

    #define RESERVE 0
    if (RESERVE) {
        hashmap_reserve(a, &h, 1024);
    }

    hashmap_put(a, &h, S("hello"), 1);
    *hashmap_entry(a, &h, S("world")) = 2;

    int v = hashmap_get(&h, S("hello"));
    printf("v = %d\n", v);
    int *p = hashmap_at(&h, S("world"));
    printf("*p = %d\n", *p);
    *p = 3;
    printf("*p = %d\n", *p);

    hashmap_foreach(&h, pair) {
        printf("%.*s => %d\n", SArg(pair->key), pair->value);
    }

    int n = hashmap_delete(&h, S("hello"));
    printf("%d", n);

    arena_temp_release(tmp);
    return 0;
}
#endif
