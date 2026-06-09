#include "migi.h"
#include "migi_random.h"

typedef enum {
    Ordering_Eq =  0, // left == right
    Ordering_Gt =  1, // left > right
    Ordering_Lt = -1, // left < right
} Ordering;

typedef int (*BinSearchCompFn)(void *left, void *right, void *user_data);

typedef struct {
    BinSearchCompFn comparator;
    void *user_data;
} SearchOpt;

static size_t binary_search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt);

// Convenience macros
//
// Search an array with a key
#define search(arr, length, key, ...)                                                \
    search_opt((byte *)(check_type(type_of(key), arr)), sizeof(*(arr)), (length), 0, \
            binarysearch__addr_of(*(arr), (key)),(SearchOpt){ .comparator = compare__func_for(*(arr)), __VA_ARGS__ })


// Search a particular field of a struct with a key
// For example:
// struct { int a, b; } *arr = { /* .... */ };
// search_key(arr, array_len(arr), a, 12) will only search for items where the field `a` is `12`
#define search_key(arr, length, field, key, ...)                                          \
    (check_type_value(type_of((arr)->field), (key)),                                      \
    binary_search_opt((byte *)arr, sizeof(*(arr)), (length),                              \
            offsetof(type_of(*(arr)), field), binarysearch__addr_of((arr)->field, (key)), \
            (SearchOpt){ .comparator = compare__func_for((arr)->field), __VA_ARGS__ }))

// Binary search an array with a key
#define binary_search(arr, length, key, ...)                                                \
    binary_search_opt((byte *)(check_type(type_of(key), arr)), sizeof(*(arr)), (length), 0, \
            binarysearch__addr_of(*(arr), (key)),(SearchOpt){ .comparator = compare__func_for(*(arr)), __VA_ARGS__ })


// Binary search a particular field of a struct with a key
// For example:
// struct { int a, b; } *arr = { /* .... */ };
// binary_search_key(arr, array_len(arr), a, 12) will only search for items where the field `a` is `12`
#define binary_search_key(arr, length, field, key, ...)                                   \
    (check_type_value(type_of((arr)->field), (key)),                                      \
    binary_search_opt((byte *)arr, sizeof(*(arr)), (length),                              \
            offsetof(type_of(*(arr)), field), binarysearch__addr_of((arr)->field, (key)), \
            (SearchOpt){ .comparator = compare__func_for((arr)->field), __VA_ARGS__ }))



// Some common comparison functions
static int compare_u64(void *left, void *right, void *user_data);
static int compare_i32(void *left, void *right, void *user_data);
static int compare_i64(void *left, void *right, void *user_data);
static int compare_f32(void *left, void *right, void *user_data);
static int compare_f64(void *left, void *right, void *user_data);
static int compare_str(void *left, void *right, void *user_data);
static int compare_cstr(void *left, void *right, void *user_data);
static int compare_char(void *left, void *right, void *user_data);




// TODO: can the logic be simplified further?
// Since Ordering is an int, the value itself doesnt matter
// TODO: move this into migi_string.h
static int str_cmp(Str a, Str b, StrEqOpt flags) {
    // Prevents using memcmp with NULL pointers
    if ( a.data && !b.data)  return Ordering_Gt;
    if (!a.data &&  b.data)  return Ordering_Lt;
    if (!a.data && !b.data)  return Ordering_Eq;

    // TODO: should this be called StrEq_IgnoreCase instead?
    if (!(flags & Eq_IgnoreCase)) return memcmp(a.data, b.data, a.length);

    for (size_t i = 0; i < min_of(a.length, b.length); i++) {
        char a_char = char_to_lower(a.data[i]);
        char b_char = char_to_lower(b.data[i]);
        if (a_char > b_char) {
            return Ordering_Gt;
        } else if (a_char < b_char) {
            return Ordering_Lt;
        }
    }

    if (a.length > b.length) return Ordering_Gt;
    if (a.length < b.length) return Ordering_Lt;
    return Ordering_Eq;
}


static size_t binary_search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt) {
    assertf(opt.comparator, "no suitable comparison function found, provide one manually");

    size_t left = 0;
    size_t right = length;

    while (left < right) {
        size_t mid = left + (right - left)/2;
        byte *elem = arr + elem_size*mid + field_offset;

        int result = opt.comparator(elem, key, opt.user_data);
        if (result == 0) return mid;

        if (result > 0) {
            right = mid;
        } else if (result < 0) {
            left  = mid + 1;
        }
    }
    return length;
}

