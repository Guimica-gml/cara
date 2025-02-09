#include "converter.h"
#include "common_ll.h"
#include <assert.h>

struct Context {
    struct serene_Allocator alloc;
    struct TypeIntern *intern;
};

static struct tst_Function convert_func(struct Context *, struct Function);
static struct tst_Expr convert_expr(struct Context *, struct Expr *);
static struct tst_Binding convert_binding(struct Context *, struct Binding);
static struct tst_Type convert_type(
    struct Context*,
    const struct Type*,
    const struct Type*
);

struct Tst convert_ast(
    struct serene_Allocator alloc, struct TypeIntern *intern, struct Ast ast
) {
    struct Context ctx = {
        .alloc = alloc,
        .intern = intern,
    };
    struct Tst out = {0};
    struct tst_FunctionsLL *funcs_last = NULL;

    for (ll_iter(head, ast.funcs)) {
        struct tst_FunctionsLL *tmp =
            serene_alloc(alloc, struct tst_FunctionsLL);
        assert(tmp && "OOM");
        *tmp = (struct tst_FunctionsLL){0};
        if (!funcs_last)
            out.funcs = tmp;
        else
            funcs_last->next = tmp;
        funcs_last = tmp;

        funcs_last->current = convert_func(&ctx, head->current);
    }

    return out;
}

static struct tst_Function convert_func(struct Context *ctx, struct Function func) {
    struct tst_Expr body = convert_expr(ctx, &func.body);
    struct tst_Binding args = convert_binding(ctx, func.args);
    struct tst_Type type;
    {
        const struct Type *ret = func.ret;
        const struct Type *args = Binding_to_type(ctx->intern, func.args);
        type = convert_type(ctx, Type_func(ctx->intern, args, ret), NULL);
    }
    return (struct tst_Function){
        .name = func.name,
        .type = type,
        .args = args,
        .body = body,
    };
}

static struct tst_Expr convert_ET_If(struct Context* ctx, struct ExprIf* expr, struct tst_Type type),
    convert_ET_Loop(struct Context* ctx, struct Expr* body, struct tst_Type type),
    convert_ET_Bareblock(struct Context* ctx, struct ExprsLL* body, struct tst_Type type),
    convert_ET_Call(struct Context* ctx, struct ExprCall* expr, struct tst_Type type),
    convert_ET_Recall(struct Context *ctx, const char *lit, struct tst_Type type),
    convert_ET_Tuple(struct Context *ctx, struct ExprTuple expr, struct tst_Type type),
    convert_ST_Let(struct Context *ctx, struct ExprLet *expr, struct tst_Type type),
    convert_ST_Break(struct Context *ctx, struct Expr *body, struct tst_Type type),
    convert_ST_Return(struct Context *ctx, struct Expr *body, struct tst_Type type),
    convert_ST_Assign(struct Context *ctx, struct ExprAssign *expr, struct tst_Type type),
    convert_ST_Const(struct Context *ctx, struct Expr *body, struct tst_Type type);

static struct tst_Expr convert_expr(struct Context *ctx, struct Expr *expr) {
    struct tst_Type type = convert_type(ctx, expr->type, NULL);

#define Case(Tag, ...) \
    case Tag: return convert_##Tag(ctx, __VA_ARGS__);

    switch (expr->tag) {
        Case(ET_If, expr->if_expr, type);
        Case(ET_Loop, expr->loop, type);
        Case(ET_Bareblock, expr->bareblock, type);
        Case(ET_Call, expr->call, type);
        Case(ET_Recall, expr->lit, type);
        Case(ET_Tuple, expr->tuple, type);
        Case(ST_Break, expr->break_stmt, type);
        Case(ST_Return, expr->return_stmt, type);
        Case(ST_Assign, expr->assign, type);
        Case(ST_Const, expr->const_stmt, type);

        // reify them, as we already checked no let's are mutated
    case ST_Mut:
        Case(ST_Let, expr->let, type);

    case ET_NumberLit: return (struct tst_Expr){
        .tag = TET_NumberLit,
        .type = type,
        .lit = expr->lit,
    };
    case ET_StringLit: return (struct tst_Expr){
        .tag = TET_StringLit,
        .type = type,
        .lit = expr->lit,
    };
    case ET_BoolLit: return (struct tst_Expr){
        .tag = TET_BoolLit,
        .type = type,
        .lit = expr->lit,
    };
    case ET_Unit: return (struct tst_Expr){
        .tag = TET_Unit,
        .type = type,
    };
    }

#undef Case
    assert(false && "why");
}

static struct tst_Expr convert_ET_If(struct Context *ctx, struct ExprIf *expr, struct tst_Type type) {
    struct tst_ExprIf *if_expr = serene_alloc(ctx->alloc, struct tst_ExprIf);
    assert(if_expr && "OOM");
    if_expr->cond = convert_expr(ctx, &expr->cond);
    if_expr->smash = convert_expr(ctx, &expr->smash);
    if_expr->pass = convert_expr(ctx, &expr->pass);
    return (struct tst_Expr){
        .tag = TET_If,
        .type = type,
        .if_expr = if_expr,
    };
}

