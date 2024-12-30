#include "./parser.h"

static struct Function decls_function(struct Arena*, struct Tokenstream*);

static struct Type type(struct Arena*, struct Tokenstream*);
static struct Type type_func(struct Arena*, struct Tokenstream*);
static struct Type type_op(struct Arena*, struct Tokenstream*, unsigned);
static struct Type type_op_left(struct Arena*, struct Tokenstream*);
static struct Type type_op_right(struct Arena*, struct Tokenstream*, struct Type, unsigned);
static struct Type type_atom(struct Arena*, struct Tokenstream*);
static struct Type type_parenthesised(struct Arena*, struct Tokenstream*);

static struct Binding binding(struct Arena*, struct Tokenstream*);
static struct Binding binding_atom(struct Arena*, struct Tokenstream*);
static struct Binding binding_parenthesised(struct Arena*, struct Tokenstream*);
static struct Binding binding_name(struct Arena*, struct Tokenstream*);

static struct Expr expr_delimited(struct Arena*, struct Tokenstream*);
static struct Expr expr_any(struct Arena*, struct Tokenstream*);
static struct Expr expr_block(struct Arena*, struct Tokenstream*);
static struct Expr expr_bareblock(struct Arena*, struct Tokenstream*);
static struct Expr expr_if(struct Arena*, struct Tokenstream*);
static struct Expr expr_loop(struct Arena*, struct Tokenstream*);
static struct Expr expr_inline(struct Arena*, struct Tokenstream*);
static struct Expr expr_op(struct Arena*, struct Tokenstream*, unsigned);
static struct Expr expr_op_left(struct Arena*, struct Tokenstream*);
static struct Expr expr_op_right(struct Arena*, struct Tokenstream*, struct Expr, unsigned);
static struct Expr expr_atom(struct Arena*, struct Tokenstream*);

static struct Statement statement(struct Arena*, struct Tokenstream*);
static struct Statement statement_let(struct Arena*, struct Tokenstream*);
static struct Statement statement_mut(struct Arena*, struct Tokenstream*);
static struct Statement statement_break(struct Arena*, struct Tokenstream*);
static struct Statement statement_return(struct Arena*, struct Tokenstream*);
static struct Statement statement_assign(struct Arena*, struct Tokenstream*);

struct Ast parse(struct Arena* arena, struct Tokenstream toks) {
    struct FunctionsLL* funcs = NULL;
    
    while (!Tokenstream_eof(&toks)) {
        switch (*Tokenstream_first_kind(&toks)) {
        case TK_Func:
            struct FunctionsLL* tmp = arena_alloc(arena, sizeof(struct FunctionsLL));
            assert(tmp && "OOM");
            tmp->current = decls_function(arena, &toks);
            tmp->next = funcs;
            funcs = tmp;
            break;
        default: goto after;
        }
    }
  after:
    assert(Tokenstream_eof(&toks));

    return (struct Ast) {
        .funcs = funcs,
    };
}

const struct Stringview unit_type = {
    .str = "()",
    .len = 2,
};
static struct Function decls_function(struct Arena* arena, struct Tokenstream* toks) {
    struct Stringview* name;
    struct Binding args;
    struct Type ret;
    struct Expr body;

    assert(Tokenstream_drop_text(toks, "func"));

    name = Tokenstream_first_text(toks);
    assert(name && Tokenstream_drop_kind(toks, TK_Name));

    args = binding_parenthesised(arena, toks);
    if (Tokenstream_drop_text(toks, ":")) {
        ret = type(arena, toks);
    } else {
        ret = (struct Type) {
            .tag = TT_Recall,
            .recall.name = &unit_type
        };
    }

    if (Tokenstream_drop_text(toks, "=")) {
        body = expr_delimited(arena, toks);
    } else {
        body = expr_bareblock(arena, toks);
        Tokenstream_drop_text(toks, ";");
    };

    return (struct Function) {
        .name = *name,
        .args = args,
        .ret = ret,
        .body = body
    };
}

static struct Type type(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    if (*Tokenstream_first_kind(toks) == TK_Func) {
        return type_func(arena, toks);
    }
    return type_op(arena, toks, 0);
}

