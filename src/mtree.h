#ifndef MTREE_H
#define MTREE_H

#include "strings.h"

struct MTree {
    struct MTree* subs;
    size_t subs_count;
    struct String name;
    void* data;
};

// cleanup is needed, since MTree doesn't know
// how to clean up it's data
void MTree_map(
    struct MTree*,
    void (*cleanup)(void* old_data),
    void* (*transform)(void* ctx, void* data),
    void* ctx
);

void MTree_map_whole(
    struct MTree*,
    void (*transform)(void* ctx, struct MTree* node),
    void* ctx
);

void MTree_print(struct MTree*, void (*print_data)(void*));

struct MTree* MTree_index(struct MTree*, struct String);

struct MTree* MTree_load(
    struct serene_Trea*,
    struct String dir_path
);

#endif
