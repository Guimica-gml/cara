#include "./parser.h"

struct Context {
    struct serene_Allocator alloc;
    struct Opdecls ops;
    struct TypeIntern *intern;
    struct Tokenstream toks;
    int var_counter;
};

static struct Type const *Context_new_typevar(struct Context *);

static struct Function decls_function(struct Context *);

static struct Type const *type(struct Context *);
static struct Type const *type_func(struct Context *);
static struct Type const *type_op(struct Context *, unsigned);
static struct Type const *type_op_left(struct Context *);
static bool type_op_right_first(struct Context *, unsigned);
static struct Type const *type_op_right(struct Context *, struct Type const *, unsigned);
static struct Type const *type_atom(struct Context *);
static struct Type const *type_parenthesised(struct Context *);

static struct Binding binding(struct Context *);
static struct Binding binding_atom(struct Context *);
static struct Binding binding_parenthesised(struct Context *);
static struct Binding binding_name(struct Context *);

static struct Expr expr_delimited(struct Context *);
static struct Expr expr_any(struct Context *);
static struct Expr expr_block(struct Context *);
static struct Expr expr_bareblock(struct Context *);
static struct Expr expr_if(struct Context *);
static struct Expr expr_loop(struct Context *);
static struct Expr expr_inline(struct Context *);
static struct Expr expr_op(struct Context *, unsigned);
static struct Expr expr_op_left(struct Context *);
static bool expr_op_right_first(struct Context *, unsigned);
static struct Expr expr_op_right(struct Context *, struct Expr, unsigned);
static struct Expr expr_atom(struct Context *);

static struct Expr statement(struct Context *);
static struct Expr statement_let(struct Context *);
static struct Expr statement_mut(struct Context *);
static struct Expr statement_break(struct Context *);
static struct Expr statement_return(struct Context *);
static struct Expr statement_assign(struct Context *);

struct Ast parse(
    struct serene_Allocator alloc, struct Opdecls ops, struct TypeIntern *intern,
    struct Tokenstream toks
) {
    struct Context ctx = {
        .alloc = alloc,
        .ops = ops,
        .intern = intern,
        .toks = toks,
        .var_counter = 0,
    };
    struct FunctionsLL *funcs = NULL;

    while (ctx.toks.len) {
        switch (ctx.toks.buf[0].kind) {
        case TK_Func: {
            struct FunctionsLL *tmp = serene_alloc(alloc, struct FunctionsLL);
            assert(tmp && "OOM");
            tmp->current = decls_function(&ctx);
            tmp->next = funcs;
            funcs = tmp;
            break;
        }
        default:
            goto after;
        }
    }
after:
    assert(ctx.toks.len == 0);

    return (struct Ast){
        .funcs = funcs,
    };
}

static struct Function decls_function(struct Context * ctx) {
    const char *name;
    struct Binding args;
    struct Type const *ret;
    struct Expr body;

    assert(Tokenstream_drop_text(&ctx->toks, "func"));

    assert(ctx->toks.len), name = ctx->toks.buf[0].spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));

    args = binding_parenthesised(ctx);
    if (Tokenstream_drop_text(&ctx->toks, ":")) {
        ret = type(ctx);
    } else {
        ret = ctx->intern->tsyms.t_unit;
    }

    if (Tokenstream_drop_text(&ctx->toks, "=")) {
        body = expr_delimited(ctx);
    } else {
        body = expr_bareblock(ctx);
        Tokenstream_drop_text(&ctx->toks, ";");
    };

    return (struct Function
    ){.name = name, .args = args, .ret = ret, .body = body};
}

static struct Type const *type(struct Context * ctx) {
    assert(ctx->toks.len);
    if (ctx->toks.buf[0].kind == TK_Func)
        return type_func(ctx);
    return type_op(ctx, 0);
}

static struct Type const *type_func(struct Context * ctx) {
    assert(Tokenstream_drop_text(&ctx->toks, "func"));
    struct Type const *args = type_parenthesised(ctx);
    assert(Tokenstream_drop_text(&ctx->toks, ":"));
    struct Type const *ret = type(ctx);
    return Type_func(ctx->intern, args, ret);
}

