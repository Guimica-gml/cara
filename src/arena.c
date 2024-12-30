#include <sys/mman.h>
#include "./arena.h"

void* arena_alloc(struct Arena* this, size_t size) {
    size_t bump = this->bump;
    bump += 15;
    bump &= ~15;
    if (bump + size >= this->cap) return NULL;
    this->bump = bump + size;
    return (void*) (this->buf + bump);
}

bool arena_init(struct Arena* arena, size_t size) {
    char* buf = mmap(
        NULL, size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );
    if (buf == MAP_FAILED) return false;
    arena->buf = buf;
    arena->bump = 0;
    arena->cap = size;
    return true;
}

void arena_deinit(struct Arena* this) {
    munmap(this->buf, this->cap);
}
