#include "migi_string.h"
#include "arena.h"

// NOTE: HASHMAP_INIT_CAP *must* always be a power of two or bad things will happen
#ifndef HASHMAP_INIT_CAP
   #define HASHMAP_INIT_CAP 256
#endif
static_assert(HASHMAP_INIT_CAP != 0 && (HASHMAP_INIT_CAP & (HASHMAP_INIT_CAP - 1)) == 0, "HASHMAP_INIT_CAP must be a nonzero power of 2");

// Any number in the range (0, 1.0) (both exclusive) should work
// A load factor of 1.0 is not supported as atleast 1 empty spot must
// exist at the end, for hashmap_pop to use it as a temporary variable
// for swap-removal and then return to caller
#ifndef HASHMAP_LOAD_FACTOR
   #define HASHMAP_LOAD_FACTOR 0.70
#endif
static_assert(HASHMAP_LOAD_FACTOR > 0.0 && HASHMAP_LOAD_FACTOR < 1.0, "HASHMAP_LOAD_FACTOR must be in the range (0.0, 1.0) (both exclusive)");

// Optional tracking of maximum probe length for statistics
#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    threadvar size_t hashmap__max_probe_length = 0;
    threadvar size_t hashmap__probes = 0;
#endif

typedef struct {
    uint64_t hash;
    uint64_t index;
} HashMapEntry;

typedef uint64_t (*HashMapHashFn)(void *data, size_t size);
typedef bool (*HashMapEqFn)(void *a, void *b, size_t size);

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

// Strings store a pointer and a length which needs to be followed to get the
// actual data to be hashed, rather than hashing the raw bytes themselves
// TODO: could also add support for integer keys where it simply uses key itself as the hash function
#define hashmap__key_type(k)      \
    _Generic((k),                 \
        Str:     HashMapKey_Str,  \
        char *:  HashMapKey_CStr, \
        default: HashMapKey_Other)


typedef struct {
    HashMapKeyType key_type;
    uint8_t elem_align;
    size_t elem_size;
    size_t key_size;
} HashMapGeneric;

#define hashmap__generic(hashmap)                                    \
    (HashMapGeneric){                                                \
        .key_type     = hashmap__key_type((hashmap)->pairs->key),    \
        .key_size     = sizeof((hashmap)->pairs->key),               \
        .elem_size    = sizeof(type_of(*(hashmap)->pairs)),          \
        .elem_align   = align_of(type_of(*(hashmap)->pairs)),        \
    }


// Array with a single element that decays to a pointer
// Needed for calls like `hashmap_put(&h, 1, foo)`, since `&1` is invalid
// NOTE: type_of(x) cannot be used here since if x is a c-string,
// then this macro returns becomes something like `char[1][LEN(cstr)]` instead of `char**`
#define hashmap__addr_of(T, x) ((type_of(T)[1]){x})