static struct Type const *type_op(struct Context * ctx, unsigned prec) {
    struct Type const *left;

    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_Name:
    case TK_OpenParen:
        left = type_atom(ctx);
        break;
    default:
        left = type_op_left(ctx);
    }
    while (type_op_right_first(ctx, prec)) {
        left = type_op_right(ctx, left, prec);
    }

    return left;
}

static struct Type const *type_op_left(struct Context * ctx) {
    struct Type const *args;
    struct Type const *name;

    assert(ctx->toks.len);
    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp >= 0 || ctx->ops.buf[i].rbp < 0)
            continue;
        if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
            continue;

        name = Type_recall(ctx->intern, ctx->toks.buf[0].spelling);

        assert(Tokenstream_drop(&ctx->toks));
        args = type_op(ctx, ctx->ops.buf[i].rbp);

        return Type_call(ctx->intern, name, args);
    }

    assert(false && "unexpected token");
}

static bool type_op_right_first(struct Context * ctx, unsigned prec) {
    if (!ctx->toks.len)
        return false;
    if (prec == 0 && ctx->toks.buf[0].kind == TK_Comma)
        return true;

    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen:
    case TK_Name:
        return true;
    default:
        break;
    }

    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp < (int)prec)
            continue;
        // same as for type_op_left()
        if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
            continue;
        return true;
    }

    return false;
}

static struct Type const *
type_op_right(struct Context *ctx, struct Type const *left, unsigned prec) {
    assert(ctx->toks.len);
    if (prec == 0 && ctx->toks.buf[0].kind == TK_Comma) {
        assert(Tokenstream_drop(&ctx->toks));
        struct Type const *right = type_op(ctx, 1);
        return Type_comma(ctx->intern, left, right);
    }

    struct Type const *name;
    struct Type const *args;
    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen:
    case TK_Name:
        name = left;
        args = type_atom(ctx);
        return Type_call(ctx->intern, name, args);
    default:
        for (size_t i = 0; i < ctx->ops.len; i++) {
            if (ctx->ops.buf[i].lbp < (int)prec)
                continue;
            if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
                continue;

            name = Type_recall(ctx->intern, ctx->toks.buf[0].spelling);
            assert(Tokenstream_drop(&ctx->toks));
            if (ctx->ops.buf[i].rbp >= 0) {
                const struct Type *right = type_op(ctx, ctx->ops.buf[i].rbp);
                args = Type_comma(ctx->intern, left, right);
            } else {
                args = left;
            }

            return Type_call(ctx->intern, name, args);
        }
    }

    assert(false && "unexpected token");
}

static const struct Type *type_atom(struct Context * ctx) {
    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen:
        return type_parenthesised(ctx);
    case TK_Name: {
        const char *name = ctx->toks.buf[0].spelling;
        Tokenstream_drop(&ctx->toks);

        return Type_recall(ctx->intern, name);
    }
    default:
        assert(false && "unexpected token encountered");
    }
}

static const struct Type *type_parenthesised(struct Context * ctx) {
    assert(Tokenstream_drop_text(&ctx->toks, "("));
    const struct Type *out = type(ctx);
    assert(Tokenstream_drop_text(&ctx->toks, ")"));
    return out;
}

static struct Binding binding(struct Context * ctx) { return binding_atom(ctx); }

static struct Binding binding_atom(struct Context * ctx) {
    assert(ctx->toks.len);
    if (ctx->toks.buf[0].kind == TK_OpenParen) {
        return binding_parenthesised(ctx);
    }
    return binding_name(ctx);
}

static struct Binding binding_parenthesised(struct Context * ctx) {
    struct Binding out = {0};
    assert(Tokenstream_drop_text(&ctx->toks, "("));
    assert(ctx->toks.len);
    if (ctx->toks.buf[0].kind != TK_CloseParen) {
        out = binding(ctx);
    } else {
        out.tag = BT_Empty;
    }
    assert(Tokenstream_drop_text(&ctx->toks, ")"));
    return out;
}

static struct Binding binding_name(struct Context * ctx) {
    const struct Type *annot;
    const char *name;

