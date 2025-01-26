#include "./types.h"
#include <assert.h>

int Type_cmp(const struct Type *a, const struct Type *b) {
    int diff = a->tag - b->tag;
    if (diff) return diff;

    switch (a->tag) {
    case TT_Func:
        diff = Type_cmp(a->func.args, b->func.args);
        if (diff) return diff;
        return Type_cmp(a->func.ret, b->func.ret);
    case TT_Call:
        diff = Type_cmp(a->call.name, b->call.name);
        if (diff) return diff;
        return Type_cmp(a->call.args, b->call.args);
    case TT_Comma:
        diff = Type_cmp(a->comma.lhs, b->comma.lhs);
        if (diff) return diff;
        return Type_cmp(a->comma.rhs, b->comma.rhs);
    case TT_Recall:
        return a->recall - b->recall;
    case TT_Var:
        return a->var - b->var;
    }

    assert(0 && "no gcc, control doesn't reach here");
}
