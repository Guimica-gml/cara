#include "stencil.h"
#include <stdlib.h>

@basetype@ *@vecname@_at(struct @vecname@ *this, size_t idx) {
    if (!this->buf) return NULL;
    if (this->len <= idx) return NULL;
    return this->buf + idx;
}

bool @vecname@_init(struct @vecname@ *this) {
    if (this->buf) return false;
    @basetype@ *buf = malloc(4 * sizeof(@basetype@));
    if (!buf) return false;
    this->buf = buf;
    this->cap = 4;
    this->len = 0;
    return true;
}

bool @vecname@_grow(struct @vecname@ *this) {
    if (!this->buf) return @vecname@_init(this);
    @basetype@ *buf = malloc(2 * this->cap * sizeof(@basetype@));
    if (!buf) return false;
    for (size_t i = 0; i < this->len; i++)
        buf[i] = this->buf[i];
    free(this->buf);
    this->cap *= 2;
    this->buf = buf;
    return true;
}

bool @vecname@_push(struct @vecname@ *this, @basetype@ elem) {
    @basetype@ *p = @vecname@_emplace(this);
    if (!p) return false;
    *p = elem;
    return true;
}

@basetype@ *@vecname@_first(struct @vecname@ *this) {
    if (this->len == 0) return NULL;
    return this->buf;
}

@basetype@ *@vecname@_last(struct @vecname@ *this) {
    if (this->len == 0) return NULL;
    return this->buf + this->len - 1;
}

@basetype@ *@vecname@_emplace(struct @vecname@ *this) {
    if (@vecname@_space(this) < 1 && !@vecname@_grow(this))
        return NULL;
    this->len++;
    return @vecname@_last(this);
}

void @vecname@_deinit(struct @vecname@ *this) {
    free(this->buf);
    this->buf = NULL;
    this->len = 0;
    this->cap = 0;
}

size_t @vecname@_space(struct @vecname@ *this) {
    return this->cap - this->len;
}
