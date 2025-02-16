#include "tokenvec.h"
#include <stdlib.h>

struct Token *Tokenvec_at(struct Tokenvec *this, size_t idx) {
    if (!this->buf) return NULL;
    if (this->len <= idx) return NULL;
    return this->buf + idx;
}

bool Tokenvec_init(struct Tokenvec *this) {
    if (this->buf) return false;
    struct Token *buf = malloc(4 * sizeof(struct Token));
    if (!buf) return false;
    this->buf = buf;
    this->cap = 4;
    this->len = 0;
    return true;
}

bool Tokenvec_grow(struct Tokenvec *this) {
    if (!this->buf) return Tokenvec_init(this);
    struct Token *buf = malloc(2 * this->cap * sizeof(struct Token));
    if (!buf) return false;
    for (size_t i = 0; i < this->len; i++)
        buf[i] = this->buf[i];
    free(this->buf);
    this->cap *= 2;
    this->buf = buf;
    return true;
}

bool Tokenvec_push(struct Tokenvec *this, struct Token elem) {
    struct Token *p = Tokenvec_emplace(this);
    if (!p) return false;
    *p = elem;
    return true;
}

struct Token *Tokenvec_first(struct Tokenvec *this) {
    if (this->len == 0) return NULL;
    return this->buf;
}

struct Token *Tokenvec_last(struct Tokenvec *this) {
    if (this->len == 0) return NULL;
    return this->buf + this->len - 1;
}

struct Token *Tokenvec_emplace(struct Tokenvec *this) {
    if (Tokenvec_space(this) < 1 && !Tokenvec_grow(this))
        return NULL;
    this->len++;
    return Tokenvec_last(this);
}

void Tokenvec_deinit(struct Tokenvec *this) {
    free(this->buf);
    this->buf = NULL;
    this->len = 0;
    this->cap = 0;
}

size_t Tokenvec_space(struct Tokenvec *this) {
    return this->cap - this->len;
}
