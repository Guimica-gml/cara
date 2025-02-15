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

struct serene_Allocator serene_Trea_dyn(struct serene_Trea* this) {
    return (struct serene_Allocator) {
        .ctx = this,
        .alloc = serene_Trea_dyn_alloc,
        .free = serene_Trea_dyn_free,
    };
}

struct Trea_Header {
    struct serene_Trea_Sub *next;
    struct serene_Trea_Sub *prev;
    uint16_t bump;
    uint16_t gen;
};

#define Trea_Sub_bufsz (4096 - sizeof(struct Trea_Header))

struct serene_Trea_Sub {
    struct Trea_Header meta;
    uint8_t buf[Trea_Sub_bufsz];
};

void* serene_Trea_alloc(struct serene_Trea* this, struct serene_Ptrmeta meta) {
    if (!this->current) return NULL;
    uint8_t* start = &this->current->buf[this->current->meta.bump];
    start += meta.align;
    start -= (intptr_t) start % meta.align;
    uint8_t* end = start + meta.size;
    if (end >= &this->current->buf[Trea_Sub_bufsz]) {
        struct serene_Trea_Sub* block = serene_alloc(this->backing, struct serene_Trea_Sub);
        if (!block) return NULL;
        block->meta.gen = this->current->meta.gen;
        struct serene_Trea_Sub* prev = this->current->meta.prev;
        if (prev) prev->meta.next = block;
        block->meta.prev = prev;
        block->meta.next = this->current;
        this->current->meta.prev = block;
        this->current = block;
        return serene_Trea_alloc(this, meta);
    }
    this->current->meta.bump = (uint16_t) (end - &this->current->buf[0]);
    return (void*) start;
}

void* serene_Trea_dyn_alloc(void* this, struct serene_Ptrmeta meta) {
    return serene_Trea_alloc((struct serene_Trea*)this, meta);
}

void serene_Trea_dyn_free(void* this, void* ptr, struct serene_Ptrmeta meta) {
    // no-op
    (void)this; (void)ptr; (void)meta;
}

struct serene_Trea serene_Trea_init(struct serene_Allocator backing) {
    struct serene_Trea_Sub* block = serene_alloc(backing, struct serene_Trea_Sub);
    block->meta = (struct Trea_Header) {0};
    return (struct serene_Trea) {
        .backing = backing,
        .current = block,
    };
}

void serene_Trea_deinit(struct serene_Trea this) {
    // doesn't work :KEKW:
    return;
    if (!this.current) return;
    uint16_t gen = this.current->meta.gen;
    struct serene_Trea_Sub* head = this.current;
    while (head && head->meta.gen >= gen) {
        if (head->meta.gen == gen) {
            struct serene_Trea_Sub* prev = head->meta.prev;
            struct serene_Trea_Sub* next = head->meta.next;
            struct serene_Trea_Sub* cur = head;
            head = next;
            if (prev != NULL) prev->meta.next = next;
            if (next != NULL) next->meta.prev = prev;
            serene_free(this.backing, cur);
        } else {
            struct serene_Trea child = {
                .backing = this.backing,
                .current = head,
            };
            head = head->meta.next;
            serene_Trea_deinit(child);
        }
    }
}

struct serene_Trea serene_Trea_sub(struct serene_Trea* root) {
    if (!root->current) return (struct serene_Trea) {0};
    struct serene_Trea_Sub* block = serene_alloc(root->backing, struct serene_Trea_Sub);
    struct serene_Trea_Sub* next = root->current->meta.next;
    uint16_t gen = root->current->meta.gen;
    if (next != NULL && next->meta.gen > gen) gen = next->meta.gen;
    gen++;
    block->meta.gen = gen;
    root->current->meta.next = block;
    block->meta.prev = root->current;
    block->meta.next = next;
    if (next != NULL) next->meta.prev = block;
    return (struct serene_Trea) {
        .backing = root->backing,
        .current = block,
    };
}
