#include "./parser.h"
#include "lexer.h"

struct Context {
    struct Arena* arena;
    struct Opdecls* ops;
    struct Tokenstream* toks;
};

static struct Function decls_function(struct Context);

static struct Type type(struct Context);
static struct Type type_func(struct Context);
static struct Type type_op(struct Context, unsigned);
static struct Type type_op_left(struct Context);
static bool type_op_right_first(struct Context, unsigned);
static struct Type type_op_right(struct Context, struct Type, unsigned);
static struct Type type_atom(struct Context);
static struct Type type_parenthesised(struct Context);

static struct Binding binding(struct Context);
static struct Binding binding_atom(struct Context);
static struct Binding binding_parenthesised(struct Context);
static struct Binding binding_name(struct Context);

static struct Expr expr_delimited(struct Context);
static struct Expr expr_any(struct Context);
static struct Expr expr_block(struct Context);
static struct Expr expr_bareblock(struct Context);
static struct Expr expr_if(struct Context);
static struct Expr expr_loop(struct Context);
static struct Expr expr_inline(struct Context);
static struct Expr expr_op(struct Context, unsigned);
static struct Expr expr_op_left(struct Context);
static bool expr_op_right_first(struct Context, unsigned);
static struct Expr expr_op_right(struct Context, struct Expr, unsigned);
static struct Expr expr_atom(struct Context);

static struct Statement statement(struct Context);
static struct Statement statement_let(struct Context);
static struct Statement statement_mut(struct Context);
static struct Statement statement_break(struct Context);
static struct Statement statement_return(struct Context);
static struct Statement statement_assign(struct Context);

struct Ast parse(struct Arena* arena, struct Opdecls ops, struct Tokenstream toks) {
    struct Context ctx = {
        .arena = arena,
        .ops = &ops,
        .toks = &toks,
    };
    struct FunctionsLL *funcs = NULL;
    struct FunctionsLL *tmp = NULL;
    
    while (!Tokenstream_eof(ctx.toks)) {
        switch (*Tokenstream_first_kind(ctx.toks)) {
        case TK_Func:
            tmp = arena_alloc(arena, sizeof(struct FunctionsLL));
            assert(tmp && "OOM");
            tmp->current = decls_function(ctx);
            tmp->next = funcs;
            funcs = tmp;
            break;
        default: goto after;
        }
    }
  after:
    assert(Tokenstream_eof(ctx.toks));

    return (struct Ast) {
        .funcs = funcs,
    };
}

static struct Function decls_function(struct Context ctx) {
    static struct Stringview unit_type = {
        .str = "()",
        .len = 2,
    };
    struct Stringview* name;
    struct Binding args;
    struct Type ret;
    struct Expr body;

    assert(Tokenstream_drop_text(ctx.toks, "func"));

    name = Tokenstream_first_text(ctx.toks);
    assert(name && Tokenstream_drop_kind(ctx.toks, TK_Name));

    args = binding_parenthesised(ctx);
    if (Tokenstream_drop_text(ctx.toks, ":")) {
        ret = type(ctx);
    } else {
        ret = (struct Type) {
            .tag = TT_Recall,
            .recall.name = unit_type
        };
    }

    if (Tokenstream_drop_text(ctx.toks, "=")) {
        body = expr_delimited(ctx);
    } else {
        body = expr_bareblock(ctx);
        Tokenstream_drop_text(ctx.toks, ";");
    };

    return (struct Function) {
        .name = *name,
        .args = args,
        .ret = ret,
        .body = body
    };
}

static struct Type type(struct Context ctx) {
    assert(!Tokenstream_eof(ctx.toks));
    if (*Tokenstream_first_kind(ctx.toks) == TK_Func) {
        return type_func(ctx);
    }
    return type_op(ctx, 0);
}

static struct Type type_func(struct Context ctx) {
    struct Type* args = arena_alloc(ctx.arena, sizeof(struct Type));
    struct Type* ret = arena_alloc(ctx.arena, sizeof(struct Type));
    assert(args && ret);
    assert(Tokenstream_drop_text(ctx.toks, "func"));
    *args = type_parenthesised(ctx);
    assert(Tokenstream_drop_text(ctx.toks, ":"));
    *ret = type(ctx);
    return (struct Type) {
        .tag = TT_Func,
        .func.args = args,
        .func.ret = ret,
    };
}

