#include "migi.h"

typedef struct Node Node;
struct Node {
    Node *next;
    Node *prev;
    Node *first_child;
    Node *last_child;
    Node *parent;

    String name;
};

Node *pre_order_next(Node *root) {
    if (root->first_child) {
        return root->first_child;
    }
    for (Node *n = root; n; n = n->parent) {
        if (n->next) {
            return n->next;
        }
    }
    return NULL;
}

Node *post_order_next(Node *root) {
    if (root->next && root->next->first_child) {
        return root->next->first_child;
    }
    for (Node *n = root; n; n = n->next) {
        if (n->next) {
            return n->next;
        }
    }
    if (root->parent) {
        return root->parent;
    }
    return NULL;
}

Node *push_child(Arena *a, Node *root, String data) {
    Node *node = arena_new(a, Node);
    node->name = data;
    node->parent = root;
    dll_push_tail(root->first_child, root->last_child, node);
    return node;
}

int main() {
    Arena *a = arena_init();
    Node *root = arena_new(a, Node);
    root->name = SV("root");

    for (size_t i = 0; i < 5; i++) {
        push_child(a, root, stringf(a, "root-child-%zu", i));
    }

    list_foreach(root->first_child, Node, child) {
        for (size_t i = 0; i < 3; i++) {
            push_child(a, child, stringf(a, "%.*s-child-%zu", SV_FMT(child->name), i));
        }
    }

    printf("Pre-Order Traversal\n");
    {
        Node *it = root;
        while (it) {
            printf("%.*s\n", SV_FMT(it->name));
            it = pre_order_next(it);
        }
    }
    printf("\n");

    printf("Post-Order Traversal\n");
    {
        Node *it = root;
        while (it->first_child) {
            it = it->first_child;
        }
        while (it) {
            printf("%.*s\n", SV_FMT(it->name));
            it = post_order_next(it);
        }
    }


    putchar('\n');
    return 0;
}