static struct tst_Expr convert_ET_Loop(struct Context* ctx, struct Expr* body, struct tst_Type type) {
    struct tst_Expr* new = serene_alloc(ctx->alloc, struct tst_Expr);
    assert(body && "OOM");
    *new = convert_expr(ctx, body);
    return (struct tst_Expr){
        .tag = TET_Loop,
        .type = type,
        .loop = new,
    };
}

static struct tst_Expr convert_ET_Bareblock(struct Context* ctx, struct ExprsLL* body, struct tst_Type type) {
    struct tst_ExprsLL* new = NULL;
    struct tst_ExprsLL* last = NULL;
    for (ll_iter(head, body)) {
        struct tst_ExprsLL* tmp =
            serene_alloc(ctx->alloc, struct tst_ExprsLL);
        assert(tmp && "OOM");
        *tmp = (struct tst_ExprsLL){0};
        if (!last)
            new = tmp;
        else
            last->next = tmp;
        last = tmp;

        last->current = convert_expr(ctx, &head->current);
    }
    return (struct tst_Expr){
        .tag = TET_Bareblock,
        .type = type,
        .bareblock = new,
    };
}

static struct tst_Expr convert_ET_Call(struct Context* ctx, struct ExprCall* expr, struct tst_Type type) {
    struct tst_ExprCall* call = serene_alloc(ctx->alloc, struct tst_ExprCall);
    assert(call && "OOM");
    call->name = convert_expr(ctx, &expr->name);
    call->args = convert_expr(ctx, &expr->args);
    return (struct tst_Expr){
        .tag = TET_Call,
        .type = type,
        .call = call,
    };
}


static struct tst_Expr convert_ET_Recall(struct Context* ctx, const char* lit, struct tst_Type type) {
#define builtin(sym) \
    (lit == ctx->intern->syms.sym)
#define mk_builtin(def) \
    { return (struct tst_Expr){.tag = TET_Builtin, .builtin = def}; }

    if builtin (s_badd) mk_builtin(EB_badd);
    if builtin (s_bsub) mk_builtin(EB_bsub);
    if builtin (s_bmul) mk_builtin(EB_bmul);
    if builtin (s_bdiv) mk_builtin(EB_bdiv);
    if builtin (s_bmod) mk_builtin(EB_bmod);
    if builtin (s_bneg) mk_builtin(EB_bneg);
    if builtin (s_band) mk_builtin(EB_band);
    if builtin (s_bor) mk_builtin(EB_bor);
    if builtin (s_bxor) mk_builtin(EB_bxor);
    if builtin (s_bnot) mk_builtin(EB_bnot);
    if builtin (s_bshl) mk_builtin(EB_bshl);
    if builtin (s_bshr) mk_builtin(EB_bshr);
    if builtin (s_bcmpEQ) mk_builtin(EB_bcmpEQ);
    if builtin (s_bcmpNE) mk_builtin(EB_bcmpNE);
    if builtin (s_bcmpGT) mk_builtin(EB_bcmpGT);
    if builtin (s_bcmpLT) mk_builtin(EB_bcmpLT);
    if builtin (s_bcmpGE) mk_builtin(EB_bcmpGE);
    if builtin (s_bcmpLE) mk_builtin(EB_bcmpLE);
    if builtin (s_syscall) mk_builtin(EB_syscall);
    if builtin (s_bptr_to_int) mk_builtin(EB_ptr_to_int);
    if builtin (s_bint_to_ptr) mk_builtin(EB_int_to_ptr);

    return (struct tst_Expr){
        .tag = TET_Recall,
        .type = type,
        .lit = lit
    };

#undef mk_builtin
#undef builtin
}

static struct tst_Expr convert_ET_Tuple(struct Context* ctx, struct ExprTuple expr, struct tst_Type type) {
    struct tst_ExprTuple *list = NULL;
    struct tst_ExprTuple *last = NULL;
    for (ll_iter(head, expr.list)) {
        struct tst_ExprTuple* tmp = serene_alloc(ctx->alloc, struct tst_ExprTuple);
        assert(tmp && "OOM");
        *tmp = (typeof(*tmp)){0};
        tmp->current = convert_expr(ctx, &head->current);
        if (!last) list = tmp;
        else last->next = tmp;
        last = tmp;
    }
    return (struct tst_Expr){
        .tag = TET_Tuple,
        .type = type,
        .tuple = list,
    };
}

static struct tst_Expr convert_ST_Let(struct Context* ctx, struct ExprLet* expr, struct tst_Type type) {
    struct tst_ExprLet *let = serene_alloc(ctx->alloc, struct tst_ExprLet);
    assert(let && "OOM");
    let->bind = convert_binding(ctx, expr->bind);
    let->init = convert_expr(ctx, &expr->init);
    return (struct tst_Expr){
        .tag = TST_Let,
        .type = type,
        .let = let,
    };
}

static struct tst_Expr convert_ST_Break(struct Context* ctx, struct Expr* body, struct tst_Type type) {
    struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
    assert(e && "OOM");
    *e = convert_expr(ctx, body);
    return (struct tst_Expr){
        .tag = TST_Break,
        .type = type,
        .break_stmt = e,
    };
}

