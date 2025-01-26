#include "./ast.h"
#include "common_ll.h"
#include <assert.h>
#include <stdio.h>

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
    printf("interning: ");
    Type_print(t);
    printf("\n");
    struct Type **entry = Typereg_search(&this->tree, t);
    if (entry) {
        printf("found (%p)!\n\n", entry);
        return *entry;
    }
    printf("found nothing!\ninserting!\n");
    
    struct Type *new = Type_copy(this->alloc, t);
    assert(Typereg_insert(&this->tree, this->alloc, new));
    printf("\n\n");
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

static void Binding_print(struct Binding *binding) {
    switch (binding->tag) {
    case BT_Empty: printf("()"); break;
    case BT_Name:
        printf("%s: ", binding->name.name);
        Type_print(binding->name.annot);
        break;
    case BT_Comma:
        Binding_print(binding->comma.lhs);
        printf(", ");
        Binding_print(binding->comma.rhs);
    }
}

static void Expr_print(struct Expr *expr, int level) {
    switch (expr->tag) {
    case ET_Unit: printf("()"); break;
    case ET_NumberLit:
    case ET_StringLit:
    case ET_BoolLit:
    case ET_Recall: printf("%s", expr->lit); break;

    case ET_If:
        printf("if ");
        Expr_print(expr->if_expr.cond, level);
        printf(" ");
        Expr_print(expr->if_expr.smash, level);
        printf(" else ");
        Expr_print(expr->if_expr.pass, level);
        break;
    case ET_Loop:
        printf("loop ");
        Expr_print(expr->loop, level);
        break;
    case ET_Bareblock: {
        printf("{\n");
        for (ll_iter(h, expr->bareblock)) {
            for (int i = 0; i < level+1; i++) printf("  ");
            Expr_print(&h->current, level+1);
            printf("\n");
        }
        for (int i = 0; i < level; i++) printf("  ");
        printf("}");
        break;
    }
    case ET_Call:
        Expr_print(expr->call.name, level);
        printf("(");
        Expr_print(expr->call.args, level+1);
        printf(")");
        break;
    case ET_Comma:
        Expr_print(expr->comma.lhs, level);
        printf(", ");
        Expr_print(expr->comma.rhs, level);
        break;

    case ST_Let:
        printf("let ");
        Binding_print(&expr->let.bind);
        printf(" = ");
        Expr_print(expr->let.init, level);
        break;
    case ST_Mut:
        printf("mut ");
        Binding_print(&expr->let.bind);
        printf(" = ");
        Expr_print(expr->let.init, level);
        break;
    case ST_Break:
        printf("break ");
        Expr_print(expr->break_stmt, level);
        break;
    case ST_Return:
        printf("return ");
        Expr_print(expr->return_stmt, level);
        break;
    case ST_Assign:
        printf("%s = ", expr->assign.name);
        Expr_print(expr->assign.expr, level);
        break;
    case ST_Const:
        Expr_print(expr->const_stmt, level);
        printf(";");
        break;
    }
}

static void Function_print(struct Function *func, int level) {
    for (int i = 0; i < level; i++) printf("  ");
    printf("func %s(", func->name);
    Binding_print(&func->args);
    printf("): ");
    Type_print(func->ret);
    printf(" ");
    Expr_print(&func->body, level);
}

void Ast_print(struct Ast *ast) {
    for (ll_iter(f, ast->funcs)) {
        Function_print(&f->current, 0);
        printf("\n");
    }
}
