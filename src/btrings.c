#include "btrings.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define tmove(dst, src, num, T)  {                      \
        T* __dst__ = dst;                               \
        T* __src__ = src;                               \
        memmove(__dst__, __src__, sizeof(T) * (num));   \
    }

#define tcopy(dst, src, num, T)  {                      \
        T* __dst__ = dst;                               \
        T* __src__ = src;                               \
        memcpy(__dst__, __src__, sizeof(T) * (num));    \
    }

struct Btrings_impl {
    struct Btrings_impl *subs[32 + 1];
    struct Btrings_impl *parent;
    struct Ordstring keys[32];
    int len;
};

static struct Btrings_impl *impl_goto(
    struct Btrings_impl *this, struct Ordstring elem
) {
    while (this->subs[0]) {
        int i = 0;
        while (
            i < this->len && Ordstring_cmp(this->keys[i], elem) < 0
        ) i++;
        this = this->subs[i];
    }
    return this;
}

static bool impl_input(
    struct Btrings_impl *this, struct serene_Allocator alloc,
    struct Ordstring *elem, struct Btrings_impl **node
) {
    bool is_node = this->subs[0] != NULL;
    bool is_full = this->len >= 32;

    int i = 0;
    while (
        i < this->len && Ordstring_cmp(this->keys[i], *elem) < 0
    ) i++; 

    if (!is_full) {
        tmove(&this->keys[i+1], &this->keys[i], this->len-i, struct Ordstring);
        this->keys[i] = *elem;

        if (is_node) {
            tmove(&this->subs[i+2], &this->subs[i+1], this->len-i, struct Btrings_impl*);
            this->subs[i+1] = *node;
            (*node)->parent = this;
        }

        this->len++;
        *node = NULL;
        return true;
    }

    struct Btrings_impl *subs[32+2];
    struct Ordstring keys[32+1];
    int m = (32+1)/2;
    tcopy(&keys[0], &this->keys[0], i, struct Ordstring);
    tcopy(&keys[i+1], &this->keys[i], 32 - i, struct Ordstring);
    keys[i] = *elem;
    if (is_node) {
        tcopy(&subs[0], &this->subs[0], i+1, struct Btrings_impl*);
        tcopy(&subs[i+2], &this->subs[i+1], 32 - i, struct Btrings_impl*);
        subs[i+1] = *node;
    }

    struct Btrings_impl *new = serene_alloc(alloc, struct Btrings_impl);
    if (!new) return false;
    *new = (struct Btrings_impl) {0};
    new->len = 32-m;
    this->len = m;
    tcopy(&this->keys[0], &keys[0], m, struct Ordstring);
    tcopy(&new->keys[0], &keys[m+1], 32-m, struct Ordstring);
    if (is_node) {
        tcopy(&this->subs[0], &subs[0], m+1, struct Btrings_impl*);
        for (int j = 0; j < m+1; j++) this->subs[j]->parent = this;
        tcopy(&new->subs[0], &subs[m+1], 32+1-m, struct Btrings_impl*);
        for (int j = 0; j < 32+1-m; j++) new->subs[j]->parent = *node;
    }

    *node = new;
    *elem = keys[m];
    return true;
}

static struct Btrings_impl *impl_insert(
    struct Btrings_impl *this, struct serene_Allocator alloc,
    struct Ordstring elem
) {
    struct Btrings_impl *leaf = impl_goto(this, elem);
    struct Btrings_impl *node = NULL;
    do {
        if (!impl_input(leaf, alloc, &elem, &node)) return NULL;
        leaf = leaf->parent;
    } while (leaf && node);

    if (!node) return this;

    struct Btrings_impl *root = serene_alloc(alloc, struct Btrings_impl);
    assert(root && "OOM");
    *root = (struct Btrings_impl) {
        .parent = NULL,
        .len = 1,
        .keys = {elem},
        .subs = {this, node},
    };
    this->parent = root;
    node->parent = root;
    return root;
}

bool Btrings_insert(
    struct Btrings *this, struct serene_Allocator alloc,
    struct Ordstring elem
) {
    if (!this->root) {
        this->root = serene_alloc(alloc, struct Btrings_impl);
        if (!this->root) return false;
        *this->root = (struct Btrings_impl) {0};
    }
    struct Btrings_impl *tmp = impl_insert(this->root, alloc, elem);
    if (!tmp) return false;
    this->root = tmp;
    return true;
}

static void impl_deinit(struct Btrings_impl *this, struct serene_Allocator alloc) {
    if (this->subs[0] != NULL) {
        for (int i = 0; i <= this->len; i++) {
            impl_deinit(this->subs[i], alloc);
        }
    }
    serene_free(alloc, this);
}

void Btrings_deinit(struct Btrings *this, struct serene_Allocator alloc) {
    if (!this->root) return;
    impl_deinit(this->root, alloc);
}

static struct Ordstring *impl_search(struct Btrings_impl *this, struct Ordstring elem) {
    while (this) {
        int diff = -1;
        int i = 0;
        while (i < this->len) {
            diff = Ordstring_cmp(this->keys[i], elem);
            if (diff >= 0) break;
            i++;
        }
        if (diff == 0) return &this->keys[i];
        this = this->subs[i];
    }
    return NULL;
}

struct Ordstring *Btrings_search(struct Btrings *this, struct Ordstring elem) {
    if (!this->root) return NULL;
    return impl_search(this->root, elem);
}

static void impl_print(struct Btrings_impl *this, int level) {
    if (this->subs[0] != NULL) {
        for (int i = 0; i < this->len; i++) {
            impl_print(this->subs[i], level + 1);
            for (int j = 0; j < level; j++) printf("  ");
            Ordstring_print(this->keys[i]);
            printf("\n");
        }
        impl_print(this->subs[this->len], level + 1);
    } else {
        for (int i = 0; i < this->len; i++) {
            for (int j = 0; j < level; j++) printf("  ");
            Ordstring_print(this->keys[i]);
            printf("\n");
        }
    }
}

void Btrings_print(struct Btrings *this) {
    if (!this->root) return;
    impl_print(this->root, 0);
}
