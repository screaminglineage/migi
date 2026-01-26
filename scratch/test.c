#include "migi.h"

typedef enum {
    RackNode_None = 0,

    RackNode_String,
    RackNode_I64,
    RackNode_U64,
    RackNode_F64,

    RackNode_Count,
} RackNodeType;

const char *RACK_NODE_NAMES[RackNode_Count] = {
    [RackNode_None]   = "N", // error
    [RackNode_String] = "S",
    [RackNode_I64]    = "I",
    [RackNode_U64]    = "U",
    [RackNode_F64]    = "D",
};

typedef struct {
    RackNodeType type;
    union {
        int64_t i64;
        uint64_t u64;
        double f64;
        String string;
    } as;
} RackNode;

typedef struct RackPair RackPair;
struct RackPair {
    RackNode key, value;
    RackPair *next;
};

typedef struct {
    RackPair *pairs;
    size_t length;
    enum {
        RackFill_None = 0,
        RackFill_Key,
        RackFill_Value
    } fill;
} Rack;


void rack_begin_pair(Arena *arena, Rack *rack) {
    RackPair *pair = arena_new(arena, RackPair);
    stack_push(rack->pairs, pair);
    rack->fill = RackFill_Key;
}

RackNode *rack__get_node_to_fill(Rack *rack) {
    RackPair *pair = rack->pairs;
    RackNode *node = NULL;
    switch (rack->fill) {
        case RackFill_Key: {
            node = &pair->key;
            rack->fill = RackFill_Value;
        } break;
        case RackFill_Value: {
            node = &pair->value;
            rack->fill = RackFill_None;
        } break;

        case RackNode_None: {
            migi_unreachablef("`%s` called without first calling `rack_begin_pair`", __func__);
        }
    }
    return node;
}

void rack_write_string(Arena *arena, Rack *rack, String string) {
    RackNode *node = rack__get_node_to_fill(rack);
    node->type = RackNode_String;
    node->as.string = str_copy(arena, string);
}

void rack_write_i64(Rack *rack, int64_t num) {
    RackNode *node = rack__get_node_to_fill(rack);
    node->type = RackNode_I64;
    node->as.i64 = num;
}

void rack__dump_node(Arena *arena, StringList *list, RackNode node) {
    strlist_push_cstr(arena, list, RACK_NODE_NAMES[node.type]);

    switch (node.type) {
        case RackNode_String: {
            String string = node.as.string;

            // store size in little endian order
            char string_length[sizeof(uint32_t)];
            string_length[0] = (string.length >> 0);
            string_length[1] = (string.length >> 8);
            string_length[2] = (string.length >> 16);
            string_length[3] = (string.length >> 24);

            strlist_push_buffer(arena, list, string_length, sizeof(uint32_t));
            strlist_push(arena, list, string);
        } break;

        case RackNode_I64: {
            int64_t num = node.as.i64;

            // store number in little endian order
            char num_as_bytes[sizeof(int64_t)];
            for (size_t i = 0; i < sizeof(int64_t); i++) {
                num_as_bytes[i] = num >> 8 * i;
            }
            strlist_push_buffer(arena, list, num_as_bytes, sizeof(int64_t));
        } break;

        case RackNode_U64:
        case RackNode_F64:
            todof("implement dumping other types");

        case RackNode_None:
        case RackNode_Count:
            migi_unreachable();
    }
}

bool rack_dump(Rack *rack, String filepath) {
    StringList list = {0};
    Temp tmp = arena_temp();
    list_foreach(rack->pairs, RackPair, pair) {
        rack__dump_node(tmp.arena, &list, pair->key);
        rack__dump_node(tmp.arena, &list, pair->value);
    }

    bool res = str_to_file(strlist_to_string(tmp.arena, &list), filepath);
    arena_temp_release(tmp);
    return res;
}

