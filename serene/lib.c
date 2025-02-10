#include "./lib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void *serene_align(void *ptr, size_t align) {
    uintptr_t hangover = ((uintptr_t) ptr) % align;
    if (hangover == 0) return ptr;
    return ptr + (align - ((uintptr_t) ptr) % align);
}

struct serene_Allocator serene_Arena_dyn(struct serene_Arena *this) {
    return (struct serene_Allocator) {
        .ctx = this, .alloc = serene_Arena_alloc, .free = serene_Arena_free,
    };
}

static bool serene_Arena_new_segment(struct serene_Arena *this, size_t buf_size) {
    if (buf_size < 4096) buf_size = 4096;
    struct serene_ArenaLL *new =
        serene_talloc(this->backing, buf_size, struct serene_ArenaLL);
    if (!new) return false;
    new->next = this->segments;
    new->cap = new->buf + buf_size;
    this->bump = new->buf;
    this->segments = new;
    return true;
}

void *serene_Arena_alloc(void *this_, struct serene_Ptrmeta meta) {
    struct serene_Arena *this = this_;

    if (!this->segments
        && !serene_Arena_new_segment(this, meta.size)) return NULL;
    char *out = serene_align(this->bump, meta.align);
    if (out + meta.size > this->segments->cap) {
        if (!serene_Arena_new_segment(this, meta.size)) return NULL;
        out = serene_align(this->bump, meta.align);
    }
    this->bump = out + meta.size;
    
    return out;
}

void serene_Arena_free(void *this_, void *ptr, struct serene_Ptrmeta meta) {
    struct serene_Arena *this = this_;
    if (this->bump == ptr + meta.size) this->bump -= meta.size;
    return;
}

void serene_Arena_deinit(struct serene_Arena *this) {
    struct serene_ArenaLL *next = this->segments;
    while (next) {
        struct serene_ArenaLL *head = next;
        next = head->next;
        serene_tfree(this->backing, head->cap - head->buf, head);
    }
}

struct serene_Allocator serene_Libc_dyn() {
    return (struct serene_Allocator){
        .ctx = NULL,
        .alloc = serene_Libc_alloc,
        .free = serene_Libc_free,
    };
}
void *serene_Libc_alloc(void *ctx, struct serene_Ptrmeta meta) {
    (void)ctx;
    return malloc(meta.size);
}
void serene_Libc_free(void *ctx, void *ptr, struct serene_Ptrmeta meta) {
    (void)meta;
    (void)ctx;
    free(ptr);
}
