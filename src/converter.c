#include "converter.h"
#include "common_ll.h"
#include <assert.h>

struct Context {
    struct serene_Allocator alloc;
    struct TypeIntern *intern;
};

static struct tst_Function convert_func(struct Context *, struct Function);
static struct tst_Expr convert_expr(struct Context *, struct Expr);
static struct tst_Binding convert_binding(struct Context *, struct Binding);
static struct tst_Type convert_type(struct Context *, const struct Type *, const struct Type *);

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
        if (!funcs_last)
            out.funcs = tmp;
        else
            funcs_last->next = tmp;
        funcs_last = tmp;

        funcs_last->current = convert_func(&ctx, head->current);
    }

    return out;
}

static struct tst_Function
convert_func(struct Context *ctx, struct Function func) {
    struct tst_Expr body = convert_expr(ctx, func.body);
    struct tst_Binding args = convert_binding(ctx, func.args);
    struct tst_Type type;
    {
        const struct Type *ret = func.ret;
        const struct Type *args = Binding_to_type(ctx->intern, func.args);
        type = convert_type(
            ctx,
            Type_func(ctx->intern, args, ret),
            NULL
        );
    }
    return (struct tst_Function){
        .name = func.name,
        .type = type,
        .args = args,
        .body = body,
    };
}

