#include "stencil.h"
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

struct @treename@_impl {
    struct @treename@_impl *subs[@branching@ + 1];
    struct @treename@_impl *parent;
    @basetype@ keys[@branching@];
    int len;
};

static struct @treename@_impl *impl_goto(
    struct @treename@_impl *this, @basetype@ elem
) {
    while (this->subs[0]) {
        int i = 0;
        while (
            i < this->len && @cmp@(this->keys[i], elem) < 0
        ) i++;
        this = this->subs[i];
    }
    return this;
}

/* static struct Split { */
/*     struct @treename@_impl *node; */
/*     @basetype@ elem; */
/* } impl_input( */
static bool impl_input(
    struct @treename@_impl *this, struct serene_Allocator alloc,
    @basetype@ *elem, struct @treename@_impl **node
    /* struct Split data */
) {
    bool is_node = this->subs[0] != NULL;
    bool is_full = this->len >= @branching@;

    int i = 0;
    while (
        i < this->len && @cmp@(this->keys[i], *elem) < 0
    ) i++; 

    if (!is_full) {
        tmove(&this->keys[i+1], &this->keys[i], this->len-i, @basetype@);
        this->keys[i] = *elem;

        if (is_node) {
            tmove(&this->subs[i+2], &this->subs[i+1], this->len-i, struct @treename@_impl*);
            this->subs[i+1] = *node;
            (*node)->parent = this;
        }

        this->len++;
        *node = NULL;
        return true;
    }

    struct @treename@_impl *subs[@branching@+2];
    @basetype@ keys[@branching@+1];
    int m = (@branching@+1)/2;
    tcopy(&keys[0], &this->keys[0], i, @basetype@);
    tcopy(&keys[i+1], &this->keys[i], @branching@ - i, @basetype@);
    keys[i] = *elem;
    if (is_node) {
        tcopy(&subs[0], &this->subs[0], i+1, struct @treename@_impl*);
        tcopy(&subs[i+2], &this->subs[i+1], @branching@ - i, struct @treename@_impl*);
        subs[i+1] = *node;
    }

    struct @treename@_impl *new = serene_alloc(alloc, struct @treename@_impl);
    if (!new) return false;
    *new = (struct @treename@_impl) {0};
    new->len = @branching@-m;
    this->len = m;
    tcopy(&this->keys[0], &keys[0], m, @basetype@);
    tcopy(&new->keys[0], &keys[m+1], @branching@-m, @basetype@);
    if (is_node) {
        tcopy(&this->subs[0], &subs[0], m+1, struct @treename@_impl*);
        for (int j = 0; j < m+1; j++) this->subs[j]->parent = this;
        tcopy(&new->subs[0], &subs[m+1], @branching@+1-m, struct @treename@_impl*);
        for (int j = 0; j < @branching@+1-m; j++) new->subs[j]->parent = *node;
    }

    *node = new;
    *elem = keys[m];
    return true;
}

static struct @treename@_impl *impl_insert(
    struct @treename@_impl *this, struct serene_Allocator alloc,
    @basetype@ elem
) {
    struct @treename@_impl *leaf = impl_goto(this, elem);
    struct @treename@_impl *node = NULL;
    do {
        if (!impl_input(leaf, alloc, &elem, &node)) return NULL;
        leaf = leaf->parent;
    } while (leaf && node);

    if (!node) return this;

    struct @treename@_impl *root = serene_alloc(alloc, struct @treename@_impl);
    assert(root && "OOM");
    *root = (struct @treename@_impl) {
        .parent = NULL,
        .len = 1,
        .keys = {elem},
        .subs = {this, node},
    };
    this->parent = root;
    node->parent = root;
    return root;
}

bool @treename@_insert(
    struct @treename@ *this, struct serene_Allocator alloc,
    @basetype@ elem
) {
    if (!this->root) {
        this->root = serene_alloc(alloc, struct @treename@_impl);
        if (!this->root) return false;
        *this->root = (struct @treename@_impl) {0};
    }
    struct @treename@_impl *tmp = impl_insert(this->root, alloc, elem);
    if (!tmp) return false;
    this->root = tmp;
    return true;
}

static void impl_deinit(struct @treename@_impl *this, struct serene_Allocator alloc) {
    if (this->subs[0] != NULL) {
        for (int i = 0; i <= this->len; i++) {
            impl_deinit(this->subs[i], alloc);
        }
    }
    serene_free(alloc, this);
}

void @treename@_deinit(struct @treename@ *this, struct serene_Allocator alloc) {
    if (!this->root) return;
    impl_deinit(this->root, alloc);
}

static @basetype@ *impl_search(struct @treename@_impl *this, @basetype@ elem) {
    while (this) {
        int diff = -1;
        int i = 0;
        while (
            i < this->len && (diff = @cmp@(this->keys[i], elem)) < 0
        ) i++;
        if (diff == 0) return &this->keys[i];
        this = this->subs[i];
    }
    return NULL;
}

@basetype@ *@treename@_search(struct @treename@ *this, @basetype@ elem) {
    if (!this->root) return NULL;
    impl_search(this->root, elem);
}

static void impl_print(struct @treename@_impl *this, int level) {
    if (this->subs[0] != NULL) {
        for (int i = 0; i < this->len; i++) {
            impl_print(this->subs[i], level + 1);
            for (int j = 0; j < level; j++) printf("  ");
            @print@(this->keys[i]);
            printf("\n");
        }
        impl_print(this->subs[this->len], level + 1);
    } else {
        for (int i = 0; i < this->len; i++) {
            for (int j = 0; j < level; j++) printf("  ");
            @print@(this->keys[i]);
            printf("\n");
        }
    }
}

void @treename@_print(struct @treename@ *this) {
    if (!this->root) return;
    impl_print(this->root, 0);
}