RackNode rack__load_node(Arena *arena, String *rack_str, String filepath) {
    RackNode node = {0};

    switch (rack_str->data[0]) {
        case 'S': {
            *rack_str = str_skip(*rack_str, 1);
            if (rack_str->length < 4) {
                fprintf(stderr, "%s: failed to load rack from `%.*s`: %s\n", __func__, SV_FMT(filepath), "unexpected EOF");
                return node;
            }

            uint32_t length = rack_str->data[0]
                | rack_str->data[1] << 8
                | rack_str->data[2] << 16
                | rack_str->data[3] << 24;
            *rack_str = str_skip(*rack_str, sizeof(uint32_t));


            if (rack_str->length < length) {
                fprintf(stderr, "%s: failed to load rack from `%.*s`: "
                        "expected string of %u characters, but got string of %zu characters\n",
                        __func__, SV_FMT(filepath), length, rack_str->length);
                return node;
            }

            char *data = arena_push_nonzero(arena, char, length);
            memcpy(data, rack_str->data, length);
            node.type = RackNode_String;
            node.as.string = (String){
                .data = data,
                .length = length
            };
            *rack_str = str_skip(*rack_str, length);
        } break;

        case 'I': {
            *rack_str = str_skip(*rack_str, 1);
            if (rack_str->length < 4) {
                fprintf(stderr, "%s: failed to load rack from `%.*s`: %s\n", __func__, SV_FMT(filepath), "unexpected EOF");
                return node;
            }

            int64_t num = 0;
            for (size_t i = 0; i < 8; i++) {
                uint64_t tmp = rack_str->data[i];
                num |= tmp << (8 * i);
            }

            node.type = RackNode_I64;
            node.as.i64 = num;
            *rack_str = str_skip(*rack_str, sizeof(int64_t));
        } break;


        case 'U':
        case 'D':
            todof("implement loading other types");
        case 'N':
            migi_unreachablef("error!");
    }
    return node;
}

bool rack_load(Arena *arena, Rack *rack, String filepath) {
    Temp tmp = arena_temp_excl(arena);
    String rack_str = str_from_file(tmp.arena, filepath);
    if (rack_str.length == 0) {
        arena_temp_release(tmp);
        return false;
    }

    while (rack_str.length != 0) {
        RackPair *pair = arena_new(arena, RackPair);
        pair->key = rack__load_node(arena, &rack_str, filepath);
        pair->value = rack__load_node(arena, &rack_str, filepath);
        stack_push(rack->pairs, pair);
    }
    arena_temp_release(tmp);
    return true;
}

void rack__print_node(RackNode node) {
    switch (node.type) {
        case RackNode_String:
            printf("%.*s", SV_FMT(node.as.string));
        break;
        case RackNode_I64:
            printf("%ld", node.as.i64);
        break;
        case RackNode_U64:
            printf("%lu", node.as.u64);
        break;
        case RackNode_F64:
            printf("%f", node.as.f64);
        break;
        case RackNode_None:
        case RackNode_Count:
            printf("error!");
        break;
    }
}

int main() {
    String filepath = S("data.rack");
    Temp tmp = arena_temp();

    Rack rack = {0};
    rack_begin_pair(tmp.arena, &rack);
    rack_write_string(tmp.arena, &rack, S("Steel Ball Run"));
    rack_write_string(tmp.arena, &rack, S("Johnny Joestar"));

    rack_begin_pair(tmp.arena, &rack);
    rack_write_string(tmp.arena, &rack, S("Jojolion"));
    rack_write_i64(&rack, 8);

    rack_begin_pair(tmp.arena, &rack);
    rack_write_string(tmp.arena, &rack, S("Jojolands"));
    rack_write_i64(&rack, 9);

    rack_dump(&rack, filepath);

    Rack rack1 = {0};
    rack_load(tmp.arena, &rack1, filepath);
    list_foreach(rack1.pairs, RackPair, pair) {
        rack__print_node(pair->key);
        printf(": ");
        rack__print_node(pair->value);
        printf("\n");
    }

    arena_temp_release(tmp);
    return 0;
}