static size_t search_opt(byte *arr, size_t elem_size, size_t length, size_t field_offset, void *key, SearchOpt opt) {
    assertf(opt.comparator, "no suitable comparison function found, provide one manually");

    for (size_t i = 0; i < length; i++) {
        byte *elem = arr + elem_size*i + field_offset;
        if (opt.comparator(elem, key, opt.user_data) == Ordering_Eq) return i;
    }
    return length;
}

static int compare_u64(void *left, void *right, void *user_data) {
    unused(user_data);
    uint64_t *a = left;
    uint64_t *b = right;
    return *a - *b;
}

static int compare_i32(void *left, void *right, void *user_data) {
    unused(user_data);
    int32_t *a = left;
    int32_t *b = right;
    return *a - *b;
}

static int compare_i64(void *left, void *right, void *user_data) {
    unused(user_data);
    int64_t *a = left;
    int64_t *b = right;
    return *a - *b;
}


static int compare_f32(void *left, void *right, void *user_data) {
    unused(user_data);
    float *a = left;
    float *b = right;
    return *a - *b;
}

static int compare_f64(void *left, void *right, void *user_data) {
    unused(user_data);
    double *a = left;
    double *b = right;
    return *a - *b;
}

static int compare_str(void *left, void *right, void *user_data) {
    unused(user_data);
    Str *a = left;
    Str *b = right;
    return str_cmp(*a, *b, 0);
}

static int compare_cstr(void *left, void *right, void *user_data) {
    unused(user_data);
    char **a = left;
    char **b = right;
    return strcmp(*a, *b);
}

static int compare_char(void *left, void *right, void *user_data) {
    unused(user_data);
    char *a = left;
    char *b = right;
    return *a - *b;
}

#define compare__func_for(type) \
    _Generic((type),            \
        Str:      compare_str,  \
        char *:   compare_cstr, \
        char:     compare_char, \
        float:    compare_f32,  \
        double:   compare_f64,  \
        int32_t:  compare_i32,  \
        int64_t:  compare_i64,  \
        uint64_t: compare_u64,  \
        default:  NULL          \
    )

#define binarysearch__addr_of(T, x) ((type_of(T)[1]){x})


void test_search() {
    Temp tmp = arena_temp();
    {
        int s = 10;
        int *arr = arena_push(tmp.arena, int, s);
        for (int i = 0; i < s; i++) {
            arr[i] = i;
        }
        for (int i = 0; i <= s; i++) {
            size_t n = binary_search(arr, s, i);
            assert((int)n == i);
        }
    }

    {
        Str arr[] = { S("bar"), S("baz"), S("foo"), S("hello"), S("world") };
        for (size_t i = 0; i < array_len(arr); i++) {
            size_t n = binary_search(arr, array_len(arr), arr[i]);
            assert(n == i);
        }
        assert(binary_search(arr, array_len(arr), S("different string")) == array_len(arr));
    }

    {
        typedef struct {
            int num1, num2;
            char ch;
        } Foo;

        int s = 10;
        Foo *arr = arena_push(tmp.arena, Foo, s);
        for (int i = 0; i < s; i++) {
            arr[i] = (Foo){
                .num1 = rand_range(0, 10),
                .num2 = i,
                .ch = rand_range('a', 'z'),
            };
        }

        for (int i = 0; i < s; i++) {
            size_t n = binary_search_key(arr, s, num2, i);
            assert((int)n == i);
        }
    }


    arena_temp_release(tmp);
}

void test_binary_search() {
    Temp tmp = arena_temp();
    {
        int s = 10;
        int *arr = arena_push(tmp.arena, int, s);
        for (int i = 0; i < s; i++) {
            arr[i] = i;
        }
        for (int i = 0; i <= s; i++) {
            size_t n = binary_search(arr, s, i);
            assert((int)n == i);
        }
    }

    {
        Str arr[] = { S("bar"), S("baz"), S("foo"), S("hello"), S("world") };
        for (size_t i = 0; i < array_len(arr); i++) {
            size_t n = binary_search(arr, array_len(arr), arr[i]);
            assert(n == i);
        }
        assert(binary_search(arr, array_len(arr), S("different string")) == array_len(arr));
    }

    {
        typedef struct {
            int num1, num2;
            char ch;
        } Foo;

        int s = 10;
        Foo *arr = arena_push(tmp.arena, Foo, s);
        for (int i = 0; i < s; i++) {
            arr[i] = (Foo){
                .num1 = rand_range(0, 10),
                .num2 = i,
                .ch = rand_range('a', 'z'),
            };
        }

        for (int i = 0; i < s; i++) {
            size_t n = binary_search_key(arr, s, num2, i);
            assert((int)n == i);
        }
    }


    arena_temp_release(tmp);
}

int main() {
    test_search();
    test_binary_search();
    return 0;
}
