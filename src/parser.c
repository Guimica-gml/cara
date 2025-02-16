#include "./parser.h"
#include <stdio.h>

struct Context {
    struct serene_Trea* alloc;
    struct Opdecls ops;
    struct TypeIntern* intern;
    struct Tokenstream toks;
};

static struct Import decls_import(struct Context *);
static struct Function decls_function(struct Context *);

static struct Type const *type(struct Context *);
static struct Type const *type_func(struct Context *);
static struct Type const *type_op(struct Context *, unsigned);
static struct Type const *type_op_left(struct Context *);
static bool type_op_right_first(struct Context *, unsigned);
static struct Type const *
type_op_right(struct Context *, struct Type const *, unsigned);
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
static struct Expr expr_op_right(struct Context *, struct Expr left, unsigned prec);
static struct Expr expr_parenthesised(struct Context*);
static struct Expr expr_atom(struct Context*);

static struct Expr statement(struct Context *);
static struct Expr statement_let(struct Context *);
static struct Expr statement_mut(struct Context *);
static struct Expr statement_break(struct Context *);
static struct Expr statement_return(struct Context *);
static struct Expr statement_assign(struct Context *);

struct Ast parse(
    struct serene_Trea* alloc, struct Opdecls ops,
    struct TypeIntern *intern, struct Tokenstream toks
) {
    struct Context ctx = {
        .alloc = alloc,
        .ops = ops,
        .intern = intern,
        .toks = toks,
    };
    struct FunctionsLL* funcs = NULL;
    struct ImportsLL* imports = NULL;

    while (true) {
        switch (Tokenstream_peek(&ctx.toks).kind) {
        case TK_Func: {
            struct FunctionsLL *tmp = serene_trealloc(alloc, struct FunctionsLL);
            assert(tmp && "OOM");
            tmp->current = decls_function(&ctx);
            tmp->next = funcs;
            funcs = tmp;
            break;
        }
        case TK_Import: {
            struct ImportsLL* tmp = serene_trealloc(alloc, struct ImportsLL);
            assert(tmp && "OOM");
            tmp->current = decls_import(&ctx);
            tmp->next = imports;
            imports = tmp;
            break;
        }
        default:
            goto after;
        }
    }
after:
    printf("last tokens is: %s\n", Tokenstream_peek(&ctx.toks).spelling.str);
    assert(Tokenstream_peek(&ctx.toks).kind == TK_EOF);

    return (struct Ast){
        .funcs = funcs,
        .imports = imports,
    };
}

static struct Import decls_import(struct Context* ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Import));
    struct String name = Tokenstream_peek(&ctx->toks).spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Semicolon));
    return (struct Import) { .path = name };
}

static struct Function decls_function(struct Context *ctx) {
    struct String name;
    struct Binding args;
    struct Type const *ret;
    struct Expr body;

    assert(Tokenstream_drop_kind(&ctx->toks, TK_Func));

    name = Tokenstream_peek(&ctx->toks).spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));

    args = binding_parenthesised(ctx);
    if (Tokenstream_drop_kind(&ctx->toks, TK_Colon)) {
        ret = type(ctx);
    } else {
        ret = ctx->intern->tsyms.t_unit;
    }

    if (Tokenstream_drop_kind(&ctx->toks, TK_Equals)) {
        body = expr_delimited(ctx);
    } else {
        body = expr_bareblock(ctx);
        Tokenstream_drop_kind(&ctx->toks, TK_Semicolon);
    };

    return (struct Function){.name = name, .args = args, .ret = ret, .body = body};
}

static struct Type const *type(struct Context *ctx) {
    if (Tokenstream_peek(&ctx->toks).kind == TK_Func)
        return type_func(ctx);
    return type_op(ctx, 0);
}

static struct Type const *type_func(struct Context *ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Func));
    struct Type const *args = type_parenthesised(ctx);
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Colon));
    struct Type const *ret = type(ctx);
    return Type_func(ctx->intern, args, ret);
}

static struct Type const *type_op(struct Context *ctx, unsigned prec) {
    struct Type const *left;

