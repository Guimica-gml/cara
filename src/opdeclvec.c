#include "opdeclvec.h"
#include <stdlib.h>

struct Opdecl *Opdecls_at(struct Opdecls *this, size_t idx) {
    if (!this->buf) return NULL;
    if (this->len <= idx) return NULL;
    return this->buf + idx;
}

bool Opdecls_init(struct Opdecls *this) {
    if (this->buf) return false;
    struct Opdecl *buf = malloc(4 * sizeof(struct Opdecl));
    if (!buf) return false;
    this->buf = buf;
    this->cap = 4;
    this->len = 0;
    return true;
}

bool Opdecls_grow(struct Opdecls *this) {
    if (!this->buf) return Opdecls_init(this);
    struct Opdecl *buf = malloc(2 * this->cap * sizeof(struct Opdecl));
    if (!buf) return false;
    for (size_t i = 0; i < this->len; i++)
        buf[i] = this->buf[i];
    free(this->buf);
    this->cap *= 2;
    this->buf = buf;
    return true;
}

bool Opdecls_push(struct Opdecls *this, struct Opdecl elem) {
    struct Opdecl *p = Opdecls_emplace(this);
    if (!p) return false;
    *p = elem;
    return true;
}

struct Opdecl *Opdecls_first(struct Opdecls *this) {
    if (this->len == 0) return NULL;
    return this->buf;
}

struct Opdecl *Opdecls_last(struct Opdecls *this) {
    if (this->len == 0) return NULL;
    return this->buf + this->len - 1;
}

struct Opdecl *Opdecls_emplace(struct Opdecls *this) {
    if (Opdecls_space(this) < 1 && !Opdecls_grow(this))
        return NULL;
    this->len++;
    return Opdecls_last(this);
}

void Opdecls_deinit(struct Opdecls *this) {
    free(this->buf);
    this->buf = NULL;
    this->len = 0;
    this->cap = 0;
}

size_t Opdecls_space(struct Opdecls *this) {
    return this->cap - this->len;
}
