#ifndef TYPES_H
#define TYPES_H

enum TypeTag {
    TT_Func,
    TT_Call,
    TT_Recall,
    TT_Comma,
    TT_Var,
};
struct Type {
    enum TypeTag tag;
    union {
        struct {
            struct Type const *args;
            struct Type const *ret;
        } func;
        struct {
            struct Type const *name;
            struct Type const *args;
        } call;
        struct {
            struct Type const *lhs;
            struct Type const *rhs;
        } comma;
        const char *recall;
        int var;
    };
};

int Type_cmp(const struct Type *, const struct Type *);

#endif