    assert(ctx->toks.len), name = ctx->toks.buf[0].spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));
    if (ctx->toks.len && ctx->toks.buf[0].kind == TK_Colon) {
        assert(Tokenstream_drop(&ctx->toks));
        annot = type(ctx);
    } else {
        annot = Context_new_typevar(ctx);
    }

    return (struct Binding){
        .tag = BT_Name,
        .name.name = name,
        .name.annot = annot,
    };
}

static struct Expr expr_delimited(struct Context * ctx) {
    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_If:
    case TK_Loop:
    case TK_OpenBrace: {
        struct Expr tmp = expr_block(ctx);
        Tokenstream_drop_text(&ctx->toks, ";");

        return tmp;
    }
    default:
        return expr_inline(ctx);
    }
}

static struct Expr expr_any(struct Context * ctx) {
    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_If:
    case TK_Loop:
    case TK_OpenBrace:
        return expr_block(ctx);
    default:
        return expr_inline(ctx);
    }
}

static struct Expr expr_block(struct Context * ctx) {
    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_If:
        return expr_if(ctx);
    case TK_Loop:
        return expr_loop(ctx);
    case TK_OpenBrace:
        return expr_bareblock(ctx);
    default:
        assert(false);
    }
}

static struct Expr expr_if(struct Context * ctx) {
    struct Expr *cond = serene_alloc(ctx->alloc, struct Expr);
    struct Expr *smash = serene_alloc(ctx->alloc, struct Expr);
    struct Expr *pass = serene_alloc(ctx->alloc, struct Expr);
    assert(cond && smash && pass && "OOM");

    assert(Tokenstream_drop_text(&ctx->toks, "if"));
    *cond = expr_any(ctx);
    *smash = expr_block(ctx);
    if (Tokenstream_drop_text(&ctx->toks, "else")) {
        *pass = expr_block(ctx);
    } else {
        *pass = (struct Expr){
            .tag = ET_Unit,
            .type = ctx->intern->tsyms.t_unit,
        };
    }

    return (struct Expr){
        .tag = ET_If,
        .type = smash->type,
        .if_expr.cond = cond,
        .if_expr.smash = smash,
        .if_expr.pass = pass,
    };
}

static struct Expr expr_loop(struct Context * ctx) {
    struct Expr *block = serene_alloc(ctx->alloc, struct Expr);
    assert(block && "OOM");

    assert(Tokenstream_drop_text(&ctx->toks, "loop"));
    *block = expr_block(ctx);

    return (struct Expr){
        .tag = ET_Loop,
        .type = Context_new_typevar(ctx),
        .loop = block,
    };
}

static struct Expr expr_bareblock(struct Context * ctx) {
    struct ExprsLL *block = NULL;
    struct ExprsLL *last = NULL;
    const struct Type *type = ctx->intern->tsyms.t_unit;

    assert(Tokenstream_drop_text(&ctx->toks, "{"));
    while (ctx->toks.len && ctx->toks.buf[0].kind != TK_CloseBrace) {
        {
            struct ExprsLL *tmp = serene_alloc(ctx->alloc, struct ExprsLL);
            assert(tmp && "OOM");
            if (!last)
                block = tmp;
            else
                last->next = tmp;
            last = tmp;
        }
        last->current = statement(ctx);

        if (Tokenstream_drop_text(&ctx->toks, ";")) {
            struct Expr *e = serene_alloc(ctx->alloc, struct Expr);
            assert(e && "OOM");
            *e = last->current;
            last->current = (struct Expr){
                .tag = ST_Const,
                .type = ctx->intern->tsyms.t_unit,
                .const_stmt = e,
            };
        } else {
            type = last->current.type;
            break;
        }
    }
    assert(Tokenstream_drop_text(&ctx->toks, "}"));

    return (struct Expr){.tag = ET_Bareblock, .type = type, .bareblock = block};
}

static struct Expr expr_inline(struct Context * ctx) { return expr_op(ctx, 0); }

static struct Expr expr_op(struct Context * ctx, unsigned prec) {
    struct Expr left;

    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_Name:
    case TK_OpenParen:
    case TK_Number:
    case TK_String:
    case TK_Bool:
        left = expr_atom(ctx);
        break;
    default:
        left = expr_op_left(ctx);
        break;
    }
    while (expr_op_right_first(ctx, prec)) {
        left = expr_op_right(ctx, left, prec);
    }

    return left;
}

