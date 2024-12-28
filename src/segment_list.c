#include <stdbool.h>
#include <math.h>

#include "./arena.h"

#ifndef LIST_NAME
#error "define macro LIST_NAME for templated segment_list"
#endif
#ifndef LIST_TYPE
#error "define macro LIST_TYPE for templated segment_list"
#endif

// indirection needed for expanding LIST_NAME
#define SPLICE_DO(X, Y) X ## Y
#define SPLICE(X, Y) SPLICE_DO(X, Y)
#define list_at SPLICE(LIST_NAME, _at)
#define list_init SPLICE(LIST_NAME, _init)
#define list_grow SPLICE(LIST_NAME, _grow)
#define list_push SPLICE(LIST_NAME, _push)
#define list_last SPLICE(LIST_NAME, _last)
#define list_place SPLICE(LIST_NAME, _place)
#define list_capacity SPLICE(LIST_NAME, _capacity)
#define list_grow_segments SPLICE(LIST_NAME, _grow_segments)
#define list_active_segments SPLICE(LIST_NAME, _active_segments)

struct LIST_NAME;
size_t list_active_segments(struct LIST_NAME*);
size_t list_capacity(struct LIST_NAME*);
bool list_grow_segments(struct LIST_NAME*, struct Arena*);
bool list_init(struct LIST_NAME*, struct Arena*);
bool list_grow(struct LIST_NAME*, struct Arena*);
LIST_TYPE* list_at(struct LIST_NAME*, size_t);
LIST_TYPE* list_last(struct LIST_NAME*);
LIST_TYPE* list_place(struct LIST_NAME*, struct Arena*);
bool list_push(struct LIST_NAME*, struct Arena*, LIST_TYPE);

//////// GENERAL INFO
/// for fallible functions returning *bool*:
///     returns true on success and {0} on error
/// for fallible functions returning *pointers*:
///     returns a valid pointer on success and {0} on error

struct LIST_NAME {
    size_t len;
    size_t segs_cap;
    LIST_TYPE** segs;
};

size_t list_active_segments(struct LIST_NAME* this) {
    if (this->len == 0) return 0;
    return 8 * sizeof(size_t) - __builtin_clzl(this->len);
}

size_t list_capacity(struct LIST_NAME* this) {
    if (this->len == 0) return 0;
    size_t total_cap = (1 << list_active_segments(this)) - 1;
    return total_cap - this->len;
}

bool list_grow_segments(struct LIST_NAME* this, struct Arena* arena) {
    if (!this->segs) return false;
    LIST_TYPE** segs = arena_alloc(arena, 2 * this->segs_cap * sizeof(LIST_TYPE*));
    if (!segs) return false;
    for (size_t i = 0; i < this->segs_cap; i++) segs[i] = this->segs[i];
    this->segs_cap = 2 * this->segs_cap;
    this->segs = segs;
    return true;
}

bool list_init(struct LIST_NAME* this, struct Arena* arena) {
    if (this->segs) return false;
    LIST_TYPE** segs = arena_alloc(arena, 4 * sizeof(LIST_TYPE*));
    if (!segs) return false;
    this->segs_cap = 4;
    this->segs = segs;
    return true;
}

bool list_grow(struct LIST_NAME* this, struct Arena* arena) {
    if (this->segs_cap == 0 && !list_init(this, arena)) return false;
    size_t asegs = list_active_segments(this);
    if (this->segs_cap <= asegs && !list_grow_segments(this, arena)) return false;
    LIST_TYPE* new_seg = arena_alloc(arena, (1 << asegs) * sizeof(LIST_TYPE));
    if (!new_seg) return false;
    this->segs[asegs] = new_seg;
    return true;
}

LIST_TYPE* list_at(struct LIST_NAME* this, size_t index) {
    if (this->len <= index) return NULL;
    index++;
    size_t seg_idx = (8 * sizeof(size_t) - __builtin_clzl(index)) - 1;
    size_t idx = index - (1 << seg_idx);
    return this->segs[seg_idx] + idx;
}

LIST_TYPE* list_last(struct LIST_NAME* this) {
    if (this->len == 0) return NULL;
    return list_at(this, this->len - 1);
}

LIST_TYPE* list_place(struct LIST_NAME* this, struct Arena* arena) {
    if (list_capacity(this) < 1 && !list_grow(this, arena)) return NULL;
    this->len++;
    return list_last(this);
}

bool list_push(struct LIST_NAME* this, struct Arena* arena, LIST_TYPE elem) {
    LIST_TYPE* p = list_place(this, arena);
    if (!p) return false;
    *p = elem;
    return true;
}

#undef list_at
#undef list_init
#undef list_grow
#undef list_push
#undef list_last
#undef list_place
#undef list_capacity
#undef list_grow_segments
#undef list_active_segments

#undef LIST_TYPE
#undef LIST_NAME