// TODO: add some more hash functions
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
            default:                migi_unreachable();          break;
        }
    }
    if (h->eq_fn == NULL) {
        switch (key_type) {
            case HashMapKey_Str:    h->eq_fn = eq_str;   break;
            case HashMapKey_CStr:   h->eq_fn = eq_cstr;  break;
            case HashMapKey_Other:  h->eq_fn = eq_bytes; break;
            default:                migi_unreachable();  break;
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
        size_t required_capacity = (size_t)(at_least * (1 + HASHMAP_LOAD_FACTOR));
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

typedef struct {
    uint64_t hash;
    int64_t entry_index;  // -1 means item was not found
} HashMapItem;

static HashMapItem hashmap__index_of(HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    TIME_FUNCTION;
    HashMapItem result = { .entry_index = -1 };
    result.hash = h->hash_fn(key, g.key_size);
    size_t i = result.hash & (h->capacity - 1);
    size_t dist = 0;

#ifdef HASHMAP_TRACK_MAX_PROBE_LENGTH
    hashmap__probes = 0;
#endif

    while (true) {
        if (h->entries[i].index == 0) {
            break;
        }

        // return if key was found
        byte *map_key = (byte *)pairs + (h->entries[i].index * g.elem_size);
        if (h->eq_fn(key, map_key, g.key_size)) {
            result.entry_index = i;
            break;
        }

        size_t cur_desired = h->entries[i].hash & (h->capacity - 1);
        size_t cur_dist = (i + h->capacity - cur_desired) & (h->capacity - 1);
        if (cur_dist < dist) {
            break;
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
    if (item.entry_index == -1) {
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
    if (item.entry_index != -1) {
        h->_temp_index = h->entries[item.entry_index].index;
    } else {
        h->_temp_index = 0;
    }
}

// Backshift Erasure (https://thenumb.at/Hashtables/#erase-backward-shift)
// Move elements back until theres an empty entry or the entry is already
// in its desired (best possible) position
static void hashmap__del_entry(HashMapHeader *h, size_t start) {
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

static void hashmap__del(HashMapHeader *h, HashMapGeneric g, void *pairs, void *key) {
    TIME_FUNCTION;
    if (h->capacity == 0) {
        h->_temp_index = 0;
        return;
    }
    HashMapItem item = hashmap__index_of(h, g, pairs, key);
    if (item.entry_index == -1) {
        h->_temp_index = 0;
        return;
    }

    // Update the entry of the last key in the hashmap data array to its new index
    byte *last_key = (byte *)pairs + (h->size * g.elem_size);
    HashMapItem last_item = hashmap__index_of(h, g, pairs, last_key);
    assertf(last_item.entry_index != -1, "hashmap should always have a last entry");

    h->entries[last_item.entry_index].index = h->entries[item.entry_index].index;
    h->_temp_index = h->entries[item.entry_index].index;
    assertf(h->_temp_index != 0, "nothing can map to the 0 value of the data array");

    h->size--;
    hashmap__del_entry(h, item.entry_index);
}


// Reserve space for insertion of `count` elements into the hashmap without growing
#define hashmap_reserve(arena, hashmap, amount)                      \
    (void)(                                                          \
        (hashmap)->pairs = hashmap__grow((arena), &(hashmap)->h,     \
                                        hashmap__generic(hashmap),   \
                                        &(hashmap)->pairs, (amount)) \
    )


#define hashmap_put(arena, hashmap, k, v)                                                              \
    (void)(                                                                                            \
        (hashmap)->pairs = hashmap__put((arena), &(hashmap)->h, hashmap__generic((hashmap)),           \
                                        (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
        (hashmap)->pairs[(hashmap)->_temp_index].key = (k),                                            \
        (hashmap)->pairs[(hashmap)->_temp_index].value = (v)                                           \
    )


// Sets the default values of the hashmap
#define hashmap_set_default(arena, hashmap, k, v) \
    (void)(                                       \
        hashmap_reserve((arena), (hashmap), 0),   \
        (hashmap)->pairs[0].key = (k),            \
        (hashmap)->pairs[0].value = (v)           \
    )


#define hashmap_entry(arena, hashmap, k)                                                           \
    ((hashmap)->pairs = hashmap__put((arena), &(hashmap)->h, hashmap__generic((hashmap)),          \
                                    (hashmap)->pairs, hashmap__addr_of((hashmap)->pairs->key, k)), \
    (hashmap)->pairs[(hashmap)->_temp_index].key = (k),                                            \
    &(hashmap)->pairs[(hashmap)->_temp_index].value)


// Get the type of the pairs for the hashmap
// NOTE: this is required as the pair type itself is an unnamed struct type declared by the HashMap() macro
#define hashmap_pair_type(hashmap) \
    type_of((hashmap)->pairs)


// Get the pair from a pointer to a value
// For example `hashmap_pair(&h, hashmap_at(&h, "world"))->key`
#define hashmap_pair(hashmap, v_ptr)                                                                      \
    ((v_ptr)                                                                                              \
        ? ((type_of((hashmap)->pairs))((uintptr_t)(v_ptr) - offsetof(type_of(*(hashmap)->pairs), value))) \
        : NULL)


#define hashmap_get(hashmap, k)                                                 \
    (check_type_value(type_of((hashmap)->pairs->key), (k)),                     \
    (hashmap__get(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, \
                   hashmap__addr_of((hashmap)->pairs->key, k)),                 \
    (hashmap)->pairs[(hashmap)->_temp_index].value))                            \


#define hashmap_at(hashmap, k)                                                  \
    (check_type_value(type_of((hashmap)->pairs->key), (k)),                     \
    (hashmap__get(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs, \
                   hashmap__addr_of((hashmap)->pairs->key, k)),                 \
    (hashmap)->_temp_index == 0                                                 \
        ? NULL                                                                  \
        : (&(hashmap)->pairs[(hashmap)->_temp_index].value)))


#define hashmap_foreach(hashmap, pair)                          \
    for (type_of((hashmap)->pairs) pair = (hashmap)->pairs + 1; \
        pair <= (hashmap)->pairs + (hashmap)->size;             \
        pair++)


#define hashmap_del(hashmap, k)                                                            \
    (check_type_value(type_of((hashmap)->pairs->key), (k)),                                \
    hashmap__del(&(hashmap)->h, hashmap__generic((hashmap)), (hashmap)->pairs,             \
                  hashmap__addr_of((hashmap)->pairs->key, k)),                             \
    (hashmap)->_temp_index == 0                                                            \
      ? (hashmap)->pairs[0].value                                                          \
      : ((hashmap)->pairs[(hashmap)->size + 2] = (hashmap)->pairs[(hashmap)->_temp_index], \
        (hashmap)->pairs[(hashmap)->_temp_index] = (hashmap)->pairs[(hashmap)->size + 1],  \
        (hashmap)->pairs[(hashmap)->size + 2].value))


int str_key() {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    HashMap(Str, int) h = {0};

    hashmap_set_default(a, &h, S("EMPTY"), 0xFF);

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
    printf("size: %zu\ncapacity: %zu\n", h.size, h.capacity);

    int n = hashmap_del(&h, S("hello"));
    printf("Deleted: %d\n", n);

    int default_value = hashmap_get(&h, S("hello"));
    printf("%X\n", default_value);

    printf("%.*s\n", SArg(hashmap_pair(&h, hashmap_at(&h, S("world")))->key));

    printf("\n\n");
    arena_temp_release(tmp);
    return 0;
}

int cstr_key() {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    HashMap(char *, int) h = {0};

    hashmap_set_default(a, &h, "EMPTY", 0xFF);

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
    printf("size: %zu\ncapacity: %zu\n", h.size, h.capacity);

    int n = hashmap_del(&h, "hello");
    printf("Deleted: %d\n", n);

    int default_value = hashmap_get(&h, "hello");
    printf("%X\n", default_value);

    printf("%s\n", hashmap_pair(&h, hashmap_at(&h, "world"))->key);

    printf("\n\n");
    arena_temp_release(tmp);
    return 0;
}

int custom_key() {
    Temp tmp = arena_temp();
    Arena *a = tmp.arena;

    typedef struct {
        int x, y;
    } Point;

    HashMap(Point, int) h = {0};

    hashmap_set_default(a, &h, ((Point){-1,-1}), 0xFF);

    #define RESERVE 0
    if (RESERVE) {
        hashmap_reserve(a, &h, 1024);
    }

    hashmap_put(a, &h, ((Point){12,24}), 1);
    *hashmap_entry(a, &h, ((Point){80,-145})) = 2;

    int v = hashmap_get(&h, ((Point){12,24}));
    printf("v = %d\n", v);
    int *p = hashmap_at(&h, ((Point){80,-145}));
    printf("*p = %d\n", *p);
    *p = 3;
    printf("*p = %d\n", *p);

    hashmap_foreach(&h, pair) {
        printf("(%d %d) => %d\n", pair->key.x, pair->key.y, pair->value);
    }
    printf("size: %zu\ncapacity: %zu\n", h.size, h.capacity);

    int n = hashmap_del(&h, ((Point){12,24}));
    printf("Deleted %d\n", n);

    int default_value = hashmap_get(&h, ((Point){12,24}));
    printf("%X\n", default_value);

    hashmap_pair_type(&h) pair = hashmap_pair(&h, hashmap_at(&h, ((Point){80,-145})));
    printf("(%d %d)\n", pair->key.x, pair->key.y);

    printf("\n\n");
    arena_temp_release(tmp);
    return 0;
}

int main() {
    str_key();
    cstr_key();
    custom_key();
    return 0;
}