static struct tst_Expr convert_ST_Return(struct Context* ctx, struct Expr* body, struct tst_Type type) {
    struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
    assert(e && "OOM");
    *e = convert_expr(ctx, body);
    return (struct tst_Expr){
        .tag = TST_Return,
        .type = type,
        .return_stmt = e,
    };
}

static struct tst_Expr convert_ST_Assign(struct Context* ctx, struct ExprAssign* expr, struct tst_Type type) {
    struct tst_ExprAssign *assign = serene_alloc(ctx->alloc, struct tst_ExprAssign);
    assert(assign && "OOM");
    assign->expr = convert_expr(ctx, &expr->expr);
    assign->name = expr->name;
    return (struct tst_Expr){
        .tag = TST_Assign,
        .type = type,
        .assign = assign,
    };
}

static struct tst_Expr convert_ST_Const(struct Context* ctx, struct Expr* body, struct tst_Type type) {
    struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
    assert(e && "OOM");
    *e = convert_expr(ctx, body);
    return (struct tst_Expr){
        .tag = TST_Const,
        .type = type,
        .const_stmt = e,
    };
}

static struct tst_Binding
convert_binding(struct Context *ctx, struct Binding binding) {
    switch (binding.tag) {
    case BT_Empty:
        return (struct tst_Binding){
            .tag = TBT_Empty,
        };
    case BT_Name:
        return (struct tst_Binding){
            .tag = TBT_Name,
            .name.name = binding.name.name,
            .name.type = convert_type(ctx, binding.name.annot, NULL),
        };
    case BT_Tuple: {
        struct tst_BindingTuple *list = NULL;
        struct tst_BindingTuple *last = NULL;
        for (ll_iter(head, binding.tuple)) {
            struct tst_BindingTuple* tmp = serene_alloc(ctx->alloc, struct BindingTuple);
            assert(tmp && "OOM");
            *tmp = (typeof(*tmp)){0};
            tmp->current = convert_binding(ctx, head->current);
            if (!last) list = tmp;
            else last->next = tmp;
            last = tmp;
        }
        return (struct tst_Binding){.tag = TBT_Tuple, .tuple = list};
    }
    }
    assert(false && "christ");
}

static struct tst_Type convert_TT_Recall(struct Context* ctx, const char* lit, const struct Type* params),
    convert_TT_Func(struct Context* ctx, const struct TypeFunc* func);

static struct tst_Type convert_type(
    struct Context *ctx, const struct Type *type, const struct Type *params
) {
    switch (type->tag) {
    case TT_Var: assert(false && "should've been eliminated in fill_type");
    case TT_Recall: return convert_TT_Recall(ctx, type->recall, params);
    case TT_Func: return convert_TT_Func(ctx, &type->func);
    case TT_Call:
        assert(!params && "should have no parameters");
        return convert_type(ctx, type->call.name, type->call.args);
    case TT_Tuple:
        assert(type->tuple == NULL && "only units should occur on their own here");
        return (struct tst_Type) {
            .tag = TTT_Unit,
        };
    }

    assert(false && "aaaa");
}


static struct tst_Type convert_TT_Recall(struct Context* ctx, const char* lit, const struct Type* params) {
    enum tst_TypeTag tag;
    if (ctx->intern->syms.s_unit == lit) {
        assert(false && "this should not be possible anymore");
    } else if (ctx->intern->syms.s_int == lit) {
        tag = TTT_Int;
    } else if (ctx->intern->syms.s_bool == lit) {
        tag = TTT_Bool;
    } else if (ctx->intern->syms.s_string == lit) {
        tag = TTT_String;
    } else if (ctx->intern->syms.s_star == lit) {
        assert(
            params && params->tag == TT_Tuple && "expected parameters"
        );
        // deliberately reverses the list
        // as later on we need the first tuple element to be the head
        // rather than the last, as we had previously
        struct tst_TypeStar* list = NULL;
        for (const struct TypeTuple* head = params->tuple; head; head = head->next) {
            struct tst_TypeStar* tmp = serene_alloc(ctx->alloc, struct tst_TypeStar);
            assert(tmp && "OOM");
            tmp->current = convert_type(ctx, head->current, NULL);
            tmp->next = list;
            list = tmp;
        }
        return (struct tst_Type){
            .tag = TTT_Star,
            .star = list,
        };
    } else {
        assert(false && "TODO");
    }
    assert(!params && "expected no parameters");
    return (struct tst_Type){
        .tag = tag,
    };
}

static struct tst_Type convert_TT_Func(struct Context* ctx, const struct TypeFunc* func) {
    struct tst_TypeFunc *new = serene_alloc(ctx->alloc, struct tst_TypeFunc);
    assert(func && "OOM");
    new->args = convert_type(ctx, func->args, NULL);
    new->ret = convert_type(ctx, func->ret, NULL);
    return (struct tst_Type){
        .tag = TTT_Func,
        .func = new,
    };
}
