#ifndef TRASHMAN_H
#define TRASHMAN_H

#include <stddef.h>

#include "stb_c_lexer.h"
#include "stb_ds.h"

typedef struct Item {
    size_t id;
    struct Item *left;
    struct Item *right;
    stb_lexer value;
} Item;

typedef struct Pair {
    Item *a;
    Item *b;
    size_t item_id;
} Pair;

// Functions and globals to share
extern Pair **global_pairs;
extern int items_equal(Item *a, Item *b);
extern void copy_into_item(Item *a, stb_lexer *b);
extern void bpe_free();
extern const char* bpe_token_string(size_t id);

#endif // TRASHMAN_H
