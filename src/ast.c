#include "./ast.h"

struct Type Type_unit(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall.name = syms.s_unit};
}

struct Type Type_bool(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall.name = syms.s_bool};
}

struct Type Type_int(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall.name = syms.s_int};
}

struct Type Type_string(struct Symbols syms) {
    return (struct Type){.tag = TT_Recall, .recall.name = syms.s_string};
}