static struct Expr expr_op_left(struct Context * ctx) {
    struct Expr *args = serene_alloc(ctx->alloc, struct Expr);
    struct Expr *name = serene_alloc(ctx->alloc, struct Expr);
    assert(args && name);

    assert(ctx->toks.len);
    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp >= 0 || ctx->ops.buf[i].rbp < 0)
            continue;
        if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
            continue;

        assert(Tokenstream_drop(&ctx->toks));
        *name = (struct Expr){
            .tag = ET_Recall,
            .type = Context_new_typevar(ctx),
            .lit = ctx->ops.buf[i].token,
        };
        *args = expr_op(ctx, ctx->ops.buf[i].rbp);
        return (struct Expr){
            .tag = ET_Call,
            .type = Context_new_typevar(ctx),
            .call.name = name,
            .call.args = args,
        };
    }

    assert(false && "unexpected token");
}

static bool expr_op_right_first(struct Context * ctx, unsigned prec) {
    if (!ctx->toks.len)
        return false;
    if (prec == 0 && ctx->toks.buf[0].kind == TK_Comma)
        return true;
    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen:
    case TK_Name:
    case TK_Number:
    case TK_String:
    case TK_Bool:
        return true;
    default:
        break;
    }

    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp < (int)prec)
            continue;
        if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
            continue;
        return true;
    }
    return false;
}

static struct Expr
expr_op_right(struct Context * ctx, struct Expr left, unsigned prec) {
    struct Expr *name = serene_alloc(ctx->alloc, struct Expr);
    struct Expr *args = serene_alloc(ctx->alloc, struct Expr);
    assert(name && args);

    assert(ctx->toks.len);
    if (prec == 0 && ctx->toks.buf[0].kind == TK_Comma) {
        assert(Tokenstream_drop(&ctx->toks));
        // reusing allocations, don't mind the names
        *name = left;
        *args = expr_op(ctx, 1);

        return (struct Expr){
            .tag = ET_Comma,
            .type = Type_product(ctx->intern, name->type, args->type),
            .comma.lhs = name,
            .comma.rhs = args,
        };
    }

    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen:
    case TK_Name:
    case TK_Number:
    case TK_String:
    case TK_Bool:
        *args = expr_atom(ctx);
        *name = left;

        return (struct Expr){
            .tag = ET_Call,
            .type = Context_new_typevar(ctx),
            .call.name = name,
            .call.args = args,
        };
    default:
        for (size_t i = 0; i < ctx->ops.len; i++) {
            if (ctx->ops.buf[i].lbp < (int)prec)
                continue;
            if (ctx->toks.buf[0].spelling != ctx->ops.buf[i].token)
                continue;

            assert(Tokenstream_drop(&ctx->toks));
            *name = (struct Expr){
                .tag = ET_Recall,
                .type = Context_new_typevar(ctx),
                .lit = ctx->ops.buf[i].token,
            };
            if (ctx->ops.buf[i].rbp >= 0) {
                struct Expr *left_ptr = serene_alloc(ctx->alloc, struct Expr);
                struct Expr *right_ptr = serene_alloc(ctx->alloc, struct Expr);
                assert(left_ptr && right_ptr);

                *left_ptr = left;
                *right_ptr = expr_op(ctx, ctx->ops.buf[i].rbp);
                *args = (struct Expr){
                    .tag = ET_Comma,
                    .type = Type_product(ctx->intern, left_ptr->type, right_ptr->type),
                    .comma.lhs = left_ptr,
                    .comma.rhs = right_ptr,
                };
            } else {
                *args = left;
            }

            return (struct Expr){
                .tag = ET_Call,
                .type = Context_new_typevar(ctx),
                .call.name = name,
                .call.args = args,
            };
        }
    }

    assert(false && "unexpected token");
}