    switch (Tokenstream_peek(&ctx->toks).kind) {
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

static struct Type const *type_op_left(struct Context *ctx) {
    struct Type const *args;
    struct Type const *name;

    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp >= 0 || ctx->ops.buf[i].rbp < 0)
            continue;
        if (Tokenstream_peek(&ctx->toks).spelling.str != ctx->ops.buf[i].token.str)
            continue;

        name = Type_recall(ctx->intern, Tokenstream_peek(&ctx->toks).spelling);

        assert(Tokenstream_drop(&ctx->toks));
        args = type_op(ctx, ctx->ops.buf[i].rbp);

        return Type_call(ctx->intern, name, args);
    }

    assert(false && "unexpected token");
}

static bool type_op_right_first(struct Context *ctx, unsigned prec) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
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
        if (Tokenstream_peek(&ctx->toks).spelling.str != ctx->ops.buf[i].token.str)
            continue;
        return true;
    }

    return false;
}

static struct Type const* type_op_right(
    struct Context* ctx,
    struct Type const* left,
    unsigned prec
) {
    struct Type const* name;
    struct Type const *args;
    switch (Tokenstream_peek(&ctx->toks).kind) {
    case TK_OpenParen:
    case TK_Name:
        name = left;
        args = type_atom(ctx);
        return Type_call(ctx->intern, name, args);
    default:
        for (size_t i = 0; i < ctx->ops.len; i++) {
            if (ctx->ops.buf[i].lbp < (int)prec)
                continue;
            if (Tokenstream_peek(&ctx->toks).spelling.str != ctx->ops.buf[i].token.str)
                continue;

            name = Type_recall(ctx->intern, Tokenstream_peek(&ctx->toks).spelling);
            assert(Tokenstream_drop(&ctx->toks));
            if (ctx->ops.buf[i].rbp >= 0) {
                const struct Type *right = type_op(ctx, ctx->ops.buf[i].rbp);
                args = Type_tuple(ctx->intern, left, right);
            } else {
                args = left;
            }

            return Type_call(ctx->intern, name, args);
        }
    }

    assert(false && "unexpected token");
}

static const struct Type *type_atom(struct Context *ctx) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
    case TK_OpenParen:
        return type_parenthesised(ctx);
    case TK_Name: {
        struct String name = Tokenstream_peek(&ctx->toks).spelling;
        Tokenstream_drop(&ctx->toks);

        return Type_recall(ctx->intern, name);
    }
    default:
        assert(false && "unexpected token encountered");
    }
}

static const struct Type *type_parenthesised(struct Context *ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_OpenParen));
    const struct Type* out = type(ctx);
    if (Tokenstream_peek(&ctx->toks).kind == TK_Semicolon) {
        assert(Tokenstream_drop(&ctx->toks));
        int n = Tokenstream_peek(&ctx->toks).number;
        assert(Tokenstream_drop_kind(&ctx->toks, TK_Number));
        const struct Type* type = out;
        for (int i = 1; i < n; i++)
            out = Type_tuple_extend(ctx->intern, out, type);
    }
    while (Tokenstream_peek(&ctx->toks).kind == TK_Comma) {
        assert(Tokenstream_drop(&ctx->toks));
        const struct Type* rhs = type(ctx);
        if (Tokenstream_peek(&ctx->toks).kind == TK_Semicolon) {
            assert(Tokenstream_drop(&ctx->toks));
            int n = Tokenstream_peek(&ctx->toks).number;
            assert(Tokenstream_drop_kind(&ctx->toks, TK_Number));
            for (int i = 0; i < n; i++)
                out = Type_tuple_extend(ctx->intern, out, rhs);
        } else {
            out = Type_tuple_extend(ctx->intern, out, rhs);
        }
    }
    assert(Tokenstream_drop_kind(&ctx->toks, TK_CloseParen));
    return out;
}

static struct Binding binding(struct Context *ctx) { return binding_atom(ctx); }

static struct Binding binding_atom(struct Context *ctx) {
    if (Tokenstream_peek(&ctx->toks).kind == TK_OpenParen) {
        return binding_parenthesised(ctx);
    }
    return binding_name(ctx);
}