static struct Type type_func(struct Arena* arena, struct Tokenstream* toks) {
    struct Type* args = arena_alloc(arena, sizeof(struct Type));
    struct Type* ret = arena_alloc(arena, sizeof(struct Type));
    assert(args && ret);
    assert(Tokenstream_drop_text(toks, "func"));
    *args = type_parenthesised(arena, toks);
    assert(Tokenstream_drop_text(toks, ":"));
    *ret = type(arena, toks);
    return (struct Type) {
        .tag = TT_Func,
        .func.args = args,
        .func.ret = ret,
    };
}

static struct Type type_op(
    struct Arena* arena,
    struct Tokenstream* toks,
    unsigned prec
) {
    struct Type left;

    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_Name: case TK_OpenParen:
        left = type_atom(arena, toks);
        break;
    default:
        left = type_op_left(arena, toks);
    }

    while (!Tokenstream_eof(toks)) {
        enum Tokenkind head = *Tokenstream_first_kind(toks);
        // TODO: 'any o in right(lb): o & ...'
        // as we currently don't collect operator decls
        switch (head) {
        case TK_Name: case TK_OpenParen:
            left = type_op_right(arena, toks, left, prec);
            break;
        default:
            goto after;
        }
    }

  after:
    return left;
}

static struct Type type_op_left(struct Arena* arena, struct Tokenstream* toks) {
    (void)arena;
    (void)toks;
    assert(false && "TODO");
}

static struct Type type_op_right(
    struct Arena* arena,
    struct Tokenstream* toks,
    struct Type left,
    unsigned prec
) {
    (void)prec;
    // TODO: any o in right(lb)...
    struct Type* name = arena_alloc(arena, sizeof(struct Type));
    struct Type* args = arena_alloc(arena, sizeof(struct Type));
    assert(name && args);
    *name = type_atom(arena, toks);
    *args = left;
    return (struct Type) {
        .tag = TT_Call,
        .call.name = name,
        .call.args = args,
    };
}

static struct Type type_atom(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_OpenParen:
        return type_parenthesised(arena, toks);
    case TK_Name:
        struct Stringview* name = Tokenstream_first_text(toks);
        Tokenstream_drop(toks);
        return (struct Type) {
            .tag = TT_Recall,
            .recall.name = name,
        };
    default: assert(false && "unexpected token encountered");
    }
}

static struct Type type_parenthesised(struct Arena* arena, struct Tokenstream* toks) {
    assert(Tokenstream_drop_text(toks, "("));
    struct Type out = type(arena, toks);
    assert(Tokenstream_drop_text(toks, ")"));
    return out;
}

static struct Binding binding(struct Arena* arena, struct Tokenstream* toks) {
    return binding_atom(arena, toks);
}

static struct Binding binding_atom(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    if (*Tokenstream_first_kind(toks) == TK_OpenParen) {
        return binding_parenthesised(arena, toks);
    }
    return binding_name(arena, toks);
}

static struct Binding binding_parenthesised(struct Arena* arena, struct Tokenstream* toks) {
    struct Binding out = {0};
    assert(Tokenstream_drop_text(toks, "("));
    assert(!Tokenstream_eof(toks));
    if (*Tokenstream_first_kind(toks) != TK_CloseParen) {
        out = binding(arena, toks);
    } else {
        out.tag = BT_Empty;
    }
    assert(Tokenstream_drop_text(toks, ")"));
    return out;
}

static struct Binding binding_name(struct Arena* arena, struct Tokenstream* toks) {
    struct Type* annot = NULL;
    struct Stringview* name;
    
    name = Tokenstream_first_text(toks);
    assert(Tokenstream_drop_kind(toks, TK_Name));
    if (!Tokenstream_eof(toks) && *Tokenstream_first_kind(toks) == TK_Colon) {
        assert(Tokenstream_drop(toks));
        annot = arena_alloc(arena, sizeof(struct Type));
        assert(annot);
        *annot = type(arena, toks);
    }

    return (struct Binding) {
        .tag = BT_Name,
        .name.name = *name,
        .name.annot = annot,
    };
}

static struct Expr expr_delimited(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_If: case TK_Loop: case TK_OpenBrace:
        struct Expr tmp = expr_block(arena, toks);
        Tokenstream_drop_text(toks, ";");
        return tmp;
    default:
        return expr_inline(arena, toks);
    }
}