static struct Type type_op(struct Context ctx, unsigned prec) {
    struct Type left;

    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_Name: case TK_OpenParen:
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

static struct Type type_op_left(struct Context ctx) {
    struct Type *args = arena_alloc(ctx.arena, sizeof(struct Type));
    struct Type *name = arena_alloc(ctx.arena, sizeof(struct Type));
    assert(args && name);
    
    assert(!Tokenstream_eof(ctx.toks));
    for (size_t i = 0; i < ctx.ops->len; i++) {
        if (ctx.ops->buf[i].lbp >= 0 || ctx.ops->buf[i].rbp < 0) continue;
        if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;
        assert(Tokenstream_drop(ctx.toks));
        *name = (struct Type) {
            .tag = TT_Recall,
            .recall.name = ctx.ops->buf[i].token,
        };
        *args = type_op(ctx, ctx.ops->buf[i].rbp);
        return (struct Type) {
            .tag = TT_Call,
            .call.name = name,
            .call.args = args,
        };
    }

    assert(false && "unexpected token");
}

static bool type_op_right_first(struct Context ctx, unsigned prec) {
    if (Tokenstream_eof(ctx.toks)) return false;
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen: case TK_Name: return true;
    default: break;
    }

    for (size_t i = 0; i < ctx.ops->len; i++) {
        if (ctx.ops->buf[i].lbp < (int) prec) continue;
        if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;
        return true;
    }
    
    return false;
}

// NOTE: the comma operator would output parse tree of
//       (a,b) => Type::Recall(",", Type::Comma(a, b))
static struct Type type_op_right(
    struct Context ctx,
    struct Type left,
    unsigned prec
) {
    struct Type* name = arena_alloc(ctx.arena, sizeof(struct Type));
    struct Type* args = arena_alloc(ctx.arena, sizeof(struct Type));
    assert(name && args);

    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen: case TK_Name:
        *name = type_atom(ctx);
        *args = left;
        
        return (struct Type) {
            .tag = TT_Call,
            .call.name = name,
            .call.args = args,
        };
    default:
        for (size_t i = 0; i < ctx.ops->len; i++) {
            if (ctx.ops->buf[i].lbp < (int) prec) continue;
            if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;
            assert(Tokenstream_drop(ctx.toks));
            *name = (struct Type) {
                .tag = TT_Recall,
                .recall.name = ctx.ops->buf[i].token,
            };
            if (ctx.ops->buf[i].rbp >= 0) {
                struct Type *left_ptr = arena_alloc(ctx.arena, sizeof(struct Type));
                struct Type *right_ptr = arena_alloc(ctx.arena, sizeof(struct Type));
                assert(left_ptr && right_ptr);

                *left_ptr = left;
                *right_ptr = type_op(ctx, ctx.ops->buf[i].rbp);
                *args = (struct Type) {
                    .tag = TT_Comma,
                    .comma.lhs = left_ptr,
                    .comma.rhs = right_ptr,
                };
            } else {
                *args = left;
            }

            return (struct Type) {
                .tag = TT_Call,
                .call.name = name,
                .call.args = args,
            };
        }
    }

    assert(false && "unexpected token");
}

static struct Type type_atom(struct Context ctx) {
    struct Stringview name;
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen:
        return type_parenthesised(ctx);
    case TK_Name:
        name = *Tokenstream_first_text(ctx.toks);
        Tokenstream_drop(ctx.toks);
        return (struct Type) {
            .tag = TT_Recall,
            .recall.name = name,
        };
    default: assert(false && "unexpected token encountered");
    }
}

static struct Type type_parenthesised(struct Context ctx) {
    assert(Tokenstream_drop_text(ctx.toks, "("));
    struct Type out = type(ctx);
    assert(Tokenstream_drop_text(ctx.toks, ")"));
    return out;
}

static struct Binding binding(struct Context ctx) {
    return binding_atom(ctx);
}