static struct Expr expr_atom(struct Context * ctx) {
    enum ExprTag tag;
    const struct Type *type;

    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_OpenParen: {
        assert(Tokenstream_drop(&ctx->toks));
        struct Expr tmp = expr_any(ctx);
        assert(Tokenstream_drop_text(&ctx->toks, ")"));
        return tmp;
    }
    case TK_Name:
        tag = ET_Recall;
        type = Context_new_typevar(ctx);
        break;
    case TK_Number:
        tag = ET_NumberLit;
        type = ctx->intern->tsyms.t_int;
        break;
    case TK_String:
        tag = ET_StringLit;
        type = ctx->intern->tsyms.t_string;
        break;
    case TK_Bool:
        tag = ET_BoolLit;
        type = ctx->intern->tsyms.t_bool;
        break;
    default:
        assert(false && "unexpected token");
    }

    const char *name = ctx->toks.buf[0].spelling;
    assert(Tokenstream_drop(&ctx->toks));
    return (struct Expr){
        .tag = tag,
        .type = type,
        .lit = name,
    };
}

static struct Expr statement(struct Context * ctx) {
    assert(ctx->toks.len);
    switch (ctx->toks.buf[0].kind) {
    case TK_Let:
        return statement_let(ctx);
    case TK_Mut:
        return statement_mut(ctx);
    case TK_Break:
        return statement_break(ctx);
    case TK_Return:
        return statement_return(ctx);
    case TK_Name:
        if (ctx->toks.len > 1 && ctx->toks.buf[1].kind == TK_Equals) {
            return statement_assign(ctx);
        }
        __attribute__((fallthrough));
    default: {
        return expr_any(ctx);
    }
    }
}

static struct Expr statement_let(struct Context * ctx) {
    struct Expr *init = serene_alloc(ctx->alloc, struct Expr);
    struct Binding bind;
    assert(init && "OOM");

    assert(Tokenstream_drop_text(&ctx->toks, "let"));
    bind = binding(ctx);
    assert(Tokenstream_drop_text(&ctx->toks, "="));
    *init = expr_any(ctx);

    return (struct Expr){
        .tag = ST_Let,
        .type = ctx->intern->tsyms.t_unit,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Expr statement_mut(struct Context * ctx) {
    struct Expr *init = serene_alloc(ctx->alloc, struct Expr);
    struct Binding bind;
    assert(init && "OOM");

    assert(Tokenstream_drop_text(&ctx->toks, "mut"));
    bind = binding(ctx);
    assert(Tokenstream_drop_text(&ctx->toks, "="));
    *init = expr_any(ctx);

    return (struct Expr){
        .tag = ST_Mut,
        .type = ctx->intern->tsyms.t_unit,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Expr statement_break(struct Context * ctx) {
    struct Expr *expr = NULL;

    assert(Tokenstream_drop_text(&ctx->toks, "break"));
    if (ctx->toks.len && ctx->toks.buf[0].kind != TK_Semicolon) {
        expr = serene_alloc(ctx->alloc, struct Expr);
        assert(expr && "OOM");
        *expr = expr_any(ctx);
    }

    return (struct Expr){
        .tag = ST_Break,
        .type = ctx->intern->tsyms.t_unit,
        .break_stmt = expr,
    };
}

static struct Expr statement_return(struct Context * ctx) {
    struct Expr *expr = serene_alloc(ctx->alloc, struct Expr);
    assert(expr);

    assert(Tokenstream_drop_text(&ctx->toks, "return"));
    if (ctx->toks.len && ctx->toks.buf[0].kind != TK_Semicolon) {
        *expr = expr_any(ctx);
    } else {
        *expr = (struct Expr){0};
    };

    return (struct Expr){
        .tag = ST_Return,
        .type = ctx->intern->tsyms.t_unit,
        .return_stmt = expr,
    };
}

static struct Expr statement_assign(struct Context * ctx) {
    struct Expr *expr = serene_alloc(ctx->alloc, struct Expr);
    const char *name;
    assert(expr);

    assert(ctx->toks.len), name = ctx->toks.buf[0].spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));
    assert(Tokenstream_drop_text(&ctx->toks, "="));
    *expr = expr_any(ctx);

    return (struct Expr){
        .tag = ST_Assign,
        .type = ctx->intern->tsyms.t_unit,
        .assign.name = name,
        .assign.expr = expr,
    };
}

static const struct Type *Context_new_typevar(struct Context *ctx) {
    struct Type tmp = {
        .tag = TT_Var,
        .var = ctx->var_counter++,
    };
    return TypeIntern_intern(ctx->intern, &tmp);
}