static struct Expr expr_any(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_If: case TK_Loop: case TK_OpenBrace:
        return expr_block(arena, toks);
    default:
        return expr_inline(arena, toks);
    }
}

static struct Expr expr_block(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_If: return expr_if(arena, toks);
    case TK_Loop: return expr_loop(arena, toks);
    case TK_OpenBrace: return expr_bareblock(arena, toks);
    default: assert(false);
    }
}

static struct Expr expr_if(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* cond = arena_alloc(arena, sizeof(struct Expr));
    struct Expr* smash = arena_alloc(arena, sizeof(struct Expr));
    struct Expr* pass = arena_alloc(arena, sizeof(struct Expr));
    assert(cond && smash && pass && "OOM");
    
    assert(Tokenstream_drop_text(toks, "if"));
    *cond = expr_any(arena, toks);
    *smash = expr_block(arena, toks);
    if (Tokenstream_drop_text(toks, "else")) {
        *pass = expr_block(arena, toks);
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

static struct Expr expr_loop(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* block = arena_alloc(arena, sizeof(struct Expr));
    assert(block && "OOM");

    assert(Tokenstream_drop_text(toks, "loop"));
    *block = expr_block(arena, toks);

    return (struct Expr) {
        .tag = ET_Loop,
        .loop.block = block,
    };
}

static struct Expr expr_bareblock(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* tail = arena_alloc(arena, sizeof(struct Expr));
    struct StatementsLL* stmts = NULL;
    assert(tail && "OOM");
    
    assert(Tokenstream_drop_text(toks, "{"));
    while (!Tokenstream_eof(toks)
           && *Tokenstream_first_kind(toks) != TK_CloseBrace) {
        struct Statement s = statement(arena, toks);
        
        if (!Tokenstream_eof(toks)
            && *Tokenstream_first_kind(toks) != TK_Semicolon) {
            assert(s.tag == ST_Const);
            tail = s.const_stmt.expr;
            break;
        }

        assert(Tokenstream_drop_text(toks, ";"));
        struct StatementsLL* tmp = arena_alloc(arena, sizeof(struct StatementsLL));
        assert(tmp && "OOM");
        tmp->current = s;
        tmp->next = stmts;
        stmts = tmp;
    }
    assert(Tokenstream_drop_text(toks, "}"));

    return (struct Expr) {
        .tag = ET_Bareblock,
        .bareblock.stmts = stmts,
        .bareblock.tail = tail
    };
}

static struct Expr expr_inline(struct Arena* arena, struct Tokenstream* toks) {
    return expr_op(arena, toks, 0);
}

static struct Expr expr_op(
    struct Arena* arena,
    struct Tokenstream* toks,
    unsigned prec
) {
    struct Expr left;

    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_Name: case TK_OpenParen: case TK_Number: case TK_String: case TK_Bool:
        left = expr_atom(arena, toks);
        break;
    default:
        left = expr_op_left(arena, toks);
    }

    while (!Tokenstream_eof(toks)) {
        enum Tokenkind head = *Tokenstream_first_kind(toks);
        // TODO: 'any o in right(lb): o & ...'
        // as we currently don't collect operator decls
        switch (head) {
        case TK_Name: case TK_OpenParen: case TK_Number: case TK_String: case TK_Bool:
            left = expr_op_right(arena, toks, left, prec);
            break;
        default:
            goto after;
        }
    }

  after:
    return left;
}

static struct Expr expr_op_left(struct Arena* arena, struct Tokenstream* toks) {
    (void)arena;
    (void)toks;
    assert(false && "TODO");
}

static struct Expr expr_op_right(
    struct Arena* arena,
    struct Tokenstream* toks,
    struct Expr left,
    unsigned prec
) {
    (void)prec;
    // TODO: any o in right(lb)...
    struct Expr* name = arena_alloc(arena, sizeof(struct Expr));
    struct Expr* args = arena_alloc(arena, sizeof(struct Expr));
    assert(name && args);
    *name = expr_atom(arena, toks);
    *args = left;
    return (struct Expr) {
        .tag = ET_Call,
        .call.name = name,
        .call.args = args,
    };
}

static struct Expr expr_atom(struct Arena* arena, struct Tokenstream* toks) {
    enum ExprTag tag;
    
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_OpenParen:
        assert(Tokenstream_drop(toks));
        struct Expr tmp = expr_any(arena, toks);
        assert(Tokenstream_drop_text(toks, ")"));
        return tmp;
    case TK_Name: tag = ET_Recall; break;
    case TK_Number: tag = ET_NumberLit; break;
    case TK_String: tag = ET_StringLit; break;
    case TK_Bool: tag = ET_BoolLit; break;
    default: assert(false && "unexpected token");
    }
    
    struct Stringview name = *Tokenstream_first_text(toks);
    assert(Tokenstream_drop(toks));
    return (struct Expr) {
        .tag = tag,
        .string.name = name,
    };
}