static struct Binding binding_atom(struct Context ctx) {
    assert(!Tokenstream_eof(ctx.toks));
    if (*Tokenstream_first_kind(ctx.toks) == TK_OpenParen) {
        return binding_parenthesised(ctx);
    }
    return binding_name(ctx);
}

static struct Binding binding_parenthesised(struct Context ctx) {
    struct Binding out = {0};
    assert(Tokenstream_drop_text(ctx.toks, "("));
    assert(!Tokenstream_eof(ctx.toks));
    if (*Tokenstream_first_kind(ctx.toks) != TK_CloseParen) {
        out = binding(ctx);
    } else {
        out.tag = BT_Empty;
    }
    assert(Tokenstream_drop_text(ctx.toks, ")"));
    return out;
}

static struct Binding binding_name(struct Context ctx) {
    struct Type* annot = NULL;
    struct Stringview* name;
    
    name = Tokenstream_first_text(ctx.toks);
    assert(Tokenstream_drop_kind(ctx.toks, TK_Name));
    if (!Tokenstream_eof(ctx.toks) && *Tokenstream_first_kind(ctx.toks) == TK_Colon) {
        assert(Tokenstream_drop(ctx.toks));
        annot = arena_alloc(ctx.arena, sizeof(struct Type));
        assert(annot);
        *annot = type(ctx);
    }

    return (struct Binding) {
        .tag = BT_Name,
        .name.name = *name,
        .name.annot = annot,
    };
}

static struct Expr expr_delimited(struct Context ctx) {
    struct Expr tmp;
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_If: case TK_Loop: case TK_OpenBrace:
        tmp = expr_block(ctx);
        Tokenstream_drop_text(ctx.toks, ";");
        return tmp;
    default:
        return expr_inline(ctx);
    }
}

static struct Expr expr_any(struct Context ctx) {
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_If: case TK_Loop: case TK_OpenBrace:
        return expr_block(ctx);
    default:
        return expr_inline(ctx);
    }
}

static struct Expr expr_block(struct Context ctx) {
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_If: return expr_if(ctx);
    case TK_Loop: return expr_loop(ctx);
    case TK_OpenBrace: return expr_bareblock(ctx);
    default: assert(false);
    }
}

static struct Expr expr_if(struct Context ctx) {
    struct Expr* cond = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Expr* smash = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Expr* pass = arena_alloc(ctx.arena, sizeof(struct Expr));
    assert(cond && smash && pass && "OOM");
    
    assert(Tokenstream_drop_text(ctx.toks, "if"));
    *cond = expr_any(ctx);
    *smash = expr_block(ctx);
    if (Tokenstream_drop_text(ctx.toks, "else")) {
        *pass = expr_block(ctx);
    } else {
        *pass = (struct Expr) {0};
    }
    
    return (struct Expr) {
        .tag = ET_If,
        .if_expr.cond = cond,
        .if_expr.smash = smash,
        .if_expr.pass = pass,
    };
}

static struct Expr expr_loop(struct Context ctx) {
    struct Expr* block = arena_alloc(ctx.arena, sizeof(struct Expr));
    assert(block && "OOM");

    assert(Tokenstream_drop_text(ctx.toks, "loop"));
    *block = expr_block(ctx);

    return (struct Expr) {
        .tag = ET_Loop,
        .loop.block = block,
    };
}

static struct Expr expr_bareblock(struct Context ctx) {
    struct Expr* tail = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct StatementsLL* stmts = NULL;
    assert(tail && "OOM");
    
    assert(Tokenstream_drop_text(ctx.toks, "{"));
    while (!Tokenstream_eof(ctx.toks)
           && *Tokenstream_first_kind(ctx.toks) != TK_CloseBrace) {
        struct Statement s = statement(ctx);
        
        if (!Tokenstream_eof(ctx.toks)
            && *Tokenstream_first_kind(ctx.toks) != TK_Semicolon) {
            assert(s.tag == ST_Const);
            tail = s.const_stmt.expr;
            break;
        }

        assert(Tokenstream_drop_text(ctx.toks, ";"));
        struct StatementsLL* tmp = arena_alloc(ctx.arena, sizeof(struct StatementsLL));
        assert(tmp && "OOM");
        tmp->current = s;
        tmp->next = stmts;
        stmts = tmp;
    }
    assert(Tokenstream_drop_text(ctx.toks, "}"));

