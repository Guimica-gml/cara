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
            struct Type* args;
            struct Type* ret;
        } func;
        struct {
            struct Type* name;
            struct Type* args;
        } call;
        struct {
            const char* name;
        } recall;
        struct {
            struct Type *lhs;
            struct Type* rhs;
        } comma;
        struct {
            int idx;
        } var;
    };
};

#endif
