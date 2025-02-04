#ifndef TYPES_H
#define TYPES_H

enum TypeTag {
    TT_Func,
    TT_Call,
    TT_Recall,
    TT_Comma,
    TT_Var,
};

struct TypeFunc {
    const struct Type *args;
    const struct Type *ret;
};
struct TypeCall {
    const struct Type *name;
    const struct Type *args;
};
struct TypeComma {
    const struct Type *lhs;
    const struct Type *rhs;
};

struct Type {
    enum TypeTag tag;
    union {
        struct TypeFunc func;
        struct TypeCall call;
        struct TypeComma comma;
        const char *recall;
        int var;
    };
};

int Type_cmp(const struct Type *, const struct Type *);
void Type_print(const struct Type *);

#endif
