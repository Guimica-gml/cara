#include "stencil.h"
#include <stdio.h>
#include <assert.h>

struct @treename@_impl {
    struct @treename@_impl *subs[@branching@ + 1];
    @basetype@ keys[@branching@];
    int len;
};

@basetype@ *@treename@_impl_search(struct @treename@_impl *this, @basetype@ elem) {
    bool is_node = this->subs[0] != NULL;
    int i = 0;
    int c = 0;
    while (true) {
        c = @cmp@(this->keys[i], elem);
        if (i >= this->len || c >= 0) break;
        i++;
    }
    if (i < this->len && c == 0) return &this->keys[i];
    if (is_node) return @treename@_impl_search(this->subs[i], elem);
    return NULL;
}

@basetype@ *@treename@_search(struct @treename@ *this, @basetype@ elem) {
    if (!this->root) return NULL;
    return @treename@_impl_search(this->root, elem);
}

static bool @treename@_impl_insert(
    struct @treename@_impl *this, struct serene_Allocator alloc, @basetype@ elem,
    struct @treename@_impl **out
) {
    bool is_node = this->subs[0] != NULL;
    bool is_full = this->len >= @branching@;
    int ins_loc = 0;
    while (
        ins_loc < this->len &&
        @cmp@(this->keys[ins_loc], elem) < 0
    ) ins_loc++;

    struct @treename@_impl *sapling = NULL;
    if (is_node) {
        if (!@treename@_impl_insert(
                this->subs[ins_loc],
                alloc,
                elem,
                &sapling
            )) return false;
        if (!sapling) return true;
    }

    if (!is_full) {
        if (is_node) {
            for (int i = this->len; i > ins_loc; i--) {
                this->keys[i] = this->keys[i-1];
                this->subs[i+1] = this->subs[i];
            }
            this->keys[ins_loc] = sapling->keys[0];
            this->subs[ins_loc] = sapling->subs[0];
            this->subs[ins_loc+1] = sapling->subs[1];
            serene_free(alloc, sapling);
        } else {
            for (int i = this->len; i > ins_loc; i--)
                this->keys[i] = this->keys[i-1];
            this->keys[ins_loc] = elem;
        }
        this->len++;
        return true;
    }

    int median = (@branching@+1)/2;
    @basetype@ keys[@branching@+1];
    struct @treename@_impl *subs[@branching@+2];

    struct @treename@_impl *left = serene_alloc(alloc, struct @treename@_impl);
    struct @treename@_impl *right = serene_alloc(alloc, struct @treename@_impl);
    if (!left || !right) return false;
    *left = *right = (struct @treename@_impl){0};
    *out = this;
    if (is_node) {
        for (int i = 0; i < ins_loc; i++) {
            keys[i] = this->keys[i];
            subs[i] = this->subs[i];
        }
        for (int i = ins_loc; i < @branching@; i++) {
            keys[i+1] = this->keys[i];
            subs[i+2] = this->subs[i+1];
        }
        keys[ins_loc] = sapling->keys[0];
        subs[ins_loc] = sapling->subs[0];
        subs[ins_loc+1] = sapling->subs[1];
        serene_free(alloc, sapling);

        int i;
        for (i = 0; i < median; i++) {
            left->keys[i] = keys[i];
            left->subs[i] = subs[i];
        }
        left->subs[i] = subs[i];

        for (i = 0; i < @branching@ - median; i++) {
            right->keys[i] = keys[i + median + 1];
            right->subs[i] = subs[i + median + 1];
        }
        right->subs[i] = subs[i + median + 1];
    } else {
        for (int i = 0; i < ins_loc; i++) keys[i] = this->keys[i];
        for (int i = ins_loc; i < @branching@; i++) keys[i+1] = this->keys[i];
        keys[ins_loc] = elem;

        for (int i = 0; i < median; i++) left->keys[i] = keys[i];
        for (int i = 0; i < @branching@ - median; i++) right->keys[i] = keys[i + median + 1];
    }
    left->len = median;
    right->len = @branching@ - median;
    this->keys[0] = keys[median];
    this->subs[0] = left;
    this->subs[1] = right;
    this->len = 1;
    return true;
}

bool @treename@_insert(struct @treename@ *this, struct serene_Allocator alloc, @basetype@ elem) {
    if (!this->root) {
        this->root = serene_alloc(alloc, struct @treename@_impl);
        assert(this->root && "OOM");
        *this->root = (struct @treename@_impl) {0};
    }
    struct @treename@_impl *tmp = NULL;
    if (!@treename@_impl_insert(this->root, alloc, elem, &tmp)) return true;
    if (tmp) this->root = tmp;
    return true;
}

void @treename@_impl_deinit(struct @treename@_impl *this, struct serene_Allocator alloc) {
    if (this->subs[0] != NULL)
        for (int i = 0; i <= this->len; i++) {
            serene_free(alloc, this->subs[i]);
        }

    serene_free(alloc, this);
}

void @treename@_deinit(struct @treename@ *this, struct serene_Allocator alloc) {
    @treename@_impl_deinit(this->root, alloc);
}
