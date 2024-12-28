#include <stdbool.h>
#include <stdlib.h>

// NOTE: uses malloc

#ifndef VEC_NAME
#error "define macro VEC_NAME for templated vector"
#endif
#ifndef VEC_TYPE
#error "define macro VEC_TYPE for templated vector"
#endif

// indirection needed for expanding LIST_NAME
#define SPLICE_DO(X, Y) X ## Y
#define SPLICE(X, Y) SPLICE_DO(X, Y)
#define vec_at SPLICE(VEC_NAME, _at)
#define vec_init SPLICE(VEC_NAME, _init)
#define vec_grow SPLICE(VEC_NAME, _grow)
#define vec_push SPLICE(VEC_NAME, _push)
#define vec_last SPLICE(VEC_NAME, _last)
#define vec_place SPLICE(VEC_NAME, _place)
#define vec_deinit SPLICE(VEC_NAME, _deinit)
#define vec_capacity SPLICE(VEC_NAME, _capacity)

struct VEC_NAME;
VEC_TYPE* vec_at(struct VEC_NAME*, size_t);
bool vec_init(struct VEC_NAME*);
bool vec_grow(struct VEC_NAME*);
bool vec_push(struct VEC_NAME*, VEC_TYPE);
VEC_TYPE* vec_last(struct VEC_NAME*);
VEC_TYPE* vec_place(struct VEC_NAME*);
void vec_deinit(struct VEC_NAME*);
size_t vec_capacity(struct VEC_NAME*);

struct VEC_NAME {
    size_t len;
    size_t cap;
    VEC_TYPE* buf;
};

VEC_TYPE* vec_at(struct VEC_NAME* this, size_t idx) {
    if (!this->buf) return NULL;
    if (this->len <= idx) return NULL;
    return this->buf + idx;
}

bool vec_init(struct VEC_NAME* this) {
    if (this->buf) return false;
    VEC_TYPE* buf = malloc(4 * sizeof(VEC_TYPE));
    if (!buf) return false;
    this->buf = buf;
    this->cap = 4;
    return true;
}

bool vec_grow(struct VEC_NAME* this) {
    if (!this->buf) return vec_init(this);
    VEC_TYPE* buf = malloc(2 * this->cap * sizeof(VEC_TYPE));
    if (!buf) return false;
    for (size_t i = 0; i < this->len; i++) buf[i] = this->buf[i];
    free(this->buf);
    this->cap *= 2;
    this->buf = buf;
    return true;
}

bool vec_push(struct VEC_NAME* this, VEC_TYPE elem) {
    VEC_TYPE* p = vec_place(this);
    if (!p) return false;
    *p = elem;
    return true;
}

VEC_TYPE* vec_last(struct VEC_NAME* this) {
    if (this->len == 0) return NULL;
    return vec_at(this, this->len-1);
}

VEC_TYPE* vec_place(struct VEC_NAME* this) {
    if (vec_capacity(this) < 1 && !vec_grow(this)) return NULL;
    this->len++;
    return vec_last(this);
}

void vec_deinit(struct VEC_NAME* this) {
    free(this->buf);
    this->buf = NULL;
    this->len = 0;
    this->cap = 0;
}

size_t vec_capacity(struct VEC_NAME* this) {
    return this->cap - this->len;
}

#undef vec_at
#undef vec_init
#undef vec_grow
#undef vec_push
#undef vec_last
#undef vec_place
#undef vec_capacity

#undef VEC_TYPE
#undef VEC_NAME
