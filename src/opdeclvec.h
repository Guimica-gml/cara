#ifndef opdeclvec_H
#define opdeclvec_H

#include "opdecl.h"
#include <stdbool.h>
#include <stddef.h>

struct Opdecls {
    size_t len;
    size_t cap;
    struct Opdecl *buf;
};

struct Opdecl *Opdecls_at(struct Opdecls *, size_t);
bool Opdecls_init(struct Opdecls *);
bool Opdecls_grow(struct Opdecls *);
bool Opdecls_push(struct Opdecls *, struct Opdecl);
struct Opdecl *Opdecls_first(struct Opdecls *);
struct Opdecl *Opdecls_last(struct Opdecls *);
struct Opdecl *Opdecls_emplace(struct Opdecls *);
void Opdecls_deinit(struct Opdecls *);
size_t Opdecls_space(struct Opdecls *);

#endif