static struct Binding binding_parenthesised(struct Context* ctx) {
    struct Binding out = {0};
    assert(Tokenstream_drop_kind(&ctx->toks, TK_OpenParen));
    if (Tokenstream_peek(&ctx->toks).kind != TK_CloseParen) {
        out = binding(ctx);
        if (Tokenstream_peek(&ctx->toks).kind == TK_Comma) {
            struct BindingTuple* node = serene_trealloc(ctx->alloc, struct BindingTuple);
            assert(node && "OOM");
            *node = (struct BindingTuple){0};
            node->current = out;
            out = (struct Binding){.tag = BT_Tuple, .tuple = node};

            struct BindingTuple* last = node;
            while (Tokenstream_peek(&ctx->toks).kind == TK_Comma) {
                assert(Tokenstream_drop(&ctx->toks));
                struct BindingTuple* tmp = serene_trealloc(ctx->alloc, struct BindingTuple);
                assert(tmp && "OOM");
                *tmp = (struct BindingTuple) {0};
                tmp->current = binding(ctx);
                last->next = tmp;
                last = tmp;
            }
        }
    } else {
        out.tag = BT_Empty;
        out.empty = Type_new_typevar(ctx->intern);
    }
    assert(Tokenstream_drop_kind(&ctx->toks, TK_CloseParen));
    return out;
}

static struct Binding binding_name(struct Context *ctx) {
    struct String name = Tokenstream_peek(&ctx->toks).spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));
    const struct Type *annot;
    if (Tokenstream_peek(&ctx->toks).kind == TK_Colon) {
        assert(Tokenstream_drop(&ctx->toks));
        annot = type(ctx);
    } else {
        annot = Type_new_typevar(ctx->intern);
    }

    return (struct Binding){
        .tag = BT_Name,
        .name.name = name,
        .name.annot = annot,
    };
}

static struct Expr expr_delimited(struct Context *ctx) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
    case TK_If:
    case TK_Loop:
    case TK_OpenBrace: {
        struct Expr tmp = expr_block(ctx);
        Tokenstream_drop_kind(&ctx->toks, TK_Semicolon);

        return tmp;
    }
    default: {
        struct Expr out = expr_inline(ctx);
        assert(Tokenstream_drop_kind(&ctx->toks, TK_Semicolon));
        return out;
    }
    }
}

static struct Expr expr_any(struct Context *ctx) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
    case TK_If:
    case TK_Loop:
    case TK_OpenBrace:
        return expr_block(ctx);
    default:
        return expr_inline(ctx);
    }
}

static struct Expr expr_block(struct Context *ctx) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
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

static struct Expr expr_if(struct Context* ctx) {
    struct Expr cond;
    struct Expr smash;
    struct Expr pass;

    assert(Tokenstream_drop_kind(&ctx->toks, TK_If));
    cond = expr_any(ctx);
    smash = expr_block(ctx);
    if (Tokenstream_drop_kind(&ctx->toks, TK_Else)) {
        pass = expr_block(ctx);
    } else {
        pass = Expr_unit(ctx->intern);
    }

    return Expr_if(ctx->alloc, cond, smash, pass);
}

static struct Expr expr_loop(struct Context* ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Loop));
    struct Expr block = expr_block(ctx);
    return Expr_loop(ctx->alloc, ctx->intern, block);
}

static struct Expr expr_bareblock(struct Context *ctx) {
    struct ExprsLL *block = NULL;
    struct ExprsLL *last = NULL;
    const struct Type *type = ctx->intern->tsyms.t_unit;

