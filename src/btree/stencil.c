#include "stencil.h"
#include <stdio.h>
#include <assert.h>

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
		int i;
		for (i = 0; i < this->len; i++) {
			if (@cmp@(elem, this->keys[i]) < 0)
				break;
		}
		this = this->subs[i];
	}
	return this;
}

static struct Split {
	struct @treename@_impl *node;
	@basetype@ lowbound;
} impl_input(
	struct @treename@_impl *this, struct serene_Allocator alloc,
	@basetype@ elem, struct @treename@_impl *sub
) {
	bool is_node = this->subs[0] != NULL;
	bool is_full = this->len >= @branching@;
	int i;

	for (i = 0; i < this->len; i++) {
		if (@cmp@(elem, this->keys[i]) < 0)
			break;
	}

	if (!is_full) {
		for (int j = this->len; j > i; j--)
			this->keys[j] = this->keys[j-1];
		this->keys[i] = elem;
		if (is_node) {
			for (int j = this->len; j > i; j--)
				this->subs[j+1] = this->subs[j];
			this->subs[i+1] = sub;
			sub->parent = this;
		}
		return (struct Split) {0};
	}
	struct @treename@_impl *node = serene_alloc(alloc, struct @treename@_impl);
	struct @treename@_impl subs[@branching@+2];
	@basetype@ keys[@branching@+1];
	int m = (@branching@+1)/2;
	assert(node);
	
	memcpy(&keys, &this->keys, i);
	memcpy(&keys[i+1], &this->keys[i], @branching@ - i);
	keys[i] = elem;
	if (is_node) {
		memcpy(&subs, &this->subs, i+1);
		memcpy(&subs[i+2], &this->subs[i+1], @branching@ - i);
		keys[i+1] = sub;
	}

	this->len = m-1;
	node->len = @branching@-m;
	memcpy(&this->keys, &keys, m-1);
	memcpy(&node->keys, &keys[m+1], @branching@-m);
	if (is_node) {
		memcpy(&this->subs, &subs, m);
		for (int j = 0; j < m-1; j++) this->subs[j]->parent = this;
		memcpy(&node->keys, &subs[m], @branching@+2-m);
		for (int j = 0; j < @branching@-m; j++) node->subs[j]->parent = node;
	}
	return (struct Split) {
		.node = node,
		.lowbound = keys[m],
	};
}

static struct @treename@_impl *impl_insert(
    struct @treename@_impl *this, struct serene_Allocator alloc,
	@basetype@ elem
) {
	struct @treename@_impl *leaf = impl_goto(this, elem);
	struct Split split = impl_input(leaf, alloc, elem, NULL)
	for (
		struct @treenam@_impl *cur = leaf->parent;
		cur && split.node;
		cur = cur->parent
	) split = impl_input(cur, alloc, split.lowbound, split.node);

	if (split.node) {
		struct @treename@_impl *root = serene_alloc(alloc, struct @treename@_impl);
		assert(root && "OOM");
		*root = (struct @treename@_impl) {
			.parent = NULL,
			.len = 1,
			.keys = {split.lowbound},
			.subs = {this, split.node},
		};
		this->parent = root;
		return root;
	}

	return this;
}

void @treename@_insert(
	struct @treenam@ *this, struct serene_Allocator alloc,
	@basetype@ elem
) {
	if (!this->root) {
		this->root = serene_alloc(alloc, struct @treename@_impl);
		assert(this->root);
		*this->root = (struct @treename@_impl) {0};
	}
	this->root = @treename@_insert(this->root, alloc, elem);
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
    impl_deinit(this->root, alloc);
}
