#ifndef @filename@_H
#define @filename@_H

#include "@include@"
#include <stdbool.h>
#include <stddef.h>

struct @vecname@ {
    size_t len;
    size_t cap;
    @basetype@ *buf;
};

@basetype@ *@vecname@_at(struct @vecname@ *, size_t);
bool @vecname@_init(struct @vecname@ *);
bool @vecname@_grow(struct @vecname@ *);
bool @vecname@_push(struct @vecname@ *, @basetype@);
@basetype@ *@vecname@_first(struct @vecname@ *);
@basetype@ *@vecname@_last(struct @vecname@ *);
@basetype@ *@vecname@_emplace(struct @vecname@ *);
void @vecname@_deinit(struct @vecname@ *);
size_t @vecname@_space(struct @vecname@ *);

#endif