    assert(Tokenstream_drop_kind(&ctx->toks, TK_OpenBrace));
    while (Tokenstream_peek(&ctx->toks).kind != TK_CloseBrace) {
        {
            struct ExprsLL* tmp = serene_trealloc(ctx->alloc, struct ExprsLL);
            *tmp = (typeof(*tmp)){0};
            assert(tmp && "OOM");
            if (!last) block = tmp;
            else last->next = tmp;
            last = tmp;
        }
        last->current = statement(ctx);

        if (Tokenstream_drop_kind(&ctx->toks, TK_Semicolon)) {
            last->current = Expr_const(ctx->alloc, ctx->intern, last->current);
        } else {
            type = last->current.type;
            break;
        }
    }
    assert(Tokenstream_drop_kind(&ctx->toks, TK_CloseBrace));

    return (struct Expr){.tag = ET_Bareblock, .type = type, .bareblock = block};
}

static struct Expr expr_inline(struct Context* ctx) {
    return expr_op(ctx, 0);
}

static struct Expr expr_op(struct Context *ctx, unsigned prec) {
    struct Expr left;

    switch (Tokenstream_peek(&ctx->toks).kind) {
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

static struct Expr expr_op_left(struct Context *ctx) {
    struct ExprCall* call = serene_trealloc(ctx->alloc, struct ExprCall);
    assert(call && "OOM");

    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (ctx->ops.buf[i].lbp >= 0 || ctx->ops.buf[i].rbp < 0)
            continue;
        if (Tokenstream_peek(&ctx->toks).spelling.str != ctx->ops.buf[i].token.str)
            continue;

        assert(Tokenstream_drop(&ctx->toks));
        struct Expr name = Expr_recall(ctx->intern, ctx->ops.buf[i].token);
        struct Expr args = expr_op(ctx, ctx->ops.buf[i].rbp);
        return Expr_call(ctx->alloc, ctx->intern, name, args);
    }

    assert(false && "unexpected token");
}

static bool expr_op_right_first(struct Context* ctx, unsigned prec) {
    struct Token op = Tokenstream_peek(&ctx->toks);
    for (size_t i = 0; i < ctx->ops.len; i++) {
        if (op.spelling.str != ctx->ops.buf[i].token.str)
            continue;
        if (ctx->ops.buf[i].lbp < (int)prec)
            return false;
        return true;
    }

    switch (op.kind) {
    case TK_OpenParen:
    case TK_Name:
    case TK_Number:
    case TK_String:
    case TK_Bool:
        return true;
    default:
        break;
    }

    return false;
}

static struct Expr expr_op_right(
    struct Context* ctx,
    struct Expr left,
    unsigned prec
) {
    struct Token op = Tokenstream_peek(&ctx->toks);
    switch (op.kind) {
    case TK_Name:
        for (size_t i = 0; i < ctx->ops.len; i++) {
            if (op.spelling.str != ctx->ops.buf[i].token.str)
                continue;
            if (ctx->ops.buf[i].lbp < (int)prec)
                continue;

            assert(Tokenstream_drop(&ctx->toks));
            struct Expr name = Expr_recall(ctx->intern, ctx->ops.buf[i].token);
            struct Expr args;
            if (ctx->ops.buf[i].rbp >= 0) {
                args = Expr_tuple(
                    ctx->alloc,
                    ctx->intern,
                    left,
                    expr_op(ctx, ctx->ops.buf[i].rbp)
                );
            } else {
                args = left;
            }

            return Expr_call(ctx->alloc, ctx->intern, name, args);
        }
        __attribute__((fallthrough));
    case TK_OpenParen:
    case TK_Number:
    case TK_String:
    case TK_Bool: {
        struct Expr args = expr_atom(ctx);
        struct Expr name = left;
        return Expr_call(ctx->alloc, ctx->intern, name, args);
    }
    default: break;
    }

    assert(false && "unexpected token");
}

static struct Expr expr_parenthesised(struct Context* ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_OpenParen));
    struct Expr out;
    if (Tokenstream_peek(&ctx->toks).kind != TK_CloseParen) {
        out = expr_any(ctx);

        if (Tokenstream_peek(&ctx->toks).kind == TK_Semicolon) {
            assert(Tokenstream_drop(&ctx->toks));
            int n = Tokenstream_peek(&ctx->toks).number;
            assert(Tokenstream_drop_kind(&ctx->toks, TK_Number));
            struct Expr expr = out;
            for (int i = 1; i < n; i++) 
                out = Expr_tuple_extend(ctx->alloc, ctx->intern, out, expr);
        }

        while (Tokenstream_peek(&ctx->toks).kind == TK_Comma) {
            assert(Tokenstream_drop(&ctx->toks));
            struct Expr next = expr_any(ctx);
            if (Tokenstream_peek(&ctx->toks).kind == TK_Semicolon) {
                assert(Tokenstream_drop(&ctx->toks));
                int n = Tokenstream_peek(&ctx->toks).number;
                assert(Tokenstream_drop_kind(&ctx->toks, TK_Number));
                for (int i = 1; i < n; i++)
                    out = Expr_tuple_extend(ctx->alloc, ctx->intern, out, next);
            }
            out = Expr_tuple_extend(ctx->alloc, ctx->intern, out, next);
        }
    } else {
        out = Expr_unit(ctx->intern);
    }
    assert(Tokenstream_drop_kind(&ctx->toks, TK_CloseParen));
    return out;
}

