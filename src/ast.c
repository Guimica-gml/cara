#include "./ast.h"
#include <assert.h>

const struct Type *Binding_to_type(struct TypeIntern *intern, struct Binding this) {
    switch (this.tag) {
    case BT_Empty:
        return intern->tsyms.t_unit;
    case BT_Name:
        return this.name.annot;
    case BT_Comma: {
        const struct Type *lhs = Binding_to_type(intern, *this.comma.lhs);
        const struct Type *rhs = Binding_to_type(intern, *this.comma.rhs);
        return Type_product(intern, lhs, rhs);
    }
    }
    assert(false && "gcc complains about control reaching here??");
}

static struct Type *Type_copy(struct serene_Allocator alloc, const struct Type *t) {
    struct Type *out = serene_alloc(alloc, struct Type);
    assert(out && "OOM");
    out->tag = t->tag;
    switch (t->tag) {
    case TT_Func:
        out->func.args = Type_copy(alloc, t->func.args);
        out->func.ret = Type_copy(alloc, t->func.ret);
        break;
    case TT_Call:
        out->call.name = Type_copy(alloc, t->call.name);
        out->call.args = Type_copy(alloc, t->call.args);
        break;
    case TT_Comma:
        out->comma.lhs = Type_copy(alloc, t->comma.lhs);
        out->comma.rhs = Type_copy(alloc, t->comma.rhs);
        break;
    case TT_Recall:
        out->recall = t->recall;
        break;
    case TT_Var:
        out->var = t->var;
        break;
    }
    return out;
}

struct TypeIntern
TypeIntern_init(struct serene_Allocator alloc, struct Symbols syms) {
    struct Type t_unit = {.tag = TT_Recall, .recall = syms.s_unit};
    struct Type t_bool = {.tag = TT_Recall, .recall = syms.s_bool};
    struct Type t_int = {.tag = TT_Recall, .recall = syms.s_int};
    struct Type t_string = {.tag = TT_Recall, .recall = syms.s_string};
    struct Type t_star = {.tag = TT_Recall, .recall = syms.s_star};

    struct TypeIntern out = {0};
    out.alloc = alloc;
    out.syms = syms;
    out.tsyms = (struct Typesyms){
#define ins(t) .t = TypeIntern_intern(&out, &t)
        ins(t_unit), ins(t_bool), ins(t_int), ins(t_string),
        ins(t_star),
#undef ins
    };
    return out;
}

const struct Type *TypeIntern_intern(struct TypeIntern *this, struct Type *t) {
    struct Type **entry = Typereg_search(&this->tree, t);
    if (entry) return *entry;

    struct Type *new = Type_copy(this->alloc, t);
    assert(Typereg_insert(&this->tree, this->alloc, new));
    return new;
}

const struct Type *Type_recall(struct TypeIntern *intern, const char *name) {
    struct Type recall = {.tag = TT_Recall, .recall = name};
    return TypeIntern_intern(intern, &recall);
}

const struct Type *Type_func(
    struct TypeIntern *intern, const struct Type *args, const struct Type *ret
) {
    struct Type func = {.tag = TT_Func, .func.args = args, .func.ret = ret};
    return TypeIntern_intern(intern, &func);
}

const struct Type *Type_comma(
    struct TypeIntern *intern, const struct Type *lhs, const struct Type *rhs
) {
    struct Type comma = {.tag = TT_Comma, .comma.lhs = lhs, .comma.rhs = rhs};
    return TypeIntern_intern(intern, &comma);
}

const struct Type *Type_call(
    struct TypeIntern *intern, const struct Type *name, const struct Type *args
) {
    struct Type call = {.tag = TT_Call, .call.name = name, .call.args = args};
    return TypeIntern_intern(intern, &call);
}

const struct Type *Type_product(
    struct TypeIntern *intern, const struct Type *lhs, const struct Type *rhs
) {
    struct Type prod = {
        .tag = TT_Call,
        .call.args = Type_comma(intern, lhs, rhs),
        .call.name = intern->tsyms.t_star,
    };
    return TypeIntern_intern(intern, &prod);
}