    return (struct Expr) {
        .tag = ET_Bareblock,
        .bareblock.stmts = stmts,
        .bareblock.tail = tail
    };
}

static struct Expr expr_inline(struct Context ctx) {
    return expr_op(ctx, 0);
}

static struct Expr expr_op(
    struct Context ctx,
    unsigned prec
) {
    struct Expr left;

    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_Name: case TK_OpenParen: case TK_Number: case TK_String: case TK_Bool:
        left = expr_atom(ctx);
        break;
    default:
        left = expr_op_left(ctx);
    }
    while (expr_op_right_first(ctx, prec)) {
        left = expr_op_right(ctx, left, prec);
    }
    
    return left;
}

static struct Expr expr_op_left(struct Context ctx) {
    struct Expr *args = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Expr *name = arena_alloc(ctx.arena, sizeof(struct Expr));
    assert(args && name);

    assert(!Tokenstream_eof(ctx.toks));
    for (size_t i = 0; i < ctx.ops->len; i++) {
        if (ctx.ops->buf[i].lbp >= 0 || ctx.ops->buf[i].rbp < 0) continue;
        if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;

        assert(Tokenstream_drop(ctx.toks));
        *name = (struct Expr) {
            .tag = ET_Recall,
            .lit.name = ctx.ops->buf[i].token,
        };
        *args = expr_op(ctx, ctx.ops->buf[i].rbp);
        return (struct Expr) {
            .tag = ET_Call,
            .call.name = name,
            .call.args = args,
        };
    }
    
    assert(false && "unexpected token");
}

static bool expr_op_right_first(struct Context ctx, unsigned prec) {
    if (Tokenstream_eof(ctx.toks)) return false;
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen: case TK_Name: case TK_Number: case TK_String: case TK_Bool:
        return true;
    default: break;
    }

    for (size_t i = 0; i < ctx.ops->len; i++) {
        if (ctx.ops->buf[i].lbp < (int) prec) continue;
        if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;
        return true;
    }
    return false;
}

// NOTE: the comma operator would output parse tree of
//       (a,b) => Type::Recall(",", Type::Comma(a, b))
static struct Expr expr_op_right(
    struct Context ctx,
    struct Expr left,
    unsigned prec
) {
    struct Expr* name = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Expr* args = arena_alloc(ctx.arena, sizeof(struct Expr));
    assert(name && args);

    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen: case TK_Name: case TK_Number: case TK_String: case TK_Bool:
        *name = expr_atom(ctx);
        *args = left;

        return (struct Expr) {
            .tag = ET_Call,
            .call.name = name,
            .call.args = args,
        };
    default:
        for (size_t i = 0; i < ctx.ops->len; i++) {
            if (ctx.ops->buf[i].lbp < (int) prec) continue;
            if (!Stringview_equal(*Tokenstream_first_text(ctx.toks), ctx.ops->buf[i].token)) continue;

            assert(Tokenstream_drop(ctx.toks));
            *name = (struct Expr) {
                .tag = ET_Recall,
                .lit.name = ctx.ops->buf[i].token,
            };
            if (ctx.ops->buf[i].rbp >= 0) {
                struct Expr* left_ptr = arena_alloc(ctx.arena, sizeof(struct Expr));
                struct Expr* right_ptr = arena_alloc(ctx.arena, sizeof(struct Expr));
                assert(left_ptr && right_ptr);

                *left_ptr = left;
                *right_ptr = expr_op(ctx, ctx.ops->buf[i].rbp);
                *args = (struct Expr) {
                    .tag = ET_Comma,
                    .comma.lhs = left_ptr,
                    .comma.rhs = right_ptr,
                };
            } else {
                *args = left;
            }

            return (struct Expr) {
                .tag = ET_Call,
                .call.name = name,
                .call.args = args,
            };
        }
    }
    
    assert(false && "unexpected token");
}

