#ifndef SERENE_H
#define SERENE_H

#include <stddef.h>
#include <stdalign.h>

void* serene_align(void*, size_t);

struct serene_Ptrmeta {
    size_t size;
    size_t align;
};

#define serene_meta(T)                                                         \
    (struct serene_Ptrmeta) { .size = sizeof(T), .align = alignof(typeof(T)) }
#define serene_nmeta(n, T)                                                     \
    (struct serene_Ptrmeta) {                                                  \
        .size = sizeof(T) * n, .align = alignof(typeof(T))                     \
    }
#define serene_tmeta(s, T)                                                     \
    (struct serene_Ptrmeta) {                                                  \
        .size = sizeof(T) + s, .align = alignof(typeof(T))                     \
    }

#define serene_alloc(a, T) (T*) a.alloc(a.ctx, serene_meta(T))
#define serene_nalloc(a, n, T) (T*) a.alloc(a.ctx, serene_nmeta(n, T))
#define serene_talloc(a, s, T) (T*) a.alloc(a.ctx, serene_tmeta(s, T))

#define serene_free(a, p) a.free(a.ctx, p, serene_meta(*p))
#define serene_nfree(a, n, p) a.free(a.ctx, p, serene_nmeta(n, *p))
#define serene_tfree(a, s, p) a.free(a.ctx, p, serene_tmeta(s, *p))

struct serene_Allocator {
    void *ctx;
    void *(*alloc)(void *, struct serene_Ptrmeta);
    void (*free)(void *, void *, struct serene_Ptrmeta);
};

struct serene_Arena {
    struct serene_Allocator backing;
    struct serene_ArenaLL {
        struct serene_ArenaLL *next;
        char *cap;
        char buf[];
    }* segments;
    char *bump;
};

struct serene_Allocator serene_Arena_dyn(struct serene_Arena *);
void *serene_Arena_alloc(void *, struct serene_Ptrmeta);
void serene_Arena_free(void *, void *, struct serene_Ptrmeta);
void serene_Arena_deinit(struct serene_Arena *);

struct serene_Allocator serene_Libc_dyn();
void *serene_Libc_alloc(void *, struct serene_Ptrmeta);
void serene_Libc_free(void *, void *, struct serene_Ptrmeta);

struct serene_Trea_Sub;
struct serene_Trea {
    struct serene_Allocator backing;
    struct serene_Trea_Sub* current;
};

#define serene_trealloc(a, T) ((T*) serene_Trea_alloc(a, serene_meta(T)))
#define serene_trenalloc(a, n, T) ((T*) serene_Trea_alloc(a, serene_nmeta(n, T)))
#define serene_tretalloc(a, s, T) ((T*) serene_Trea_alloc(a, serene_tmeta(s, T)))

struct serene_Allocator serene_Trea_dyn(struct serene_Trea*);
void* serene_Trea_alloc(struct serene_Trea*, struct serene_Ptrmeta);
void* serene_Trea_dyn_alloc(void*, struct serene_Ptrmeta);
void serene_Trea_dyn_free(void*, void* ptr, struct serene_Ptrmeta);
struct serene_Trea serene_Trea_init(struct serene_Allocator backing);
void serene_Trea_deinit(struct serene_Trea);
struct serene_Trea serene_Trea_sub(struct serene_Trea* root);

#endif