static struct Statement statement(struct Arena* arena, struct Tokenstream* toks) {
    assert(!Tokenstream_eof(toks));
    switch (*Tokenstream_first_kind(toks)) {
    case TK_Let: return statement_let(arena, toks);
    case TK_Mut: return statement_mut(arena, toks);
    case TK_Break: return statement_break(arena, toks);
    case TK_Return: return statement_return(arena, toks);
    case TK_Name:
        enum Tokenkind* next_peek = Tokenstream_at_kind(toks, 1);
        if (next_peek && *next_peek == TK_Equals) {
            return statement_assign(arena, toks);
        }
        __attribute__((fallthrough));
    default:
        struct Expr* eptr = arena_alloc(arena, sizeof(struct Expr));
        assert(eptr && "OOM");
        *eptr = expr_any(arena, toks);
        return (struct Statement) {
            .tag = ST_Const,
            .const_stmt.expr = eptr,
        };
    }
}

static struct Statement statement_let(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* init = arena_alloc(arena, sizeof(struct Expr));
    struct Binding bind;
    assert(init && "OOM");
    
    assert(Tokenstream_drop_text(toks, "let"));
    bind = binding(arena, toks);
    assert(Tokenstream_drop_text(toks, "="));
    *init = expr_any(arena, toks);

    return (struct Statement) {
        .tag = ST_Let,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Statement statement_mut(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* init = arena_alloc(arena, sizeof(struct Expr));
    struct Binding bind;
    assert(init && "OOM");
    
    assert(Tokenstream_drop_text(toks, "mut"));
    bind = binding(arena, toks);
    assert(Tokenstream_drop_text(toks, "="));
    *init = expr_any(arena, toks);

    return (struct Statement) {
        .tag = ST_Mut,
        .let.bind = bind,
        .let.init = init,
    };
}

static struct Statement statement_break(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* expr = NULL;

    assert(Tokenstream_drop_text(toks, "break"));
    if (!Tokenstream_eof(toks) && *Tokenstream_first_kind(toks) != TK_Semicolon) {
        expr = arena_alloc(arena, sizeof(struct Expr));
        assert(expr && "OOM");
        *expr = expr_any(arena, toks);
    }

    return (struct Statement) {
        .tag = ST_Break,
        .break_stmt.expr = expr,
    };
}

static struct Statement statement_return(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* expr = NULL;

    assert(Tokenstream_drop_text(toks, "return"));
    if (!Tokenstream_eof(toks) && *Tokenstream_first_kind(toks) != TK_Semicolon) {
        expr = arena_alloc(arena, sizeof(struct Expr));
        assert(expr && "OOM");
        *expr = expr_any(arena, toks);
    }

    return (struct Statement) {
        .tag = ST_Return,
        .return_stmt.expr = expr,
    };
}

static struct Statement statement_assign(struct Arena* arena, struct Tokenstream* toks) {
    struct Expr* expr = arena_alloc(arena, sizeof(struct Expr));
    struct Stringview* name;
    assert(expr);

    name = Tokenstream_first_text(toks);
    assert(name);
    assert(Tokenstream_drop_kind(toks, TK_Name));
    assert(Tokenstream_drop_text(toks, "="));
    *expr = expr_any(arena, toks);

    return (struct Statement) {
        .tag = ST_Assign,
        .assign.name = *name,
        .assign.expr = expr,
    };
}