static struct Expr expr_atom(struct Context ctx) {
    enum ExprTag tag;
    
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_OpenParen:
        assert(Tokenstream_drop(ctx.toks));
        struct Expr tmp = expr_any(ctx);
        assert(Tokenstream_drop_text(ctx.toks, ")"));
        return tmp;
    case TK_Name: tag = ET_Recall; break;
    case TK_Number: tag = ET_NumberLit; break;
    case TK_String: tag = ET_StringLit; break;
    case TK_Bool: tag = ET_BoolLit; break;
    default: assert(false && "unexpected token");
    }
    
    struct Stringview name = *Tokenstream_first_text(ctx.toks);
    assert(Tokenstream_drop(ctx.toks));
    return (struct Expr) {
        .tag = tag,
        .lit.name = name,
    };
}

static struct Statement statement(struct Context ctx) {
    enum Tokenkind *next_peek;
    struct Expr* eptr;
    assert(!Tokenstream_eof(ctx.toks));
    switch (*Tokenstream_first_kind(ctx.toks)) {
    case TK_Let: return statement_let(ctx);
    case TK_Mut: return statement_mut(ctx);
    case TK_Break: return statement_break(ctx);
    case TK_Return: return statement_return(ctx);
    case TK_Name:
        next_peek = Tokenstream_at_kind(ctx.toks, 1);
        if (next_peek && *next_peek == TK_Equals) {
            return statement_assign(ctx);
        }
        __attribute__((fallthrough));
    default:
        eptr = arena_alloc(ctx.arena, sizeof(struct Expr));
        assert(eptr && "OOM");
        *eptr = expr_any(ctx);
        return (struct Statement) {
            .tag = ST_Const,
            .const_stmt.expr = eptr,
        };
    }
}

static struct Statement statement_let(struct Context ctx) {
    struct Expr* init = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Binding bind;
    assert(init && "OOM");
    
    assert(Tokenstream_drop_text(ctx.toks, "let"));
    bind = binding(ctx);
    assert(Tokenstream_drop_text(ctx.toks, "="));
    *init = expr_any(ctx);

    return (struct Statement) {
        .tag = ST_Let,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Statement statement_mut(struct Context ctx) {
    struct Expr* init = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Binding bind;
    assert(init && "OOM");
    
    assert(Tokenstream_drop_text(ctx.toks, "mut"));
    bind = binding(ctx);
    assert(Tokenstream_drop_text(ctx.toks, "="));
    *init = expr_any(ctx);

    return (struct Statement) {
        .tag = ST_Mut,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Statement statement_break(struct Context ctx) {
    struct Expr* expr = NULL;

    assert(Tokenstream_drop_text(ctx.toks, "break"));
    if (!Tokenstream_eof(ctx.toks) && *Tokenstream_first_kind(ctx.toks) != TK_Semicolon) {
        expr = arena_alloc(ctx.arena, sizeof(struct Expr));
        assert(expr && "OOM");
        *expr = expr_any(ctx);
    }

    return (struct Statement) {
        .tag = ST_Break,
        .break_stmt.expr = expr,
    };
}

static struct Statement statement_return(struct Context ctx) {
    struct Expr* expr = NULL;

    assert(Tokenstream_drop_text(ctx.toks, "return"));
    if (!Tokenstream_eof(ctx.toks) && *Tokenstream_first_kind(ctx.toks) != TK_Semicolon) {
        expr = arena_alloc(ctx.arena, sizeof(struct Expr));
        assert(expr && "OOM");
        *expr = expr_any(ctx);
    }

    return (struct Statement) {
        .tag = ST_Return,
        .return_stmt.expr = expr,
    };
}

static struct Statement statement_assign(struct Context ctx) {
    struct Expr* expr = arena_alloc(ctx.arena, sizeof(struct Expr));
    struct Stringview* name;
    assert(expr);

    name = Tokenstream_first_text(ctx.toks);
    assert(name);
    assert(Tokenstream_drop_kind(ctx.toks, TK_Name));
    assert(Tokenstream_drop_text(ctx.toks, "="));
    *expr = expr_any(ctx);

    return (struct Statement) {
        .tag = ST_Assign,
        .assign.name = *name,
        .assign.expr = expr,
    };
}