static struct Expr expr_atom(struct Context *ctx) {
    struct Token peek = Tokenstream_peek(&ctx->toks);
    switch (peek.kind) {
        case TK_OpenParen:
            return expr_parenthesised(ctx);
        case TK_Name:
            assert(Tokenstream_drop(&ctx->toks));
            return Expr_recall(ctx->intern, peek.spelling);
        case TK_Number:
            assert(Tokenstream_drop(&ctx->toks));
            return Expr_number(ctx->intern, peek.spelling);
        case TK_String:
            assert(Tokenstream_drop(&ctx->toks));
            return Expr_string(ctx->intern, peek.spelling);
        case TK_Bool:
            assert(Tokenstream_drop(&ctx->toks));
            return Expr_bool(ctx->intern, peek.spelling);
        default:
            assert(false && "unexpected token");
    }
}

static struct Expr statement(struct Context *ctx) {
    switch (Tokenstream_peek(&ctx->toks).kind) {
        case TK_Let: return statement_let(ctx);
        case TK_Mut: return statement_mut(ctx);
        case TK_Break: return statement_break(ctx);
        case TK_Return: return statement_return(ctx);
        case TK_Name: {
            struct Tokenstream tmp = ctx->toks;
            assert(Tokenstream_drop(&tmp));
            if (Tokenstream_peek(&tmp).kind == TK_Equals) {
                return statement_assign(ctx);
            }
            __attribute__((fallthrough));
        }
        default: return expr_any(ctx);
    }
}

static struct Expr statement_let(struct Context *ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Let));
    struct Binding bind = binding(ctx);
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Equals));
    struct Expr init = expr_any(ctx);
    return Expr_let(ctx->alloc, ctx->intern, bind, init);
}

static struct Expr statement_mut(struct Context *ctx) {
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Mut));
    struct Binding bind = binding(ctx);
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Equals));
    struct Expr init = expr_any(ctx);
    return Expr_mut(ctx->alloc, ctx->intern, bind, init);
}

static struct Expr statement_break(struct Context *ctx) {
    struct Expr expr;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Break));
    if (Tokenstream_peek(&ctx->toks).kind != TK_Semicolon) {
        expr = expr_any(ctx);
    } else {
        expr = Expr_unit(ctx->intern);
    }
    return Expr_break(ctx->alloc, ctx->intern, expr);
}

static struct Expr statement_return(struct Context *ctx) {
    struct Expr expr;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Return));
    if (Tokenstream_peek(&ctx->toks).kind != TK_Semicolon) {
        expr = expr_any(ctx);
    } else {
        expr = Expr_unit(ctx->intern);
    };
    return Expr_return(ctx->alloc, ctx->intern, expr);
}

static struct Expr statement_assign(struct Context *ctx) {
    struct String name = Tokenstream_peek(&ctx->toks).spelling;
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Name));
    assert(Tokenstream_drop_kind(&ctx->toks, TK_Equals));
    struct Expr expr = expr_any(ctx);
    return Expr_assign(ctx->alloc, ctx->intern, name, expr);
}