static struct tst_Expr convert_expr(struct Context *ctx, struct Expr expr) {
    struct tst_Type type = convert_type(ctx, expr.type, NULL);
    switch (expr.tag) {
    case ET_NumberLit:
        return (struct tst_Expr){
            .tag = TET_NumberLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_StringLit:
        return (struct tst_Expr){
            .tag = TET_StringLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_BoolLit:
        return (struct tst_Expr){
            .tag = TET_BoolLit,
            .type = type,
            .lit = expr.lit,
        };
    case ET_Recall:
        return (struct tst_Expr
        ){.tag = TET_Recall, .type = type, .lit = expr.lit};
    case ET_Unit:
        return (struct tst_Expr){
            .tag = TET_Unit,
            .type = type,
        };
    case ET_If: {
        struct tst_Expr *cond = serene_alloc(ctx->alloc, struct tst_Expr);
        struct tst_Expr *smash = serene_alloc(ctx->alloc, struct tst_Expr);
        struct tst_Expr *pass = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(cond && smash && pass && "OOM");
        *cond = convert_expr(ctx, *expr.if_expr.cond);
        *smash = convert_expr(ctx, *expr.if_expr.smash);
        *pass = convert_expr(ctx, *expr.if_expr.pass);
        return (struct tst_Expr){
            .tag = TET_If,
            .type = type,
            .if_expr.cond = cond,
            .if_expr.smash = smash,
            .if_expr.pass = pass,
        };
    }
    case ET_Loop: {
        struct tst_Expr *body = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(body && "OOM");
        *body = convert_expr(ctx, *expr.loop);
        return (struct tst_Expr){
            .tag = TET_Loop,
            .type = type,
            .loop = body,
        };
    }
    case ET_Bareblock: {
        struct tst_ExprsLL *body = NULL;
        struct tst_ExprsLL *last = NULL;
        for (ll_iter(head, expr.bareblock)) {
            struct tst_ExprsLL *tmp =
                serene_alloc(ctx->alloc, struct tst_ExprsLL);
            assert(tmp && "OOM");
            if (!last)
                body = tmp;
            else
                last->next = tmp;
            last = tmp;

            last->current = convert_expr(ctx, head->current);
        }
        return (struct tst_Expr){
            .tag = TET_Bareblock,
            .type = type,
            .bareblock = body,
        };
    }
    case ET_Call: {
        struct tst_Expr *name = serene_alloc(ctx->alloc, struct tst_Expr);
        struct tst_Expr *args = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(name && args && "OOM");
        *name = convert_expr(ctx, *expr.call.name);
        *args = convert_expr(ctx, *expr.call.args);
        return (struct tst_Expr){
            .tag = TET_Call,
            .type = type,
            .call.name = name,
            .call.args = args,
        };
    }
    case ET_Comma: {
        struct tst_Expr *lhs = serene_alloc(ctx->alloc, struct tst_Expr);
        struct tst_Expr *rhs = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(lhs && rhs && "OOM");
        *lhs = convert_expr(ctx, *expr.comma.lhs);
        *rhs = convert_expr(ctx, *expr.comma.rhs);
        return (struct tst_Expr){
            .tag = TET_Comma,
            .type = type,
            .comma.lhs = lhs,
            .comma.rhs = rhs,
        };
    }

        // reify them, as we already checked no let's are mutated
    case ST_Mut:
    case ST_Let: {
        struct tst_Binding bind = convert_binding(ctx, expr.let.bind);
        struct tst_Expr *init = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(init && "OOM");
        *init = convert_expr(ctx, *expr.let.init);
        return (struct tst_Expr){
            .tag = TST_Let,
            .type = type,
            .let.bind = bind,
            .let.init = init,
        };
    }
    case ST_Break: {
        struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(ctx, *expr.break_stmt);
        return (struct tst_Expr){
            .tag = TST_Break,
            .type = type,
            .break_stmt = e,
        };
    }
    case ST_Return: {
        struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(ctx, *expr.return_stmt);
        return (struct tst_Expr){
            .tag = TST_Return,
            .type = type,
            .return_stmt = e,
        };
    }
    case ST_Assign: {
        struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(ctx, *expr.assign.expr);
        return (struct tst_Expr){
            .tag = TST_Assign,
            .type = type,
            .assign.name = expr.assign.name,
            .assign.expr = e,
        };
    }
    case ST_Const: {
        struct tst_Expr *e = serene_alloc(ctx->alloc, struct tst_Expr);
        assert(e && "OOM");
        *e = convert_expr(ctx, *expr.const_stmt);
        return (struct tst_Expr){
            .tag = TST_Const,
            .type = type,
            .const_stmt = e,
        };
    }
    }

    assert(false && "why");
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
    case BT_Comma: {
        struct tst_Binding *lhs = serene_alloc(ctx->alloc, struct tst_Binding);
        struct tst_Binding *rhs = serene_alloc(ctx->alloc, struct tst_Binding);
        assert(lhs && rhs && "OOM");
        *lhs = convert_binding(ctx, *binding.comma.lhs);
        *rhs = convert_binding(ctx, *binding.comma.rhs);
        return (struct tst_Binding) {
            .tag = TBT_Comma,
            .comma.lhs = lhs,
            .comma.rhs = rhs
        };
    }
    }
    assert(false && "christ");
}

static struct tst_Type convert_type(struct Context *ctx, const struct Type *type, const struct Type *params) {
    switch (type->tag) {
    case TT_Var:
        assert(false && "should've been eliminated in fill_type");
    case TT_Recall: {
        enum tst_TypeTag tag;
        if (ctx->intern->syms.s_unit == type->recall) {
            tag = TTT_Unit;
        } else if (ctx->intern->syms.s_int == type->recall) {
            tag = TTT_Int;
        } else if (ctx->intern->syms.s_bool == type->recall) {
            tag = TTT_Bool;
        } else if (ctx->intern->syms.s_string == type->recall) {
            tag = TTT_String;
        } else if (ctx->intern->syms.s_star == type->recall) {
            assert(params && params->tag == TT_Comma && "expected two parameters");
            struct tst_Type *lhs = serene_alloc(ctx->alloc, struct tst_Type);
            struct tst_Type *rhs = serene_alloc(ctx->alloc, struct tst_Type);
            assert(lhs && rhs && "OOM");
            *lhs = convert_type(ctx, params->comma.lhs, NULL);
            *rhs = convert_type(ctx, params->comma.rhs, NULL);
            return (struct tst_Type){
                .tag = TTT_Star,
                .star.lhs = lhs,
                .star.rhs = rhs,
            };
        } else {
            assert(false && "TODO");
        }
        assert(!params && "expected no parameters");
        return (struct tst_Type) {
            .tag = tag,
        };
    }
    case TT_Func: {
        struct tst_Type *args = serene_alloc(ctx->alloc, struct tst_Type);
        struct tst_Type *ret = serene_alloc(ctx->alloc, struct tst_Type);
        assert(args && ret && "OOM");
        *args = convert_type(ctx, type->func.args, NULL);
        *ret = convert_type(ctx, type->func.ret, NULL);
        assert(!params && "expected no parameters");
        return (struct tst_Type){
            .tag = TTT_Func,
            .func.args = args,
            .func.ret = ret,
        };
    }
    case TT_Call:
        assert(!params && "should have no parameters");
        return convert_type(ctx, type->call.name, type->call.args);
    case TT_Comma:
        assert(false && "shouldn't occur in convert_type on it's own!");
    }

    assert(false && "aaaa");
}
