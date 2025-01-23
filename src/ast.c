#include "./ast.h"
#include <assert.h>

struct Type Binding_to_type(struct Symbols syms, struct Binding this) {
    switch (this.tag) {
    case BT_Empty:
        return (struct Type){
            .tag = TT_Recall,
            .recall = syms.s_unit,
        };
    case BT_Name:
        return this.name.annot;
    }
    assert(false && "gcc complains about control reaching here??");
}

struct Type Type_unit(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall = syms.s_unit};
}

struct Type Type_bool(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall = syms.s_bool};
}

struct Type Type_int(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall = syms.s_int};
}

struct Type Type_string(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall = syms.s_string};
}

struct Type Type_product(
    struct serene_Allocator alloc, struct Symbols syms, struct Type lhs,
    struct Type rhs
) {
    struct Type *plhs = serene_alloc(alloc, struct Type);
    struct Type *prhs = serene_alloc(alloc, struct Type);
    struct Type *args = serene_alloc(alloc, struct Type);
    struct Type *name = serene_alloc(alloc, struct Type);
    assert(plhs && prhs && args && name && "OOM");
    *plhs = lhs;
    *prhs = rhs;
    *args = (struct Type){
        .tag = TT_Comma,
        .comma.lhs = plhs,
        .comma.rhs = prhs,
    };
    *name = (struct Type){
        .tag = TT_Recall,
        .recall = syms.s_star,
    };
    return (struct Type){
        .tag = TT_Call,
        .call.args = args,
        .call.name = name,
    };
}
